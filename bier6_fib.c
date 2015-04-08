/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#include "bier6.h"

#include <net/ip6_route.h>
#include <linux/byteorder/generic.h>

static struct bier6_fib_entry *bier6_fib_entry_goc(struct bier6_prefix *p,
		struct in6_addr *addr, int iif) {
	struct bier6_fib_entry *fe;
	list_for_each_entry(fe, &p->fib, le) {
		if(fe->dev == iif &&
				!memcmp(addr, &fe->addr, sizeof(*addr)))
			return fe;
	}
	if(!(fe = kmalloc(sizeof(*fe), GFP_KERNEL)))
		return NULL;
	fe->addr = *addr;
	memset(&fe->mask, 0, sizeof(fe->mask));
	fe->dev = iif;
	list_add_tail(&fe->le, &p->fib);
	return fe;
}

static void bier6_fib_prefix_flush(struct bier6 *b, struct bier6_prefix *p)
{
	struct bier6_fib_entry *fe, *fe2;
	list_for_each_entry_safe(fe, fe2, &p->fib, le) {
		list_del(&fe->le);
		kfree(fe);
	}
}

static int bier6_fib_prefix_compile(struct bier6 *b, struct bier6_prefix *p)
{
	struct bier6_rib_entry *re;
	struct rt6_info *rinfo;
	struct bier6_fib_entry *fe;
	bier6_fib_prefix_flush(b, p);
	list_for_each_entry(re, &p->rib, le) {
		rinfo = rt6_lookup(dev_net(b->netdev), &re->addr, NULL, 0, 0);
		if(rinfo == NULL) {
			printk(KERN_INFO "bier6_fib: No route to %pI6\n", &re->addr);
		} else if (!(fe = bier6_fib_entry_goc(p, &rinfo->rt6i_gateway, rinfo->rt6i_idev->dev->dev_id))) {
			printk(KERN_INFO "bier6_fib: Could not allocate memory\n");
			bier6_fib_prefix_flush(b, p);
			dst_release(&rinfo->dst); //rinfo and dst are freed this way (see rt6_lookup)
			return -ENOMEM;
		} else {
			if(re->index >= 64) {
				IN6_64(&fe->mask, 0) |= cpu_to_be64(1 << (128 - re->index));
			} else {
				IN6_64(&fe->mask, 1) |= cpu_to_be64(1 << re->index);
			}
			printk("bier6_fib: Setting %pI6%%%s mask to %pI6\n",
					&fe->addr, rinfo->rt6i_idev->dev->name, &fe->mask);
		}
	}
	return 0;
}

void bier6_rib_prefix_flush(struct bier6 *b, struct bier6_prefix *p) {
	struct bier6_rib_entry *e, *e2;
	list_for_each_entry_safe(e, e2, &p->rib, le) {
		list_del(&e->le);
		kfree(e);
	}
	bier6_fib_prefix_compile(b, p);
}

int bier6_rib_bit_set(struct bier6 *b, struct bier6_prefix *p,
		u8 index, struct in6_addr *dst)
{
	struct bier6_rib_entry *re;
	list_for_each_entry(re, &p->rib, le) {
		if(re->index == index)
			goto found;
	}
	if(!dst)
		return -ENOENT;
	if((re = kmalloc(sizeof(*re), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	re->index = index;
	re->addr = *dst;
	list_add(&re->le, &p->rib);
	goto out;
found:
	if(dst)
		return -EEXIST;
	list_del(&re->le);
	kfree(re);
out:
	bier6_fib_prefix_compile(b, p);
	return 0;
}

void bier6_rib_prefix_del(struct bier6 *b, struct bier6_prefix *p)
{
	bier6_rib_prefix_flush(b, p);
	list_del(&p->le);
	kfree(p);
}

struct bier6_prefix *bier6_rib_prefix_goc(struct bier6 *b,
		struct in6_addr *prefix, u8 plen, int create)
{
	struct bier6_prefix *p;
	list_for_each_entry(p, &b->prefixes, le) {
		if(!memcmp(prefix, &p->prefix, sizeof(*prefix)) &&
				plen == p->plen)
			return p;
	}
	if(!create || (p = kmalloc(sizeof(*p), GFP_KERNEL)) == NULL)
		return NULL;

	printk("Adding prefix %pI6C/%d in bier6 %p\n",prefix, plen, b);
	INIT_LIST_HEAD(&p->fib);
	INIT_LIST_HEAD(&p->rib);
	p->prefix = *prefix;
	p->plen = plen;
	if(plen > 64) {
		IN6_64(&p->mask, 0) = 0xffffffffffffffff;
		IN6_64(&p->mask, 1) = cpu_to_be64(0xffffffffffffffff << (128 - plen));
	} else {
		IN6_64(&p->mask, 0) = cpu_to_be64(0xffffffffffffffff << (64 - plen));
		IN6_64(&p->mask, 1) = 0;
	}
	list_add_tail(&p->le, &b->prefixes);
	return p;
}

int bier6_rib_dump(struct bier6 *b, struct seq_file *m)
{
	struct bier6_prefix *p;
	struct bier6_rib_entry *re;
	struct bier6_fib_entry *fe;

	list_for_each_entry(p, &b->prefixes, le) {
		seq_printf(m, "rib add %pI6/%d\n", &p->prefix, p->plen);
		list_for_each_entry(re, &p->rib, le) {
			seq_printf(m, "rib set %pI6/%d %d %pI6\n", &p->prefix, p->plen,
					re->index, &re->addr);
		}
	}
	seq_printf(m, "\n");
	list_for_each_entry(p, &b->prefixes, le) {
		list_for_each_entry(fe, &p->fib, le) {
			seq_printf(m, "fib %pI6/%d %pI6%%%d %pI6\n", &p->prefix, p->plen,
					&fe->addr, fe->dev, &fe->mask);
		}
	}
	return 0;
}

void bier6_rib_flush(struct bier6 *b)
{
	struct bier6_prefix *p, *p2;
	list_for_each_entry_safe(p, p2, &b->prefixes, le) {
		bier6_rib_prefix_flush(b, p);
		list_del(&p->le);
		kfree(p);
	}
}

void bier6_rib_init(struct bier6 *b)
{
	printk("Init bier6 fib\n");
	INIT_LIST_HEAD(&b->prefixes);
}

void bier6_rib_term(struct bier6 *b)
{
	printk("Terminate bier6 fib\n");
	bier6_rib_flush(b);
}

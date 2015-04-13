/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#include "bier6.h"

#include <net/ip6_route.h>
#include <net/route.h>
#include <linux/byteorder/generic.h>

static void inline getmask(struct in6_addr *mask, u8 plen)
{
	if(plen > 64) {
		IN6_64(mask, 0) = 0xffffffffffffffff;
		IN6_64(mask, 1) = cpu_to_be64(0xffffffffffffffff << (128 - plen));
	} else {
		IN6_64(mask, 0) = cpu_to_be64(0xffffffffffffffff << (64 - plen));
		IN6_64(mask, 1) = 0;
	}
}

static void bier6_fib_flush(struct bier6_dev *dev)
{
	struct bier6_fib_entry *f1, *f2;
	list_for_each_entry_safe(f1, f2, &dev->fib, le) {
		list_del(&f1->le);
		kfree(f1);
	}
}

static int bier6_fib_compile(struct bier6_dev *dev)
{
	struct bier6_rib_entry *re;
	struct bier6_fib_entry *fe;
	struct rt6_info *rinfo;
	struct in6_addr *nh;

	bier6_fib_flush(dev);
	list_for_each_entry(re, &dev->rib, le) {
		if((rinfo = rt6_lookup(dev_net(dev->netdev), &re->dst, NULL, 0, 0)) == NULL) {
			printk(KERN_INFO "bier6_fib: No route to %pI6\n", &re->dst);
			continue;
		}
		nh = (rinfo->rt6i_flags & RTF_GATEWAY)?&rinfo->rt6i_gateway:&re->dst;
		list_for_each_entry(fe, &dev->fib, le) {
			if(fe->plen == re->plen && fe->device == rinfo->dst.dev &&
					!memcmp(nh, &fe->nh, sizeof(*nh)) &&
					!((IN6_64(&fe->addr, 0) ^ IN6_64(&re->addr, 0)) & IN6_64(&fe->mask, 0)) &&
					!((IN6_64(&fe->addr, 1) ^ IN6_64(&re->addr, 1)) & IN6_64(&fe->mask, 1))) {
				goto found;
			}
		}
		if (!(fe = kmalloc(sizeof(*fe), GFP_KERNEL))) {
			printk(KERN_INFO "bier6_fib: Out of memory\n");
			ip6_rt_put(rinfo);
			bier6_fib_flush(dev);
			return -ENOMEM;
		}
		fe->plen = re->plen;
		getmask(&fe->mask, re->plen);
		fe->addr = re->addr;
		fe->nh = *nh;
		fe->device = rinfo->dst.dev;
		list_add_tail(&fe->le, &dev->fib);
		goto out;
found:
		//Note: This is valid because prefixes are the same. It may not remain correct if we change that.
		IN6_64(&fe->addr, 0) |= IN6_64(&re->addr, 0);
		IN6_64(&fe->addr, 1) |= IN6_64(&re->addr, 1);
out:
		ip6_rt_put(rinfo);
	}
	return 0;
}

int bier6_rib_set(struct bier6_dev *dev,
		struct in6_addr *addr, u8 plen, struct in6_addr *dst)
{
	struct bier6_rib_entry *re;
	list_for_each_entry(re, &dev->rib, le) {
		if(re->plen == plen &&
				!memcmp(&re->addr, addr, sizeof(*addr)))
			goto found;
	}
	if(!dst)
		return -ENOENT;
	if((re = kmalloc(sizeof(*re), GFP_KERNEL)) == NULL)
		return -ENOMEM;
	re->plen = plen;
	re->addr = *addr;
	re->dst = *dst;
	list_add_tail(&re->le, &dev->rib);
	goto out;
found:
	if(dst)
		return -EEXIST;
	list_del(&re->le);
	kfree(re);
out:
	bier6_fib_compile(dev);
	return 0;
}

int bier6_rib_dump(struct bier6 *b, struct seq_file *m)
{
	struct bier6_dev *dev;
	struct bier6_rib_entry *re;
	struct bier6_fib_entry *fe;

	list_for_each_entry(dev, &b->devices, le) {
		seq_printf(m, "dev add %s\n", dev->netdev->name);
		list_for_each_entry(re, &dev->rib, le) {
			seq_printf(m, "rib %s add %pI6/%d %pI6\n",
					dev->netdev->name, &re->addr, re->plen, &re->dst);
		}
	}

	seq_printf(m, "\n");
	list_for_each_entry(dev, &b->devices, le) {
		list_for_each_entry(fe, &dev->fib, le) {
			seq_printf(m, "fib %s %pI6/%d %pI6%%%s\n",
					dev->netdev->name, &fe->addr, fe->plen,
					&fe->nh, fe->device->name);
		}
	}
	return 0;
}

void bier6_rib_flush(struct bier6_dev *dev)
{
	struct bier6_rib_entry *re, *re2;
	list_for_each_entry_safe(re, re2, &dev->rib, le) {
		list_del(&re->le);
		kfree(re);
	}
	bier6_fib_compile(dev);
}

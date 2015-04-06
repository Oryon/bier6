/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#include "bier6.h"

#include <net/ip6_route.h>
#include <linux/byteorder/generic.h>

static void bier6_fib_raz(struct bier6_device *dev)
{
	struct bier6_neigh *n, *n2;
	list_for_each_entry_safe(n, n2, &dev->fib, le) {
		kfree(n);
	}
	INIT_LIST_HEAD(&dev->fib);
}

static struct bier6_neigh *bier6_fib_goc(
		struct bier6_device *dev,
		struct in6_addr *addr, int iif)
{
	struct bier6_neigh *n;
	list_for_each_entry(n, &dev->fib, le) {
		if(n->dev == iif &&
				!memcmp(addr, &n->addr, sizeof(*addr)))
			return n;
	}
	if(!(n = kmalloc(sizeof(*n), GFP_KERNEL)))
		return NULL;
	n->addr = *addr;
	n->bitmask = 0;
	n->dev = iif;
	list_add_tail(&n->le, &dev->fib);
	return n;
}

static void bier6_fib_compile(struct bier6_device *dev)
{
	int i;
	struct rt6_info *rinfo;
	struct bier6_neigh *n;
	bier6_fib_raz(dev);
	for(i=0; i<RIB_BITS; i++) {
		if(dev->rib[i].set) {
			rinfo = rt6_lookup(dev_net(dev->dev), &dev->rib[i].addr, NULL, 0, 0);
			if(rinfo == NULL) {
				printk(KERN_INFO "bier6_fib: No route to %pI6\n", &dev->rib[i].addr);
			} else if (!(n = bier6_fib_goc(dev, &rinfo->rt6i_gateway, rinfo->rt6i_idev->dev->dev_id))) {
				printk(KERN_INFO "bier6_fib: Could not allocate memory\n");
			} else {
				n->bitmask |= cpu_to_be32(1 << i);
				printk("bier6_fib: Setting %pI6%%%s mask to %d\n", &n->addr, rinfo->rt6i_idev->dev->name, be32_to_cpu(n->bitmask));
			}
		}
	}
}

void bier6_fib_set_bit(struct bier6_device *dev, __u8 bitindex, struct in6_addr *dst)
{
	printk(KERN_INFO"bier6_fib_set_bit: bit %d addr %p\n", bitindex, dst);
	if(dst) {
		dev->rib[bitindex].set = 1;
		dev->rib[bitindex].addr = *dst;
	} else {
		dev->rib[bitindex].set = 0;
	}
	bier6_fib_compile(dev);
}

int bier6_fib_dump(struct bier6_device *dev, struct seq_file *m)
{
	int i;
	struct bier6_neigh *n;
	for(i=0; i<RIB_BITS; i++)
		if(dev->rib[i].set)
			seq_printf(m, "bit %d %pI6\n", i, &dev->rib[i].addr);
		else
			seq_printf(m, "bit %d null\n", i);
	list_for_each_entry(n, &dev->fib, le)
		seq_printf(m, "fib %pI6%%%d %x\n", &n->addr, n->dev, be32_to_cpu(n->bitmask));
	return 0;
}

void bier6_fib_init(struct bier6_device *dev)
{
	int i;
	printk("Init bier6 fib\n");
	INIT_LIST_HEAD(&dev->fib);
	for(i=0; i<RIB_BITS; i++)
		dev->rib[i].set = 0;
}

void bier6_fib_term(struct bier6_device *dev)
{
	printk("Terminate bier6 fib\n");
	bier6_fib_raz(dev);
}

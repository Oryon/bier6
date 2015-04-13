/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */

#include <uapi/linux/in6.h>

#include <net/ip.h>
#include <net/ipv6.h>
#include <net/route.h>
#include <net/ip6_route.h>

#include <linux/inet.h>

#define DRIVER_AUTHOR "Pierre Pfister <pierre pfister@darou.fr>"
#define DRIVER_DESC   "A simple IPv6 based BIER forwarder."

#include "bier6.h"

static struct bier6 bier;

static int bier6_ipv6_output(struct bier6_dev *dev,
		struct sk_buff *skb, struct bier6_fib_entry *fe)
{
	struct flowi6 fl6;
	struct dst_entry *dst;
	struct rt6_info *rt;

	fl6 = (struct flowi6) {
		.flowi6_oif = fe->device->ifindex,
		.daddr = fe->nh,
	};

	dst = ip6_route_output(dev_net(dev->netdev), NULL, &fl6);
	if (dst->error) {
		dst_release(dst);
		return -1;
	}

	printk("skb_dev: %s dst_dev: %s\n", skb->dev->name, dst->dev->name);
	rt = container_of(dst, struct rt6_info, dst);

	skb->dev = dst->dev;
	skb_dst_drop(skb);
	skb_dst_set(skb, dst);
	dst_output(skb);
	return 0;
}

void bier6_ipv6_input(struct bier6_dev *dev, struct sk_buff *old_skb)
{
	struct in6_addr daddr = ipv6_hdr(old_skb)->daddr, daddr2;
	struct bier6_fib_entry *fe;
	struct sk_buff *skb;

	printk(KERN_ERR "bier6_ipv6_input\n");
	if (ipv6_hdr(old_skb)->hop_limit <= 1) {
		/* Force OUTPUT device used as source address */
		//old_skb->dev = dst->dev;
		icmpv6_send(old_skb, ICMPV6_TIME_EXCEED, ICMPV6_EXC_HOPLIMIT, 0);
		kfree_skb(old_skb);
		return;
	}
	ipv6_hdr(old_skb)->hop_limit--;

	list_for_each_entry(fe, &dev->fib, le) {
		//Test if destination in fib prefix
		if(((IN6_64(&daddr, 0) ^ IN6_64(&fe->addr, 0)) & IN6_64(&fe->mask, 0)) ||
				((IN6_64(&daddr, 1) ^ IN6_64(&fe->addr, 1)) & IN6_64(&fe->mask, 1)))
			continue;

		IN6_64(&daddr2, 0) = IN6_64(&daddr, 0) & IN6_64(&fe->addr, 0);
		IN6_64(&daddr2, 1) = IN6_64(&daddr, 1) & IN6_64(&fe->addr, 1);

		//Test if there are bits set to 1
		if(!(IN6_64(&daddr2, 0) & ~IN6_64(&fe->mask, 0)) &&
				!(IN6_64(&daddr2, 1) & ~IN6_64(&fe->mask, 1)))
			continue;

		if(!(skb = skb_copy(old_skb, GFP_KERNEL))) {
			printk("skb_copy failed\n");
			continue;
		}

		memcpy(&ipv6_hdr(skb)->daddr, &daddr2, sizeof(struct in6_addr));
		skb->ip_summed = CHECKSUM_PARTIAL;

		if(bier6_ipv6_output(dev, skb, fe))
			kfree_skb(skb);
	}

	kfree_skb(old_skb);
}

static int __init init_bier6(void)
{
	printk(KERN_INFO "Loading bier6 - Simple IPv6 BIER forwarder.\n");
	INIT_LIST_HEAD(&bier.devices);
	return bier6_dev_init(&bier);
}

static void __exit cleanup_bier6(void)
{
	struct bier6_dev *dev, *d2;
	printk(KERN_INFO "Cleaning-up bier6 module\n");
	list_for_each_entry_safe(dev, d2, &bier.devices, le)
		bier6_netdev_destroy(dev);

	bier6_dev_term(&bier);
}

module_init(init_bier6);
module_exit(cleanup_bier6);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

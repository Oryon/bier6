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

static struct bier6_device *bier6_dev;

void bier6_ipv6_input(struct bier6_device *dev, struct sk_buff *old_skb)
{
	struct sk_buff *skb;
	struct flowi6 fl6;
	struct dst_entry *dst;
	__be32 mask;
	struct bier6_neigh *n;

	if (ipv6_hdr(old_skb)->hop_limit <= 1) {
		/* Force OUTPUT device used as source address */
		//old_skb->dev = dst->dev;
		icmpv6_send(old_skb, ICMPV6_TIME_EXCEED, ICMPV6_EXC_HOPLIMIT, 0);
		kfree_skb(old_skb);
		return;
	}

	memset(&fl6, 0, sizeof(fl6));
	mask = ((__be32 *)&ipv6_hdr(old_skb)->daddr)[3];
	list_for_each_entry(n, &dev->fib, le) {
		if(!(mask & n->bitmask))
			continue;
		printk("Sending a packet maybe with mask %d\n", be32_to_cpu(mask));
		fl6.daddr = n->addr;
		fl6.__fl_common.flowic_iif = n->dev;

		if(!(dst = ip6_route_output(dev_net(dev->dev), NULL, &fl6)))
			continue;

		if(!(skb = skb_copy(old_skb, GFP_KERNEL))) {
			dst_release(dst);
			continue;
		}

		((__be32 *)&ipv6_hdr(skb)->daddr)[3] = (mask & n->bitmask);
		skb_dst_set(skb, dst);
		dst_output(skb);
	}

	kfree_skb(old_skb);
}

static struct in6_addr ipdst1 = { { { 0x20,0x01,0xde,0xad,0,0,0,0,0,0,0,0,0,0,0,1 } } };
static struct in6_addr ipdst2 = { { { 0x20,0x01,0xde,0xad,0,0,0,0,0,0,0,0,0,0,0,2 } } };
static struct in6_addr loopback = { { { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,1 } } };

static int __init init_bier6(void)
{
	int err;
	printk(KERN_INFO "Loading bier6 - Simple IPv6 BIER forwarder.\n");
	if((err = bier6_netdev_init(&bier6_dev)))
		return err;

	bier6_dev_init();
	bier6_fib_init(bier6_dev);

	bier6_fib_set_bit(bier6_dev, 0, &ipdst1);
	bier6_fib_set_bit(bier6_dev, 1, &ipdst2);
	bier6_fib_set_bit(bier6_dev, 2, &loopback);
	return 0;
}

static void __exit cleanup_bier6(void)
{
	printk(KERN_INFO "Cleaning-up bier6 module\n");
	bier6_fib_term(bier6_dev);
	bier6_netdev_term(bier6_dev);
	bier6_dev_term();
}

module_init(init_bier6);
module_exit(cleanup_bier6);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

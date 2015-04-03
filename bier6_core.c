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

static struct in6_addr ipdst = { { { 0x20,0x01,0xde,0xad,0,0,0,0,0,0,0,0,0,0,0,1 } } };

void bier6_ipv6_input(struct bier6_device *dev, struct sk_buff *old_skb)
{
	const struct ipv6hdr *ip6h = ipv6_hdr(old_skb);
	struct flowi6 fl6;
	struct dst_entry *dst;

	memset(&fl6, 0, sizeof(fl6));
	fl6.flowi6_proto = ip6h->nexthdr;
	fl6.saddr = ip6h->saddr;
	fl6.daddr = ipdst;

	dst = ip6_route_output(dev_net(dev->dev), NULL, &fl6);
	if (dst == NULL || dst->error) {
		dst_release(dst);
		kfree_skb(old_skb);
		return;
	}

	skb_dst_set(old_skb, dst);
	dst_output(old_skb);
}

static int __init init_bier6(void)
{
	printk(KERN_INFO "Loading bier6 - Simple IPv6 BIER forwarder.\n");
	return bier6_netdev_init();
}

static void __exit cleanup_bier6(void)
{
	printk(KERN_INFO "Cleaning-up bier6 module\n");
	bier6_netdev_term();
}

module_init(init_bier6);
module_exit(cleanup_bier6);

MODULE_LICENSE("Proprietary");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

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

/*
static int prefix_contains(struct in6_addr *p, u8 plen, struct in6_addr *addr)
{
	u8 b = plen/8;
	u8 r = plen%8;
	return !memcmp(p, addr, b) &&
			(!r || !( (((u8 *)p)[b] ^ ((u8 *)addr)[b]) & (0xff << (8 - r))));
}
*/

void bier6_ipv6_forward(struct bier6_dev *dev,
		struct bier6_prefix *p, struct in6_addr *daddr,
		struct sk_buff *old_skb)
{
	struct sk_buff *skb;
	struct bier6_fib_entry *fe;
	struct rt6_info *rinfo;
	struct in6_addr daddr2;

	struct flowi6 fl6;
	struct dst_entry *dst;

	list_for_each_entry(fe, &p->fib, le) {
		if(!(IN6_64(daddr, 0) & IN6_64(&fe->mask, 0)) &&
				!(IN6_64(daddr, 1) & IN6_64(&fe->mask, 1)))
			continue;

		printk("Sending a packet maybe with mask %pI6\n", &fe->mask);
/*
		if((rinfo = rt6_lookup(dev_net(dev->netdev), &fe->addr, NULL, fe->dev, 0)) == NULL) {
			printk("Unreachable\n");
			continue; //Unreachable
		}
		*/

		fl6.daddr = fe->addr;
		fl6.__fl_common.flowic_oif = fe->dev;

		if(!(dst = ip6_route_output(dev_net(dev->netdev), NULL, &fl6)))
			continue;

		if(!(skb = skb_copy(old_skb, GFP_KERNEL))) {
			printk("skb_copy failed\n");
			//dst_release(&rinfo->dst);
			dst_release(dst);
			continue;
		}

		IN6_64(&daddr2, 0) = IN6_64(daddr, 0) & (IN6_64(&fe->mask, 0) | IN6_64(&p->mask, 0));
		IN6_64(&daddr2, 1) = IN6_64(daddr, 1) & (IN6_64(&fe->mask, 1) | IN6_64(&p->mask, 1));

		printk("Destination set to %pI6 = %pI6 & (%pI6 | %pI6)\n", &daddr2, daddr, &fe->mask, &p->mask);
		ipv6_hdr(skb)->daddr = daddr2;
		skb->ip_summed = CHECKSUM_PARTIAL;
		//skb_dst_set(skb, &rinfo->dst);
		skb_dst_set(skb, dst);
		dst_output(skb);
	}

	kfree_skb(old_skb);
}

void bier6_ipv6_input(struct bier6_dev *dev, struct sk_buff *old_skb)
{
	struct in6_addr daddr = ipv6_hdr(old_skb)->daddr;
	struct bier6_prefix *p;

	if (ipv6_hdr(old_skb)->hop_limit <= 1) {
		/* Force OUTPUT device used as source address */
		//old_skb->dev = dst->dev;
		icmpv6_send(old_skb, ICMPV6_TIME_EXCEED, ICMPV6_EXC_HOPLIMIT, 0);
		kfree_skb(old_skb);
		return;
	}
	ipv6_hdr(old_skb)->hop_limit--;

	list_for_each_entry(p, &dev->prefixes, le) {
		printk("(%pI6 ^ %pI6) & %pI6\n", &daddr, &p->prefix, &p->mask);
		if(((IN6_64(&daddr, 0) ^ IN6_64(&p->prefix, 0)) &
				IN6_64(&p->mask, 0)) ||
			((IN6_64(&daddr, 1) ^ IN6_64(&p->prefix, 1)) &
				IN6_64(&p->mask, 1)))
			continue;

		bier6_ipv6_forward(dev, p, &daddr, old_skb);
		return;
	}

	//Dropping packet
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

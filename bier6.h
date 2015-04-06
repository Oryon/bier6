/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#ifndef BIER6_H
#define BIER6_H

#include <linux/types.h>
#include <linux/list.h>
#include <net/ipv6.h>

#include <linux/seq_file.h>

struct bier6_neigh {
	struct list_head le;
	struct in6_addr addr;
	__be32 dev;
	__be32 bitmask;
};

#define RIB_BITS 32

struct bier6_device {
	struct net_device *dev;
	struct {
		bool set;
		struct in6_addr addr;
	} rib[RIB_BITS];
	struct list_head fib;
};

int bier6_netdev_init(struct bier6_device **dev);
void bier6_netdev_term(struct bier6_device *dev);

int bier6_dev_init(struct bier6_device *dev);
void bier6_dev_term(void);

void bier6_ipv6_input(struct bier6_device *dev, struct sk_buff *old_skb);

void bier6_fib_init(struct bier6_device *dev);
void bier6_fib_term(struct bier6_device *dev);

void bier6_fib_set_bit(struct bier6_device *dev, __u8 bitindex, struct in6_addr *dst);
int bier6_fib_dump(struct bier6_device *dev, struct seq_file *m);

#endif //BIER6_H

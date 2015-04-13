/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#ifndef BIER6_H
#define BIER6_H

#include <uapi/asm-generic/errno-base.h>
#include <linux/cdev.h>
#include <linux/inet.h>
#include <linux/list.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <net/ipv6.h>

#define IN6_64(p, i) ((u64 *)(p))[i]

struct bier6_fib_entry {
	struct list_head le;
	struct in6_addr addr;      //Prefix and bits
	struct in6_addr mask;      //1's on prefix part
	u8 plen;
	struct in6_addr nh;        //Next hop address
	struct net_device *device; //Network device
};

struct bier6_rib_entry {
	struct list_head le;
	struct in6_addr addr; //Prefix and bits
	u8 plen;              //Prefix length
	struct in6_addr dst;  //Owner of the bits
};

struct bier6_dev {
	struct list_head le;
	struct bier6 *bier;
	struct net_device *netdev;
	struct list_head rib;
	struct list_head fib;
};

struct bier6 {
	/* Network devices */
	struct list_head devices;

	/* Character device */
	dev_t devno;
	struct class *class;
	struct device *chardev;
	struct cdev cdev;
};

int bier6_netdev_goc(struct bier6 *b, char *devname,
		int create, struct bier6_dev **dev);
void bier6_netdev_destroy(struct bier6_dev *dev);

int bier6_dev_init(struct bier6 *b);
void bier6_dev_term(struct bier6 *b);

void bier6_ipv6_input(struct bier6_dev *dev, struct sk_buff *skb);

void bier6_rib_flush(struct bier6_dev *dev);
int bier6_rib_set(struct bier6_dev *dev,
		struct in6_addr *addr, u8 plen, struct in6_addr *dst);

int bier6_rib_dump(struct bier6 *b, struct seq_file *m);

#endif //BIER6_H

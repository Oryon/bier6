/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#include <linux/netdevice.h>
#include <linux/if_arp.h>

#include <uapi/linux/if.h>
#include <uapi/asm-generic/errno-base.h>

#include "bier6.h"

#define NETDEV        "bier6"

#define bier6_netdev_priv(dev) (*((struct bier6_dev **)netdev_priv(dev)))

static int bier6_netdev_up(struct net_device *dev)
{
	printk("bier6: Device %s is going up.\n", dev->name);
	netif_start_queue(dev);
	return 0;
}

static int bier6_netdev_down(struct net_device *dev)
{
	printk("bier6: Device %s is going down.\n", dev->name);
	netif_stop_queue(dev);
	return 0;
}

static netdev_tx_t bier6_netdev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	printk(KERN_INFO "bier6_netdev_xmit\n");

	if(ntohs(skb->protocol) != ETH_P_IPV6)
		goto out;

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	bier6_ipv6_input(bier6_netdev_priv(dev), skb);

out:
	return NETDEV_TX_OK;
}

static const struct net_device_ops bier6_netdev_ops = {
//	.ndo_init	= ,	// Called at register_netdev
//	.ndo_uninit	= ,	// Called at unregister_netdev
	.ndo_open	= bier6_netdev_up,	// Called at ifconfig nat64 up
	.ndo_stop	= bier6_netdev_down,	// Called at ifconfig nat64 down
	.ndo_start_xmit	= bier6_netdev_xmit,	// REQUIRED, must return NETDEV_TX_OK
//	.ndo_change_rx_flags = ,	// Called when setting promisc or multicast flags.
//	.ndo_change_mtu = ,
//	.net_device_stats = ,	// Called for usage statictics, if NULL dev->stats will be used.
};

void bier6_netdev_setup(struct net_device *dev)
{
	dev->netdev_ops = &bier6_netdev_ops;
//	dev->destructor = nat64_netdev_free;
	dev->type = ARPHRD_NONE;
	dev->hard_header_len = 0;
	dev->addr_len = 0;
	dev->mtu = ETH_DATA_LEN;
	dev->features = 0;
	//dev->features = NETIF_F_NETNS_LOCAL;
// | NETIF_F_NO_CSUM;
	dev->flags = IFF_NOARP | IFF_POINTOPOINT;
}

static int bier6_netdev_ctrl(struct bier6 *b, char *devname,
		int create, struct bier6_dev **dev, int term)
{
	int err = 0;
	if(term)
		goto term;

	list_for_each_entry((*dev), &b->devices, le)
		if(!strcmp(devname, (*dev)->netdev->name))
			return 0;

	if(!create)
		return -ENODEV;

	if((*dev = kmalloc(sizeof(**dev), GFP_KERNEL)) == NULL)
		return -ENOMEM;

	if (((*dev)->netdev = alloc_netdev(sizeof(*dev),
			devname, bier6_netdev_setup)) == NULL) {
		err = -ENOMEM;
		goto err_alloc;
	}

	if((err = register_netdev((*dev)->netdev)))
		goto err_register;

	(*dev)->bier = b;
	*((struct bier6_dev **)netdev_priv((*dev)->netdev)) = *dev;
	list_add(&(*dev)->le, &b->devices);
	INIT_LIST_HEAD(&(*dev)->prefixes);
	return 0;

term:
	bier6_rib_flush(*dev);
	list_del(&(*dev)->le);
	unregister_netdev((*dev)->netdev);
err_register:
	free_netdev((*dev)->netdev);
err_alloc:
	kfree(*dev);
err_kmalloc:
	return err;
}

int bier6_netdev_goc(struct bier6 *b, char *devname, int create, struct bier6_dev **dev)
{
	return bier6_netdev_ctrl(b, devname, create, dev, 0);
}

void bier6_netdev_destroy(struct bier6_dev *dev)
{
	bier6_netdev_ctrl(dev->bier, NULL, 0, &dev, 1);
}

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

static int bier6_netdev_up(struct net_device *dev)
{
	printk("bier6: Device is going up.\n");
	netif_start_queue(dev);
	return 0;
}

static int bier6_netdev_down(struct net_device *dev)
{
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
	bier6_ipv6_input(netdev_priv(dev), skb);

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
	dev->features = NETIF_F_NETNS_LOCAL;
// | NETIF_F_NO_CSUM;
	dev->flags = IFF_NOARP | IFF_POINTOPOINT;
}

int bier6_netdev_init(struct bier6_device **dev)
{
	struct net_device *netdev;
	int err = -ENOMEM;
	netdev = alloc_netdev(sizeof(struct bier6_device), NETDEV, bier6_netdev_setup);
	if (netdev == NULL)
		goto out;

	*dev = netdev_priv(netdev);
	(*dev)->dev = netdev;

	if((err = register_netdev(netdev)))
		goto out_reg;

	return 0;
out_reg:
	free_netdev(netdev);
out:
	return err;
}

void bier6_netdev_term(struct bier6_device *dev)
{
	unregister_netdev(dev->dev);
	free_netdev(dev->dev);
}



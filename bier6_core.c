/*
 * Author: Pierre Pfister <pierre pfister@darou.fr>
 *
 * Copyright (c) 2014 Cisco Systems, Inc.
 *
 */

#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */
#include <linux/netdevice.h>
#include <linux/if_arp.h>

#include <uapi/linux/if.h>
#include <uapi/asm-generic/errno-base.h>

#define DRIVER_AUTHOR "Pierre Pfister <pierre pfister@darou.fr>"
#define DRIVER_DESC   "A simple IPv6 based BIER forwarder."
#define NETDEV        "bier6"

struct bier6_device {
	int tx_count;
	int rx_count;
} *bier6_device;

static struct net_device *bier6_dev;

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

void bier6_ipv6_input(struct sk_buff *old_skb)
{

}

static netdev_tx_t bier6_netdev_xmit(struct sk_buff *skb, struct net_device *dev)
{
	printk(KERN_INFO "bier6_netdev_xmit\n");

	if(ntohs(skb->protocol) != ETH_P_IPV6)
		goto out;

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	bier6_ipv6_input(skb);

out:
	kfree_skb(skb);
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

static int __init init_bier6(void)
{
	int err = -ENOMEM;

	printk(KERN_INFO "Loading bier6 - Simple IPv6 BIER forwarder.\n");
	bier6_dev = alloc_netdev(sizeof(struct bier6_device), NETDEV, bier6_netdev_setup);
	if (bier6_dev == NULL)
		goto out;

	bier6_device = netdev_priv(bier6_dev);

	if((err = register_netdev(bier6_dev)))
		goto out_reg;

	return 0;
out_reg:
	free_netdev(bier6_dev);
out:
	return err;
}

static void __exit cleanup_bier6(void)
{
	printk(KERN_INFO "Cleaning-up bier6 module\n");
	unregister_netdev(bier6_dev);
	free_netdev(bier6_dev);
}

module_init(init_bier6);
module_exit(cleanup_bier6);

MODULE_LICENSE("Proprietary");

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);

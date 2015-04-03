

#ifndef BIER6_H
#define BIER6_H

struct bier6_device {
	struct net_device *dev;
};

int bier6_netdev_init(void);
void bier6_netdev_term(void);

void bier6_ipv6_input(struct bier6_device *dev, struct sk_buff *old_skb);

#endif //BIER6_H

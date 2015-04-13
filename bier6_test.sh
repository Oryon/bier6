#!/bin/sh
#
# Test scripts for bier6
#

cd `dirname $0`

if [ "$1" = "up" ]; then
	make
	sudo insmod bier6.ko
	sudo chmod a+rw /dev/bier6
	
	sudo ip netns add out
	sudo ip link add veth0 type veth peer name veth1
	sudo ip link set veth1 netns out

	sudo sysctl net.ipv6.conf.all.forwarding=1
	sudo sysctl net.ipv6.conf.default.forwarding=1
	sudo ip netns exec out sysctl net.ipv6.conf.all.forwarding=1
	sudo ip netns exec out sysctl net.ipv6.conf.default.forwarding=1

	#Configure veth0
	sudo ip link set veth0 up
	sudo ip -6 addr add 2001:dead:3::2/64 dev veth0
	#sudo ip -6 route add default via fe80::13 dev veth0
	sudo ip -6 route add default via 2001:dead:3::1 dev veth0
	#sudo ip -6 route add default via fe80::13 dev eth0
	#sudo ip -6 route add default via fe80::5:73ff:fea0:3e9 dev eth1

	#Configure veth1
	sudo ip netns exec out ip link set veth1 up
	sudo ip netns exec out ip link set lo up
	sudo ip netns exec out ip -6 addr add 2001:dead:3::1/64 dev veth1
	#sudo ip netns exec out ip -6 addr add 2001:f00d::2/128

	#sudo ip netns exec out sudo ip tuntap add tun0 mode tun
	#sudo ip netns exec out ip -6 route add 2001:f00d::2/128 dev tun0

	#Configure b6.0
	echo "dev add b6.0" > /dev/bier6
	sudo ip link set b6.0 up
	sudo ip -6 route add 2001:f00d::/64 dev b6.0

	#Configure b6.1
	echo "dev add b6.1" > /dev/bier6
	sudo ip link set b6.1 netns out
	sudo ip netns exec out ip link set b6.1 up
	sudo ip netns exec out ip -6 route add 2001:f00d::/64 dev b6.1

	#Configure b6.0 BIER forwarding
	echo "rib b6.0 add 2001:f00d::1/64 ::1" > /dev/bier6
	echo "rib b6.0 add 2001:f00d::2/64 2001:dead:3::1" > /dev/bier6
	echo "rib b6.0 add 2001:f00d::4/64 2001:dead:ffff::1" > /dev/bier6

	#Configure b6.1 BIER forwarding
	echo "rib b6.1 add 2001:f00d::1/64 ::1" > /dev/bier6
	echo "rib b6.1 add 2001:f00d::2/64 2001:dead:3::1" > /dev/bier6
	echo "rib b6.1 add 2001:f00d::4/64 2001:dead:ffff::1" > /dev/bier6

	echo "---------------------------------------"
	cat /dev/bier6
elif [ "$1" = "down" ]; then
	sudo rmmod bier6
	sudo ip link del veth0
	sudo ip netns exec out ip link set lo down
	sudo ip netns exec out ip link del veth1
	sudo ip netns del out
fi

if [ "$1" = "up2" ]; then
	make
	sudo insmod bier6.ko
	sudo chmod a+rw /dev/bier6
	sudo sysctl net.ipv6.conf.all.forwarding=1
	sudo sysctl net.ipv6.conf.default.forwarding=1
	sudo ip -6 addr add 2001:dead:3::2/64 dev br0
	echo "dev add bier6" > /dev/bier6
	sudo ip link set bier6 up
	sudo ip -6 route add 2001:f00d::/64 dev bier6
	echo "rib bier6 add 2001:f00d::1/64 ::1" > /dev/bier6
	echo "rib bier6 add 2001:f00d::2/64 2001:dead:3::1" > /dev/bier6
	echo "rib bier6 add 2001:f00d::4/64 2001:dead:ffff::1" > /dev/bier6
	echo "rib bier6 add 2001:f00d::8/64 2001:dead:3::2" > /dev/bier6
elif [ "$1" = "down2" ]; then
	sudo rmmod bier6
	sudo ip -6 addr del 2001:dead:3::2/64 dev br0
fi


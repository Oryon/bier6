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

	sudo ip link set veth0 up
	sudo ip -6 addr add 2001:dead:3::2/64 dev veth0
	sudo ip -6 route add default via 2001:dead:3::1 dev veth0

	sudo ip netns exec out ip link set lo up
	sudo ip netns exec out ip link set veth1 up
	sudo ip netns exec out ip -6 addr add 2001:dead:3::1/64 dev veth1
	sudo ip netns exec out ip -6 addr add 2001:f00d::2/128 dev lo

	echo "rib add 2001:f00d::/64" > /dev/bier6
	echo "rib set 2001:f00d::/64 0 ::1" > /dev/bier6
	echo "rib set 2001:f00d::/64 1 2001:dead:3::1" > /dev/bier6
	echo "rib set 2001:f00d::/64 2 2001:dead:ffff::1" > /dev/bier6

	echo "---------------------------------------"
	cat /dev/bier6
elif [ "$1" = "down" ]; then
	sudo rmmod bier6
	sudo ip netns del out
	sudo ip link del veth0
fi

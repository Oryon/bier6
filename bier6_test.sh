#!/bin/sh
#
# Test scripts for bier6
#

cd `dirname $0`




make || exit 1
sudo rmmod bier6
sudo insmod bier6.ko || exit 1 
sudo chmod a+rw /dev/bier6

sudo ip link set bier6 up || exit 1
sudo ip -6 route add 2001:f00d::/64 dev bier6
sudo ip -6 route add 2001:dead:1::/64 dev eth0 via fe80::1:12
sudo ip -6 route add 2001:dead:2::/64 dev eth0 via fe80::2:12

echo "rib add 2001:f00d::/64" > /dev/bier6
echo "rib set 2001:f00d::/64 0 ::1" > /dev/bier6
echo "rib set 2001:f00d::/64 1 2001:dead:1::1" > /dev/bier6
echo "rib set 2001:f00d::/64 2 2001:dead:2::1" > /dev/bier6

sleep 1

ping6 2001:f00d::7

sudo rmmod bier6
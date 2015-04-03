#!/bin/sh
#
# Test scripts for bier6
#

cd `dirname $0`




make || exit 1
sudo rmmod bier6
sudo insmod bier6.ko || exit 1 

sudo ip link set bier6 up || exit 1
sudo ip -6 route add 2001:f00d::/64 dev bier6
sudo ip -6 route add 2001:dead::/64 dev eth0 via fe80::12

ping6 2001:f00d::7

sudo rmmod bier6
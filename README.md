=== BIER6 Linux Kernel Module === 

bier6 is a Linux kernel module implementing BIER forwarding over IPv6.

The module creates a pseudo-device /dev/bier6 used for configuration.
You can write to the device in order to configure it, and read its content
in order to see the current configuration.

Examples:
  echo "dev add b6.0" > /dev/bier6
  echo "rib b6.0 add 2001:db8:1::1/64 2001::db8:ffff::2"
  cat /dev/bier6

bier6.sh is a helper program to perform the same operations.

Examples:
  bier6.sh help
  bier6.sh dev add b6.0
  bier6.sh rib b6.0 add 2001:db8:1::1/64 2001::db8:ffff::2
  bier6.sh show

The module allows creating an arbitrary number of BIER virtual network devices.
Each network device can be configured with a set of BIER routing entries.

Note: 
  You have to configure the linux routing table in order to forward the packets
  you care about toward a bier device. This is not handled by the device itself.

Bier routing entries are composed of a prefix and a final destination.
Most significant bits from the prefix are used for data packet destination
address matching. Least significant bits are used for storing the BIER bits.

For instance, if the configuraiton is:
rib b6.0 add 2001:db8::1/64 2001:db8:1::1
rib b6.0 add 2001:db8::2/64 2001:db8:2::1

If the device b6.0 receives a data packet which destination is included
in 2001:db8::/64, it will look at the least significant bits and forward 
based on BIER logic.

A packet sent to 2001:db8::1 will be forwarded toward 2001:db8:1::1.
A packet sent to 2001:db8::2 will be forwarded toward 2001:db8:2::1.
A packet sent to 2001:db8::3 will either be split into two packets and
sent seperately, or sent together if both destinations share the same
next-hop.




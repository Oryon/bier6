=== BIER6 Linux Kernel Module === 

bier6 is a Linux kernel module implementing BIER forwarding over IPv6.

The BIER network makes use of one or multiple IPv6 prefixes assigned to
BIER forwarding. Packets which destination address is included in one of 
these prefixes is forwarded based on the BIER scheme.

BIER bits are encoded as part of the destination address, using the lowest
significant bits. Consequently, the destination address is modified as the
packet travels through the BIER network.

For instance, if the prefix 2001:db8:1:1::/64 is used for BIER forwarding,
all BIER egress routers will be assigned bit indexes from 0 to 63. 
 - A packet sent to 2001:db8:1:1::1 will be forwarded toward the owner of the 
address associated with the 0th bit.
 - A packet sent to 2001:db8:1:1::1:1 will be forwarded toward the owner of the
address associated with the 0th bit, and the owner of the address associated with
the 16th bit.



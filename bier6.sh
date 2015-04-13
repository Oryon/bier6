#!/bin/sh
#
# Script utility for bier6 functions.
#

usage () {
	cat << EOF
bier6 show
	Dump bier6 configuration.

bier6 dev [add|del] <device>
	Create or destroy a bier device.

bier6 rib <device> [add|del] <prefix> [<destination>]
	Create or destroy a bier routing entry. Most significant
	prefix bits are used for data packet matching. Less
	significant prefix bits are the bier bits, used for bier
	forwarding.

bier6 help
	Prints this message.
EOF
	exit $1
}

if [ "$1" = "" -o "$1" = "help" ]; then
	usage 0
fi

module=`lsmod | grep bier6`
if [ "$module" = "" ]; then
	echo "bier6 module is not loaded (Do sudo modprobe bier6)."
fi

sudo chmod ug+rw /dev/bier6 || exit 1

case "$1" in
	"show")
		sudo cat /dev/bier6
		;;
	"rib"|"dev")
		sudo echo "$@" > /dev/bier6
		;;
	*)
		usage 1
esac

exit $?




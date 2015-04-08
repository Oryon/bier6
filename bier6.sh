#!/bin/sh
#
# Script utility for bier6 functions.
#

usage () {
	cat << EOF
bier6 show
	Dump bier6 configuration.

bier6 rib <device> [add|del] <prefix>
	Adds or remove a rib entry for the given prefix.

bier6 rib <device> set <prefix> <n> [<address>|null]
	Set or unset the n'th address used for bier-based 
	forwarding. Bit count starts from lowest significance.
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




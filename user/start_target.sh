#!/bin/sh

printusage()
{
	echo
	echo "$0 id=<devid> -files <lunfile1,lunfile2...> [-v]"
	echo
	echo "id=<devid>        : The id number of the SCSI HBA instance. This can be any number between 0 and 255."
	echo
	echo "-files <lunfiles> : The name of the lun files seperated by comma."
	echo "                    The size of lun file should in multiple of 2048 blocks(512 bytes)."
	echo "                    Use 'dd if=/dev/zero of=<filename> count=<size of lun in 512 byte blocks>' to create file."
	echo "                    These file holds the disk image."
	echo
	echo "-v                : Verbose mode of the scsi target user process."
	echo
	echo

}
TARGET_NAME="./vscsitarget"
MODULE_FILE="../kernel/vscsihba.ko"

/sbin/modprobe scsi_mod >/dev/null 2>&1
/sbin/insmod $MODULE_FILE >/dev/null 2>&1

major=`cat /proc/devices |grep vscsihba |cut -f1 -d" "`
minor=$1
shift;

id=`echo $minor | cut -f1 -d=`

if [ "$id" != "id" ]
then
	printusage
	exit
fi

minor=`echo $minor | cut -f2 -d=`

if [ -z "$minor"  ]
then
	printusage
	exit
fi


if [ -z "$major" ]
then
	echo "vscsihba module not loaded"
	exit
fi


devname="vscsihbadev.$minor"

rm -f $devname >/dev/null 2>&1
mknod $devname c $major $minor >/dev/null 2>&1
if [ $? -ne 0 ]
then
	printusage
	exit
fi

$TARGET_NAME -dev $devname $*
if [ $? -eq 100 ]
then
	printusage
fi

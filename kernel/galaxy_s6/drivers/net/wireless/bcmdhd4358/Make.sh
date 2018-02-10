#!/bin/bash


###############
# help message
###############
usage() {
	echo "./Make.sh -[option] [arg]"
	echo
	echo "Example)"
	echo "  Default build is Panda platform"
	echo "     $ ./Make.sh  "
	echo "  -p) Build for Panda platform"
	echo "     $ ./Make.sh -p panda  "
	echo "  -p) Build for M0 platform"
	echo "     $ ./Make.sh -p M0 "
	echo "  -p) Build for D2 platform"
	echo "     $ ./Make.sh -p D2 "
	echo
	echo "  -b) Specific build brand "
	echo "     $ ./Make.sh -p M0 -b dhd-cdc-sdmmc-android-panda-icsmr1-cfg80211-oob-customer_hw4"
	echo "  -s) Specific SDK location(Kernel source location) "
	echo "     $ ./Make.sh -p M0 -s /tools/linux/src/linux-3.0.15-m0-0502"
	echo "  -f) flat directory compile "
	echo "     $ ./Make.sh -f -p M0"
	echo "  -f) flat directory compile "
	echo "     $ ./Make.sh -f -p M0 -s /tools/linux/src/linux-3.0.31-m0-JB-120824"
	echo "  -t) Specific Toolchain location "
	echo "     $ ./Make.sh -p M0 -t /projects/hnd/tools/linux/hndtools-arm-eabi-4.4.3/bin"
	echo ""
	echo "  all new platform example)"
	echo "     $ ./Make.sh -p NewGPhone -t /projects/hnd/tools/linux/hndtools-arm-eabi-4.4.3/bin "
	echo "                 -s /tools/linux/src/linux-4.5.6-NewGPhone-120716"
	echo "                 -b dhd-cdc-sdmmc-android-newgphone-keylimepie-cfg80211-oob-customer_hw4"
	echo ""
	echo "  clean) Clean up"
	echo "     $ ./Make.sh clean "
	echo
}


###################
# Script Arguments
###################
while getopts 'b:dhs:t:p:f' opt
do

	case $opt in
		# Specific Build brand
		b)
			PRIVATE_BUILD_BRAND=$OPTARG
			;;
		# Debug Build brand
		d)
			EXTRA_BUILD_BRAND="-debug"
			;;
		# Help message
		h)
			usage
			exit;
			;;
		# SDK kernel source specific location
		s)
			LINUXDIR=$OPTARG
			;;
		# Toolchain specific location
		t)
			PRIVATE_TOOLCHAIN_PATH=$OPTARG
			;;
		# Target Platform
		p)
			PLATFORM=$OPTARG
			;;
		# After Mogrified source (flat directory type)
		f)
			FLAT_TYPE=y
			;;
	esac
done

shift $(($OPTIND - 1))
REMAIN=$1

###################
# Default Variable
###################
if [ "$FLAT_TYPE" = "y" ];then
	# for FLAT_TYPE (after mogrified source)
	MAKE_PATH=""
else
	# for directory type (pure svn checkout source)
	MAKE_PATH="-C src/dhd/linux"
fi
CROSS_COMPILE=arm-eabi-
ARCH=arm
CPU_JOBS=`cat /proc/cpuinfo | grep processor | wc -l`

###########
# Clean up
###########
if [ "$1" = "clean" ]; then
	echo "make $MAKE_PATH clean"
	make $MAKE_PATH clean --no-print-directory QUIET_BUILD=1
	exit;
fi


##########################################
# Set Default Variable depend on Platform
##########################################
case $PLATFORM in
	panda|PANDA)
		PLATFORM=panda
		BUILD_BRAND=dhd-cdc-sdmmc-android-panda-jellybean-cfg80211-oob-customer_hw4
		TOOLCHAIN_PATH=/projects/hnd/tools/linux/hndtools-arm-eabi-4.4.0/bin
		LINUXVER=3.0.8-panda
		;;

	D2|d2)
		BUILD_BRAND=dhd-cdc-sdmmc-android-m0-jellybean-cfg80211-oob-customer_hw4
		TOOLCHAIN_PATH=/projects/hnd/tools/linux/hndtools-arm-eabi-4.4.3/bin
		LINUXVER=3.0.31-d2-JB-120911
		;;

	T0|t0)
		BUILD_BRAND=dhd-cdc-sdmmc-android-m0-jellybean-cfg80211-oob-customer_hw4
		TOOLCHAIN_PATH=/projects/hnd/tools/linux/hndtools-arm-eabi-4.4.3/bin
		LINUXVER=3.0.31-T0-JB-120904
		;;

	*|M0|m0)
		BUILD_BRAND=dhd-cdc-sdmmc-android-m0-jellybean-cfg80211-oob-customer_hw4
		TOOLCHAIN_PATH=/projects/hnd/tools/linux/hndtools-arm-eabi-4.4.3/bin
		LINUXVER=3.0.31-m0-JB-120824
		;;
esac

##############################
# build_brand by user command
##############################
if [ ! "$PRIVATE_BUILD_BRAND" = "" ]; then
	BUILD_BRAND=$PRIVATE_BUILD_BRAND
fi

# extra build brand (-debug)
BUILD_BRAND=$BUILD_BRAND$EXTRA_BUILD_BRAND


# user toolchain
if [ ! "$PRIVATE_TOOLCHAIN_PATH" = "" ]; then
	TOOLCHAIN_PATH=$PRIVATE_TOOLCHAIN_PATH
fi

if [ ! -d $TOOLCHAIN_PATH ]; then
    echo "toolchain path($TOOLCHAIN_PATH) is not exist!! please check it";
    exit;
fi


#######################
# Setup Make arguments
#######################
MAKE_ARGS+=" CROSS_COMPILE=$CROSS_COMPILE"
MAKE_ARGS+=" ARCH=$ARCH"
MAKE_ARGS+=" PATH=$PATH:$TOOLCHAIN_PATH"
MAKE_ARGS+=" OEM_ANDROID=1"

if [ "$FLAT_TYPE" = "y" ];then
	# for FLAT_TYPE (after mogrified source)
	if [ ! $LINUXDIR = "" ];then
		MAKE_ARGS+=" KDIR=$LINUXDIR"
	else
		MAKE_ARGS+=" KDIR=/tools/linux/src/linux-$LINUXVER"
	fi
	BUILD_BRAND=""
else
	# for directory type (pure svn checkout source)
	if [ ! $LINUXDIR = "" ];then
		MAKE_ARGS+=" LINUXDIR=$LINUXDIR"
	else
		MAKE_ARGS+=" LINUXVER=$LINUXVER"
	fi
fi

MAKE_ARGS+=" -j$CPU_JOBS"


####################
# Build Information
####################
echo "====================================================="
echo "PLATFORM       : $PLATFORM"
echo "BUILD_BRAND    : $BUILD_BRAND"
if [ ! $LINUXDIR = "" ];then
	echo "LINUXDIR       : $LINUXDIR"
else
	echo "LINUXVER       : $LINUXVER"
fi
echo
echo "ARCH           : $ARCH"
echo "CROSS_COMPILE  : $CROSS_COMPILE"
echo "TOOLCHAIN_PATH : $TOOLCHAIN_PATH"
echo "MAKE_PATH      : $MAKE_PATH"
echo "PATH           : $PATH:$TOOLCHAIN_PATH"
echo "====================================================="
echo
#echo "====================================================="
#echo "$ make $MAKE_PATH $MAKE_ARGS $BUILD_BRAND"
#echo "====================================================="
#echo ""


###########################
# epivers.h check & create
##########################
if [ "$FLAT_TYPE" = "" ];then
	if [ ! -e src/include/epivers.h ];then
		echo "-----------------------------------"
		echo "epivers.h create for version header"
		cd src/include && make && cd ../../
		echo "-----------------------------------"
	fi
fi


#

####################################
# check WIFI_BUILD_ONLY in Makefile. if not exist then add three line at top of Makefile
####################################
if [ "$FLAT_TYPE" = "y" ];then               
	grep "WIFI_BUILD_ONLY" Makefile > /dev/null
	if [ $? -ne 0 ];then
	    	mv Makefile Makefile.original
    		echo "# Three line added by Make.sh, original Makefile backup to Makefile.original                   
ifeq (\$(WIFI_BUILD_ONLY),y)                                                                             
	include \$(KDIR)/.config                                                                             
endif                                                                                                    
" > Makefile
		cat Makefile.original >> Makefile
	fi
fi


####################
# Call Sub Makefile
####################
make $MAKE_PATH $MAKE_ARGS $BUILD_BRAND --no-print-directory QUIET_BUILD=1 WIFI_BUILD_ONLY=y

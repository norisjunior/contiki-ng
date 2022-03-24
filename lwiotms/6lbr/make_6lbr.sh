#!/bin/bash

echo "############################################################"
echo "###                   6LBR compilation                   ###"
echo "############################################################"

if [[ $# -eq 0 ]] ; then
    echo 'usage:'
    echo '- hardware platform supported: LAUNCHPAD or FIREFLY'
    echo 'example: make_6lbr.sh LAUNCHPAD'
    echo '         make_6lbr.sh FIREFLY'
    echo ''
    exit 0
fi

make distclean

if [ "$1" == "LAUNCHPAD" ] ; then
  make TARGET=cc26x0-cc13x0 BOARD=launchpad/cc2650 PORT=/dev/ttyACM0 border-router.upload
elif [ "$1" == "FIREFLY" ] ; then
  make TARGET=zoul BOARD=firefly PORT=/dev/ttyUSB0 border-router.upload
fi

#!/bin/bash

echo "############################################################"
echo "###                   6LBR compilation                   ###"
echo "############################################################"

if [[ $# -eq 0 ]] ; then
    echo 'usage:'
    echo '- hardware platform supported: LAUNCHPAD or REMOTE'
    echo 'example: make_app.sh SENSORTAG'
    echo '         make_app.sh REMOTE'
    echo ''
    exit 0
fi

make distclean

if [ "$1" == "SENSORTAG" ] ; then
  make lwpubsub-fedsensor-lwaiot TARGET=cc26x0-cc13x0 BOARD=sensortag/cc2650 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-lwaiot
elif [ "$1" == "REMOTE" ] ; then
  make lwpubsub-fedsensor-lwaiot TARGET=zoul BOARD=remote-revb PORT=/dev/ttyUSB1 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-lwaiot.upload
fi

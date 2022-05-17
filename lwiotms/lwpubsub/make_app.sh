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
  make lwpubsub-fedsensor-ml_and_msg TARGET=cc26x0-cc13x0 BOARD=sensortag/cc2650 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-ml_and_msg
elif [ "$1" == "REMOTE" ] ; then
  make lwpubsub-fedsensor-ml_and_msg TARGET=zoul BOARD=remote-revb PORT=/dev/ttyUSB1 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-ml_and_msg.upload
elif [ "$1" == "NATIVE" ] ; then
  make lwpubsub-fedsensor-ml_and_msg TARGET=native MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=0 lwpubsub-fedsensor-ml_and_msg
elif [ "$1" == "CC1352P1" ] ; then
  make lwpubsub-fedsensor-ml_and_msg TARGET=simplelink BOARD=launchpad/cc1352p1 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-ml_and_msg
fi

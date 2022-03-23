#!/bin/bash

echo "############################################################"
echo "###                   6LBR compilation                   ###"
echo "############################################################"

if [[ $# -eq 0 ]] ; then
    echo 'usage:'
    echo '- hardware platform supported: LAUNCHPAD or FIREFLY'
    echo '- TSCH schedule: MINIMAL or ORCHESTRA'
    echo 'example: make_6lbr.sh LAUNCHPAD MINIMAL'
    echo '         make_6lbr.sh FIREFLY ORCHESTRA'
    echo ''
    exit 0
fi

if   [ "$1" != 'LAUNCHPAD' ] && [ "$1" != 'FIREFLY' ] ; then
  echo 'error: only LAUNCHPAD or FIREFLY supported platforms'
  exit 0
fi

if   [ "$2" != 'MINIMAL' ] && [ "$2" != 'ORCHESTRA' ] ; then
  echo 'error: only MINIMAL or ORCHESTRA allowed TSCH schedules'
  exit 0
fi

if   [ "$2" == 'ORCHESTRA' ] ; then
  is_orchestra=1
fi


#dir_binaries=~/Google\ Drive/Doutorado/#Artigos/LWPubSub\ for\ IoT/Experiments/binaries

make distclean

if [ "$1" == "LAUNCHPAD" ] ; then
  make TARGET=cc26x0-cc13x0 BOARD=launchpad/cc2650 MAKE_WITHSCHEDORCHESTRA=$is_orchestra
elif [ "$1" == "FIREFLY" ] ; then
  make TARGET=zoul BOARD=firefly PORT=/dev/ttyUSB0 MAKE_WITHSCHEDORCHESTRA=$is_orchestra border-router.upload
fi

#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo "############################################################"
    echo "###                      6LBR start                      ###"
    echo "############################################################"
    echo 'usage:'
    echo '- hardware platform supported: LAUNCHPAD or FIREFLY'
    echo 'example: start_6lbr.sh LAUNCHPAD'
    echo '         start_6lbr.sh FIREFLY'
    echo ''
    exit 0
fi

if   [ "$1" != 'LAUNCHPAD' ] && [ "$1" != 'FIREFLY' ] ; then
  echo 'error: only LAUNCHPAD or FIREFLY supported platforms'
  exit 0
fi

#sudo ../../tools/serial-io/tunslip6 -L -v -s /dev/ttyACM0 fd00::1/64

if [ "$1" == "LAUNCHPAD" ] ; then
  sudo ../../tools/serial-io/tunslip6 -L -v -s /dev/ttyACM0 fd00::1/64
elif [ "$1" == "FIREFLY" ] ; then
  sudo ../../tools/serial-io/tunslip6 -L -v -s /dev/ttyUSB0 fd00::1/64
fi

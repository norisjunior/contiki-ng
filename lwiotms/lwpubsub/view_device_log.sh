#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo "############################################################"
    echo "###                      6LBR start                      ###"
    echo "############################################################"
    echo 'usage:'
    echo '- hardware platform supported: LAUNCHPAD or FIREFLY'
    echo 'example: view_device_log.sh SENSORTAG'
    echo '         view_device_log.sh REMOTE'
    echo ''
    exit 0
fi

if   [ "$1" != 'SENSORTAG' ] && [ "$1" != 'REMOTE' ] && [ "$1" != 'CC1352P1' ] ; then
  echo 'error: only SENSORTAG, REMOTE or CC1352P1 supported platforms'
  exit 0
fi

#sudo ../../tools/serial-io/tunslip6 -L -v -s /dev/ttyACM0 fd00::1/64

if [ "$1" == "SENSORTAG" ] || [ "$1" == "CC1352P1" ] ; then
  sudo ../../tools/serial-io/serialdump -s /dev/ttyACM0
elif [ "$1" == "REMOTE" ] ; then
  sudo ../../tools/serial-io/serialdump -s /dev/ttyUSB1
fi

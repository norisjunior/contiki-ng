#!/bin/bash

if [[ $# -eq 0 ]] ; then
    echo "############################################################"
    echo "###                      6LBR start                      ###"
    echo "############################################################"
    echo 'usage:'
    echo '- hardware platform supported: LAUNCHPAD or FIREFLY'
    echo 'example: view_logs_app.sh SENSORTAG'
    echo '         view_logs_app.sh REMOTE'
    echo ''
    exit 0
fi

if   [ "$1" != 'SENSORTAG' ] && [ "$1" != 'REMOTE' ] ; then
  echo 'error: only SENSORTAG or REMOTE supported platforms'
  exit 0
fi

#sudo ../../tools/serial-io/tunslip6 -L -v -s /dev/ttyACM0 fd00::1/64

if [ "$1" == "SENSORTAG" ] ; then
  sudo ../../tools/serial-io/serialdump -s /dev/ttyACM2
elif [ "$1" == "FIREFLY" ] ; then
  sudo ../../tools/serial-io/serialdump -s /dev/ttyUSB1
fi

PARA O FEDSENSOR

Configurações que funcionaram:
Compilar o código com o contiker:
contiker/contiki-ng@sha256:deae47d07406e49a066da8f183722ace94762578a9e50b52387c801f77c881db
Usar: LOG_INFO para o MQTT e também para o TSCH

```
#!/bin/bash
cd FedSensor
git fetch origin
git reset --hard origin/master
git submodule update
cd contiki-ng-4.7
git submodule update --init --recursive
```


*Make examples:*
make lwpubsub-fedsensor-lwaiot TARGET=cc26x0-cc13x0 BOARD=sensortag/cc2650 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-lwaiot

make lwpubsub-fedsensor-lwaiot TARGET=cc26x0-cc13x0 BOARD=launchpad/cc2650 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-lwaiot

make lwpubsub-fedsensor-lwaiot TARGET=simplelink BOARD=launchpad/cc1352p2 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-lwaiot

make lwpubsub-fedsensor-lwaiot TARGET=zoul BOARD=remote-revb PORT=/dev/ttyUSB1 MAKE_KEYSIZE=128 MAKE_CRYPTOMODE=CCM MAKE_WITH_ENERGY=1 lwpubsub-fedsensor-lwaiot.upload

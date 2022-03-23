CONTIKI_PROJECT = lwpubsub-fedsensor-lwaiot
all: $(CONTIKI_PROJECT)

CONTIKI = ../..

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt



#TSCH SCHEDULE DEFINITIONS
ifndef MAKE_WITHSCHEDORCHESTRA
  ${info TSCH SCHEDULE not defined, using TSCH-MINIMAL}
endif

ifeq ($(MAKE_WITHSCHEDORCHESTRA),1)
  ${info TSCH SCHEDULE - ORCHESTRA}
  MODULES += os/services/orchestra
  CFLAGS += -DLWPUBSUB_ORCHESTRA=1
else
  ${info TSCH SCHEDULE not defined, using TSCH-MINIMAL}
endif


-include $(CONTIKI)/Makefile.identify-target

MODULES_REL += arch/platform/$(TARGET)

PLATFORMS_ONLY = cc26x0-cc13x0 cc2538dk openmote zoul native simplelink cooja


#Customizations

###
#Target definitions
MAKE_NATIVE = 0

ifeq ($(TARGET),native)
  ${info TARGET IS NATIVE - no energest!}
  MAKE_NATIVE = 1
else
  MAKE_MAC = MAKE_MAC_TSCH
endif

ifeq ($(MAKE_NATIVE),1)
  CFLAGS += -DMAKE_NATIVE=1
endif

###
#Energy definition
ifeq ($(MAKE_WITH_ENERGY),1)
  CFLAGS += -DENERGEST_CONF_ON=1
endif



###
#Crypto keysize definition
ifndef MAKE_KEYSIZE
  ${info KEYSIZE not defined, using AES128}
  CFLAGS += -DAES128=1
  CFLAGS += -DKEYSIZE=128
endif

ifeq ($(MAKE_KEYSIZE), 128)
  ${info KEY - 128}
  CFLAGS += -DAES128=1
  CFLAGS += -DKEYSIZE=128
else ifeq ($(MAKE_KEYSIZE), 192)
  ${info KEY - 192}
  CFLAGS += -DAES192=1
  CFLAGS += -DKEYSIZE=192
else ifeq ($(MAKE_KEYSIZE), 256)
  ${info KEY - 256}
  CFLAGS += -DAES256=1
  CFLAGS += -DKEYSIZE=256
endif



###
#Crypto algorithm definition
ifndef MAKE_CRYPTOMODE
  ${info CRYPTOMODE not defined, using CCM}
  CFLAGS += -DCRYPTOMODE=1
endif

ifeq ($(MAKE_CRYPTOMODE), CBC)
  ${info CRYPTO MODE - CBC}
  CFLAGS += -DCRYPTOMODE=2
else ifeq ($(MAKE_CRYPTOMODE), CTR)
  ${info CRYTPO MODE - CTR}
  CFLAGS += -DCRYPTOMODE=3
else ifeq ($(MAKE_CRYPTOMODE), CCM)
  ${info CRYTPO MODE - CCM}
  CFLAGS += -DCRYPTOMODE=1

#else
#	${error CRYPTOMODE not recognized}
endif



###
#Polling frequency definition
ifndef MAKE_POLLFREQUENCY
  ${info POLLFREQUENCY not defined, using 1 message each 10 seconds}
  CFLAGS += -DLWPUBSUB_POLLFREQUENCY=10
endif

ifeq ($(MAKE_POLLFREQUENCY), VERYHIGH)
  ${info POLL FREQUENCY - VERY HIGH}
  CFLAGS += -DLWPUBSUB_POLLFREQUENCY=5
else ifeq ($(MAKE_POLLFREQUENCY), HIGH)
  ${info POLL FREQUENCY - HIGH}
  CFLAGS += -DLWPUBSUB_POLLFREQUENCY=15
else ifeq ($(MAKE_POLLFREQUENCY), MEDIUM)
  ${info POLL FREQUENCY - MEDIUM}
  CFLAGS += -DLWPUBSUB_POLLFREQUENCY=900
else ifeq ($(MAKE_POLLFREQUENCY), LOW)
  ${info POLL FREQUENCY - LOW}
  CFLAGS += -DLWPUBSUB_POLLFREQUENCY=21600
else ifeq ($(MAKE_POLLFREQUENCY), VERYLOW)
  ${info POLL FREQUENCY - VERY LOW}
  CFLAGS += -DLWPUBSUB_POLLFREQUENCY=86400
#else
#	${error CRYPTOMODE not recognized}
endif

#for math.h
TARGET_LIBFILES += -lm

SMALL = 1


include $(CONTIKI)/Makefile.include

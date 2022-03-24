CONTIKI_PROJECT = border-router
all: $(CONTIKI_PROJECT)
CONTIKI = ../..

# The BR is either native or embedded, and in the latter case must support SLIP
PLATFORMS_EXCLUDE = nrf52dk z1
BOARDS_EXCLUDE = nrf52840/dongle


MODULES += os/services/shell



# Include RPL BR module
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/rpl-border-router
# Include webserver module
MODULES_REL += webserver
# Include optional target-specific module
include $(CONTIKI)/Makefile.identify-target
MODULES_REL += $(TARGET)




###
#Layer 2
ifeq ($(LINK_LAYER),TSCH)
  MAKE_MAC = MAKE_MAC_TSCH
endif





include $(CONTIKI)/Makefile.include

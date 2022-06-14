/*
 * Copyright (c) 2012, Texas Instruments Incorporated - http://www.ti.com/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* Enable TCP */
#define UIP_CONF_TCP 1

/* Disable UDP */
#define UIP_CONF_UDP 0


/* Change to 1 to use with the IBM Watson IoT platform */
#define MQTT_CLIENT_CONF_WITH_IBM_WATSON 0

/*
 * The IPv6 address of the MQTT broker to connect to.
 * Ignored if MQTT_CLIENT_CONF_WITH_IBM_WATSON is 1
 */
#define MQTT_CLIENT_CONF_BROKER_IP_ADDR "fd00::1"

//Connection with test mosquitto MQTT Broker
//#define MQTT_CLIENT_CONF_BROKER_IP_ADDR "0064:ff9b:0000:0000:0000:0000:05c4:5fd0"

//#ProjectHuaweiUSP
//IPv4: 159.138.214.207
//IPv6: 9f8a:d6cf
//#define MQTT_CLIENT_CONF_BROKER_IP_ADDR "0064:ff9b:0000:0000:0000:0000:9f8a:d6cf"

/*
 * The Organisation ID.
 *
 * When in Watson mode, the example will default to Org ID "quickstart" and
 * will connect using non-authenticated mode. If you want to use registered
 * devices, set your Org ID here and then make sure you set the correct token
 * through MQTT_CLIENT_CONF_AUTH_TOKEN.
 */
#ifndef MQTT_CLIENT_CONF_ORG_ID
#define MQTT_CLIENT_CONF_ORG_ID "LWPubSub"
#endif

/*
 * The MQTT username.
 *
 * Ignored in Watson mode: In this mode the username is always "use-token-auth"
 */
// #define MQTT_CLIENT_CONF_USERNAME "mqtt-client-username"

/*
 * The MQTT auth token (password) used when connecting to the MQTT broker.
 *
 * Used with as well as without Watson.
 *
 * Transported in cleartext!
 */
// #define MQTT_CLIENT_CONF_AUTH_TOKEN "AUTHTOKEN"


/*
 * Security
*/
//Parameters

#define LWPUBSUB_AES_CBC_128 0x01
#define LWPUBSUB_AES_CBC_192 0x02
#define LWPUBSUB_AES_CBC_256 0x03
#define LWPUBSUB_AES_CTR_128 0x11
#define LWPUBSUB_AES_CTR_192 0x12
#define LWPUBSUB_AES_CTR_256 0x13
#define LWPUBSUB_AES_CCM     0x21
#define LWPUBSUB_NO_AES      0x00 //Only for testing purposes

//Always encrypted if not stated
#ifndef LWPUBSUB_IS_ENCRYPTED
  #define LWPUBSUB_IS_ENCRYPTED 1
#endif

//CBC mode value is 2
//CTR mode value is 3
#if (KEYSIZE == 192) && (CRYPTOMODE == 2)
  #define LWPUBSUB_CRYPTO_MODE LWPUBSUB_AES_CBC_192
#elif (KEYSIZE == 256) && (CRYPTOMODE == 2)
  #define LWPUBSUB_CRYPTO_MODE LWPUBSUB_AES_CBC_256
#elif (KEYSIZE == 128) && (CRYPTOMODE == 3)
  #define LWPUBSUB_CRYPTO_MODE LWPUBSUB_AES_CTR_128
#elif (KEYSIZE == 192) && (CRYPTOMODE == 3)
  #define LWPUBSUB_CRYPTO_MODE LWPUBSUB_AES_CTR_192
#elif (KEYSIZE == 256) && (CRYPTOMODE == 3)
  #define LWPUBSUB_CRYPTO_MODE LWPUBSUB_AES_CTR_256
#elif (KEYSIZE == 128) && (CRYPTOMODE == 2)
  #define LWPUBSUB_CRYPTO_MODE LWPUBSUB_AES_CBC_128
#elif (KEYSIZE == 128) && (CRYPTOMODE == 1)
  #define LWPUBSUB_CRYPTO_MODE LWPUBSUB_AES_CCM
#endif



//#define LOG_CONF_LEVEL_IPV6                        LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_RPL                         LOG_LEVEL_INFO
//#define LOG_CONF_LEVEL_6LOWPAN                     LOG_LEVEL_DBG
//#define LOG_CONF_LEVEL_TCPIP                       LOG_LEVEL_DBG
#define LOG_CONF_LEVEL_MAC                         LOG_LEVEL_INFO
//#define LOG_CONF_LEVEL_FRAMER                      LOG_LEVEL_INFO
#define MQTT_CLIENT_CONF_LOG_LEVEL                 LOG_LEVEL_INFO


#ifndef ENERGEST_CONF_ON
  #define ENERGEST_CONF_ON 0
#endif

#if (MAKE_NATIVE == 1)
  #define ENERGEST_CONF_ON 0
#endif


#if BOARD_SENSORTAG || BOARD_LAUNCHPAD
#define RF_BLE_CONF_ENABLED 0
#define SET_CCFG_SIZE_AND_DIS_FLAGS_DIS_GPRAM        0x0 //disable cache and use the space as RAM
#define RF_CONF_MODE    RF_MODE_2_4_GHZ //for launchpad
#endif

//Reduce LPM modes on zoul. Never enter LPM 2, only 0 (no LPM) or 1
#define LPM_CONF_MAX_PM   0

//Saving RAM
#define QUEUEBUF_CONF_NUM 4
#define NBR_TABLE_CONF_MAX_NEIGHBORS 4
#define NETSTACK_MAX_ROUTE_ENTRIES 4


#define PROCESS_CONF_NO_PROCESS_NAMES 1

/*---------------------------------------------------------------------------*/
#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
/** @} */

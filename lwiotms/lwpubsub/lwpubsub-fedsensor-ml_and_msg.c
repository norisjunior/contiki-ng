/*
 * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
 * Copyright (c) 2017, George Oikonomou - http://www.spd.gr
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
#include "contiki.h"
#include "net/routing/routing.h"
#include "mqtt.h"
#include "mqtt-prop.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "os/sys/log.h"
#include "mqtt-client.h"
#if BOARD_SENSORTAG
#include "board-peripherals.h"
#include "ti-lib.h"
#endif

#include <string.h>
#include <strings.h>
#include <stdarg.h>


//LWPubSub - AES functions
#include "lib/tiny-aes.h"

#include <stdio.h> /* For printf() */

#include  <stdlib.h>

#include <math.h>

#include "lib/random.h"


//LWPubSub - AES-CCM_STAR
#include "lib/ccm-star.h"
#include "lib/hexconv.h"
#include <stdbool.h>

#define MICLEN 8

//Noris
#if (ENERGEST_CONF_ON == 1)
  #include "sys/energest.h"
#endif /* ENERGEST_ON */




/*---------------------------------------------------------------------------*/
#define LOG_MODULE "LWPubSub"
#ifdef MQTT_CLIENT_CONF_LOG_LEVEL
#define LOG_LEVEL MQTT_CLIENT_CONF_LOG_LEVEL
#else
#define LOG_LEVEL LOG_LEVEL_NONE
#endif
/*---------------------------------------------------------------------------*/
/* Controls whether the example will work in IBM Watson IoT platform mode */
#ifdef MQTT_CLIENT_CONF_WITH_IBM_WATSON
#define MQTT_CLIENT_WITH_IBM_WATSON MQTT_CLIENT_CONF_WITH_IBM_WATSON
#else
#define MQTT_CLIENT_WITH_IBM_WATSON 0
#endif
/*---------------------------------------------------------------------------*/
/* MQTT broker address. Ignored in Watson mode */
#ifdef MQTT_CLIENT_CONF_BROKER_IP_ADDR
#define MQTT_CLIENT_BROKER_IP_ADDR MQTT_CLIENT_CONF_BROKER_IP_ADDR
#else
#define MQTT_CLIENT_BROKER_IP_ADDR "fd00::1"
#endif
/*---------------------------------------------------------------------------*/
/*
 * MQTT Org ID.
 *
 * If it equals "quickstart", the client will connect without authentication.
 * In all other cases, the client will connect with authentication mode.
 *
 * In Watson mode, the username will be "use-token-auth". In non-Watson mode
 * the username will be MQTT_CLIENT_USERNAME.
 *
 * In all cases, the password will be MQTT_CLIENT_AUTH_TOKEN.
 */
#ifdef MQTT_CLIENT_CONF_ORG_ID
#define MQTT_CLIENT_ORG_ID MQTT_CLIENT_CONF_ORG_ID
#else
#define MQTT_CLIENT_ORG_ID "LWPubSub"
#endif
/*---------------------------------------------------------------------------*/
/* MQTT token */
#ifdef MQTT_CLIENT_CONF_AUTH_TOKEN
#define MQTT_CLIENT_AUTH_TOKEN MQTT_CLIENT_CONF_AUTH_TOKEN
#else
#define MQTT_CLIENT_AUTH_TOKEN "AUTHTOKEN"
#endif
/*---------------------------------------------------------------------------*/
#if MQTT_CLIENT_WITH_IBM_WATSON
/* With IBM Watson support */
static const char *broker_ip = "0064:ff9b:0000:0000:0000:0000:b8ac:7cbd";
#define MQTT_CLIENT_USERNAME "use-token-auth"

#else /* MQTT_CLIENT_WITH_IBM_WATSON */
/* Without IBM Watson support. To be used with other brokers, e.g. Mosquitto */
static const char *broker_ip = MQTT_CLIENT_BROKER_IP_ADDR;

/*
#ifdef MQTT_CLIENT_CONF_USERNAME
#define MQTT_CLIENT_USERNAME MQTT_CLIENT_CONF_USERNAME
#else
#define MQTT_CLIENT_USERNAME "use-token-auth"
#endif
*/

#endif /* MQTT_CLIENT_WITH_IBM_WATSON */
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_STATUS_LED
#define MQTT_CLIENT_STATUS_LED MQTT_CLIENT_CONF_STATUS_LED
#else
#define MQTT_CLIENT_STATUS_LED LEDS_GREEN
#endif
/*---------------------------------------------------------------------------*/
#ifdef MQTT_CLIENT_CONF_WITH_EXTENSIONS
#define MQTT_CLIENT_WITH_EXTENSIONS MQTT_CLIENT_CONF_WITH_EXTENSIONS
#else
#define MQTT_CLIENT_WITH_EXTENSIONS 0
#endif
/*---------------------------------------------------------------------------*/
/*
 * A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect)
 */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)

/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)

/*---------------------------------------------------------------------------*/
/*
 * Number of times to try reconnecting to the broker.
 * Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;
#define STATE_INIT            0
#define STATE_REGISTERED      1
#define STATE_CONNECTING      2
#define STATE_CONNECTED       3
#define STATE_PUBLISHING      4
#define STATE_DISCONNECTED    5
#define STATE_NEWCONFIG       6
#define STATE_CONFIG_ERROR 0xFE
#define STATE_ERROR        0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_ORG_ID_LEN        32
#define CONFIG_TYPE_ID_LEN       32
#define CONFIG_AUTH_TOKEN_LEN    32
#define CONFIG_EVENT_TYPE_ID_LEN 32
#define CONFIG_CMD_TYPE_LEN       8
#define CONFIG_IP_ADDR_STR_LEN   64
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION         (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
/* Default configuration values */
#define DEFAULT_TYPE_ID             "mqtt-client"
#define DEFAULT_EVENT_TYPE_ID       "status"
#define DEFAULT_SUBSCRIBE_CMD_TYPE  "+"
#define DEFAULT_BROKER_PORT         1883

//Noris
//A variável passada no Makefile é: LWPUBSUB_POLLFREQUENCY
//#define DEFAULT_PUBLISH_INTERVAL    (30 * CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
#ifndef LWPUBSUB_POLLFREQUENCY
#define LWPUBSUB_POLLFREQUENCY 15
#endif
/*---------------------------------------------------------------------------*/

#define DEFAULT_PUBLISH_INTERVAL    (LWPUBSUB_POLLFREQUENCY * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER    60
// #if (LWPUBSUB_POLLFREQUENCY >= 14400)
//   #define DEFAULT_KEEP_ALIVE_TIMER    (65535)
// #else
//   #define DEFAULT_KEEP_ALIVE_TIMER    (LWPUBSUB_POLLFREQUENCY * 6)
// #endif
#define DEFAULT_RSSI_MEAS_INTERVAL  (CLOCK_SECOND * 30)
/*---------------------------------------------------------------------------*/
#define MQTT_CLIENT_SENSOR_NONE     (void *)0xFFFFFFFF
/*---------------------------------------------------------------------------*/
/* Payload length of ICMPv6 echo requests used to measure RSSI with def rt */
#define ECHO_REQ_PAYLOAD_LEN   20
/*---------------------------------------------------------------------------*/
PROCESS_NAME(mqtt_client_process);
AUTOSTART_PROCESSES(&mqtt_client_process);
/*---------------------------------------------------------------------------*/





/*GLOBAL VARS FOR LWAIOT*/
static int line = 0;
static int column = 0;
static int m = 0; // controla o índice do valor a ser recebido no char valuereceived[] que será valuereceived[m], o certo é não usar a variável column
static char pipe[2] = "|";
static char semic[2] = ";";
static char hashtag[2] = "#";
static char dois[2] = "2";
static char valuereceived[10];



/* Variable used to inform publish() to do not print energest values When
   startting from commandReceived */
static int publish_from_command = 0;
/* Format: objectID||instanceID
   Example: 33030, 33031, 33040, 33040, 33380, 33110, 33111, 33250, 33251, etc.
   5 char + \0
*/
#define IPSO_SENSOR_SIZE 6
static int ml_model = 0;
//static int make_decision = 0;
static int sensorsNumber = 0;
static unsigned long sensorList[10]; //33250\0
static int polling_interval = 0;
static float new_observation[10]; //{30.39, 89.86, 0.86, 42.86, 0.0, 0.0, 0.0, 0.0, 0.0};
static char temp[4]; //para guardar a action
static int action = 0;

// static int MODEL_IN_USE = 0;
// //Controla se o modelo está em uso ou não. Se estiver em uso, não precisa ficar fazendo as contas de


/*For k-means*/
static float centroids[10][10]; // são 10 linhas de 10 colunas com valores float
static int number_of_centroids = 0;

/*For logreg*/
static float bias[10];
static float weights[10][10]; // são 10 linhas de 10 colunas com valores float
static int number_of_classes = 0;

/*For linreg*/
static float lin_bias = 0.0;
static float lin_weights[10];




// Get measurements
#if BOARD_SENSORTAG
#define SENSOR_READING_PERIOD (CLOCK_SECOND * 10) // de 20 para 10
#define SENSOR_READING_RANDOM (CLOCK_SECOND << 4)

static struct ctimer tmp_timer, hdc_timer, mpu_timer;

static void init_tmp_reading(void *not_used);
static void init_hdc_reading(void *not_used);
static void init_mpu_reading(void *not_used);

#define HDC_1000_READING_ERROR    CC26XX_SENSOR_READING_ERROR
#define TMP_007_READING_ERROR     CC26XX_SENSOR_READING_ERROR
static int value_measurement = 0;

/*---------------------------------------------------------------------------*/
static void
print_mpu_reading(int reading)
{
  if(reading < 0) {
    printf("-");
    reading = -reading;
  }

  printf("%d.%02d", reading / 100, reading % 100);
}
/*---------------------------------------------------------------------------*/
static void
get_tmp_reading()
{

  clock_time_t next = SENSOR_READING_PERIOD +
    (random_rand() % SENSOR_READING_RANDOM);


    value_measurement = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_TEMP);
    if(value_measurement != HDC_1000_READING_ERROR) {
      printf("HDC: Temp=%d.%02d C\n", value_measurement / 100, value_measurement % 100);
    } else {
      printf("HDC: Temp Read Error\n");
    }

  // value_measurement = tmp_007_sensor.value(TMP_007_SENSOR_TYPE_ALL);
  //
  // if(value_measurement == TMP_007_READING_ERROR) {
  //   printf("TMP: Ambient Read Error\n");
  //   return;
  // }
  //
  // value_measurement = tmp_007_sensor.value(TMP_007_SENSOR_TYPE_AMBIENT);
  // printf("TMP: Ambient=%d.%03d C\n", value_measurement / 1000, value_measurement % 1000);

  // value = tmp_007_sensor.value(TMP_007_SENSOR_TYPE_OBJECT);
  // printf("TMP: Object=%d.%03d C\n", value / 1000, value % 1000);

  // SENSORS_DEACTIVATE(tmp_007_sensor);

  ctimer_set(&tmp_timer, next, init_tmp_reading, NULL);
}
/*---------------------------------------------------------------------------*/
static void
get_hdc_reading()
{
  clock_time_t next = SENSOR_READING_PERIOD +
    (random_rand() % SENSOR_READING_RANDOM);

  value_measurement = hdc_1000_sensor.value(HDC_1000_SENSOR_TYPE_HUMID);
  if(value_measurement != HDC_1000_READING_ERROR) {
    printf("HDC: Humidity=%d.%02d %%RH\n", value_measurement / 100, value_measurement % 100);
  } else {
    printf("HDC: Humidity Read Error\n");
  }

  ctimer_set(&hdc_timer, next, init_hdc_reading, NULL);

}
/*---------------------------------------------------------------------------*/
static void
get_mpu_reading()
{
  int value;

  clock_time_t next = SENSOR_READING_PERIOD +
    (random_rand() % SENSOR_READING_RANDOM);

  printf("MPU Gyro: X=");
  value = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_X);
  print_mpu_reading(value);
  printf(" deg/sec\n");

  printf("MPU Gyro: Y=");
  value = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Y);
  print_mpu_reading(value);
  printf(" deg/sec\n");

  printf("MPU Gyro: Z=");
  value = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_GYRO_Z);
  print_mpu_reading(value);
  printf(" deg/sec\n");

  printf("MPU Acc: X=");
  value = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_X);
  print_mpu_reading(value);
  printf(" G\n");
  new_observation[0] = (float)value/100;

  printf("MPU Acc: Y=");
  value = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Y);
  print_mpu_reading(value);
  printf(" G\n");
  new_observation[1] = (float)value/100;

  printf("MPU Acc: Z=");
  value = mpu_9250_sensor.value(MPU_9250_SENSOR_TYPE_ACC_Z);
  print_mpu_reading(value);
  printf(" G\n");
  new_observation[2] = (float)value/100;

  SENSORS_DEACTIVATE(mpu_9250_sensor);

  ctimer_set(&mpu_timer, next, init_mpu_reading, NULL);
}
/*---------------------------------------------------------------------------*/
static void
init_tmp_reading(void *not_used)
{
  // SENSORS_ACTIVATE(tmp_007_sensor);
  SENSORS_ACTIVATE(hdc_1000_sensor);
}
/*---------------------------------------------------------------------------*/
static void
init_hdc_reading(void *not_used)
{
  SENSORS_ACTIVATE(hdc_1000_sensor);
}
/*---------------------------------------------------------------------------*/
static void
init_mpu_reading(void *not_used)
{
  mpu_9250_sensor.configure(SENSORS_ACTIVE, MPU_9250_SENSOR_TYPE_ALL);
}
#endif
/*---------------------------------------------------------------------------*/







/**
 * \brief Data structure declaration for the MQTT client configuration
 */
typedef struct mqtt_client_config {
  char org_id[CONFIG_ORG_ID_LEN];
  char type_id[CONFIG_TYPE_ID_LEN];
  char auth_token[CONFIG_AUTH_TOKEN_LEN];
  char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
  char broker_ip[CONFIG_IP_ADDR_STR_LEN];
  char cmd_type[CONFIG_CMD_TYPE_LEN];
  clock_time_t pub_interval;
  int def_rt_ping_interval;
  uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE    32
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 *
 * d:quickstart:status:EUI64 is 32 bytes long
 * iot-2/evt/status/fmt/json is 25 bytes
 * We also need space for the null termination
 */

//LWPubSub - Buffer size at least 120
#define BUFFER_SIZE 25
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
#define APP_BUFFER_SIZE 512
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
#define QUICKSTART "quickstart"
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
/* Parent RSSI functionality */
static struct uip_icmp6_echo_reply_notification echo_reply_notification;
static struct etimer echo_request_timer;
static int def_rt_rssi = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;
/*---------------------------------------------------------------------------*/
#if MQTT_CLIENT_WITH_EXTENSIONS
extern const mqtt_client_extension_t *mqtt_client_extensions[];
extern const uint8_t mqtt_client_extension_count;
#else
static const mqtt_client_extension_t *mqtt_client_extensions[] = { NULL };
static const uint8_t mqtt_client_extension_count = 0;
#endif
/*---------------------------------------------------------------------------*/
/* MQTTv5 */
#if MQTT_5
static uint8_t PUB_TOPIC_ALIAS;

struct mqtt_prop_list *publish_props;

/* Control whether or not to perform authentication (MQTTv5) */
#define MQTT_5_AUTH_EN 0
#if MQTT_5_AUTH_EN
struct mqtt_prop_list *auth_props;
#endif
#endif
/*---------------------------------------------------------------------------*/













/*---------------------------------------------------------------------------*/
/********************************* LWPubSub **********************************/
#define LWPUBSUB_TOPIC_SIZE 16

char *key = "4e6f726973504144506f6c6955535021";
static uint8_t key_bytes[16]; //AES-CCM-8 - 128-bit key


//char *nonce = "0000000000000";
#define NONCE_LEN 13
static char nonce[NONCE_LEN*2] = "00000000000000000000000000";

static void generate_nonce()
{
  //Random initialization
  random_init(random_rand() + (int16_t)uipbuf_get_attr(UIPBUF_ATTR_RSSI));

//  printf("Rand 13: %lu", rand()%13);

  for (int i = 0, j = 0; i < NONCE_LEN; ++i, j += 2)
  {
    sprintf(nonce + j, "%02x", rand()%13 & 0xff);
  }

  LOG_DBG("Nonce generate_nonce: %s\n\n", nonce);
  // for (int i = 0; i < 13; i++) {
  //    nonce[i] = rand();
  // }
}


#if (LOG_LEVEL == LOG_LEVEL_DBG)
static void dump(uint8_t* str, int len)
{
  unsigned char i;
  for (i = 0; i < len; ++i) {
    if (i % 4 == 0) { printf("  "); }
    if (i % 16 == 0) { printf("\n"); }
    printf("%.2x ", str[i]);
  }
  printf("\n");
}
#endif  /* IF LOG_LEVEL_DBG - PHEX and DUMP */

// https://stackoverflow.com/questions/23191203/convert-float-to-string-without-sprintf
#define CHAR_BUFF_SIZE 10
static char * _float_to_char(float x, char *p) {
    char *s = p + CHAR_BUFF_SIZE; // go to end of buffer
    uint16_t decimals;  // variable to store the decimals
    int units;  // variable to store the units (part to left of decimal place)
    if (x < 0) { // take care of negative numbers
        decimals = (int)(x * -10000) % 10000; // make 1000 for 3 decimals etc.
        units = (int)(-1 * x);
    } else { // positive numbers
        decimals = (int)(x * 10000) % 10000;
        units = (int)x;
    }

    *--s = (decimals % 10) + '0';
    decimals /= 10; // repeat for as many decimal places as you need
    *--s = (decimals % 10) + '0';

    decimals /= 10; // repeat for as many decimal places as you need
    *--s = (decimals % 10) + '0';

    decimals /= 10; // repeat for as many decimal places as you need
    *--s = (decimals % 10) + '0';

    *--s = '.';

    while (units > 0) {
        *--s = (units % 10) + '0';
        units /= 10;
    }
    if (x < 0) *--s = '-'; // unary minus sign for negative numbers
    return s;
}

/************************ SECURITY FUNCTIONS *********************************/
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/

//static char metadata[20];
static char objectID[6];
static char instanceID[2];
#define COMMAND_RECEIVED_BUFFER 500
char commandReceived[COMMAND_RECEIVED_BUFFER];
static int commandReceivedlen = 0;



static void parsePayload(uint8_t* mqttPayload, int mqttPayload_len)
{
  //char payload[200];
  //memset(payload, 0, sizeof(payload));
  //memset(valuereceived, 0, sizeof(valuereceived));

  //Limpando todas as variáveis existentes:
  //memset(objectID, 0, sizeof(objectID));
  //memset(instanceID, 0, sizeof(instanceID));
  memset(commandReceived, 0, COMMAND_RECEIVED_BUFFER);

  //strncpy(payload, (char *)mqttPayload, mqttPayload_len);


#if (LOG_LEVEL == LOG_LEVEL_DBG)
  LOG_DBG("FUNCTION parsePayload - mqttPayload decrypted: "); dump(mqttPayload, mqttPayload_len); printf("\n");
#endif

  LOG_DBG("mqttPayload_len: %d\n", mqttPayload_len);

  //payload[mqttPayload_len] = '\0';

  //LOG_DBG("Payload: %s\n", payload);


// #if (MAKE_NATIVE == 1)
//   LOG_DBG("strlen payload size: %lu\n", strlen(payload));
// #else
//   LOG_DBG("strlen payload size: %u\n", strlen(payload));
// #endif

  //Exemplo de mensagem recebida: 32001010;1|33250;2|33251;3|33252;4|33253"

  int position = 0;
  int i, j = 0;

    for (int i = position; i < 5; i++) {
      //objectID[i] = payload[i];
      objectID[i] = (char)mqttPayload[i];
      position = i;
    }

    position++;

    objectID[position] = '\0';

    LOG_DBG("FUNCTION parsePayload, valor do position: %d\n", position);

//nem precisa ser o instanceID pode ser uma variável que controla o intervalo de
//tempo de obtenção das medições
    //instanceID[0] = payload[position];
    instanceID[0] = (char)mqttPayload[position];
    instanceID[1] = '\0';

    position += 2;

    //for (i = position, j = 0; i < strlen(payload); i++, j++) {
    for (i = position, j = 0; i < mqttPayload_len; i++, j++) {
      //commandReceived[j] = payload[i];
      commandReceived[j] = (char)mqttPayload[i];
    }

    commandReceived[j] = '\0';

    commandReceivedlen = j;

    LOG_DBG("ObjectID: %s\n", objectID);

    LOG_DBG("InstanceID: %s\n", instanceID);

    LOG_DBG("CommandReceived: %s\n", commandReceived);

    LOG_DBG(" - CommandReceivedLen: %d\n", commandReceivedlen);


#if BOARD_SENSORTAG
  if(strncmp(objectID, "03303", 5) == 0) { //commandReceived request measurement
    LOG_INFO(" - Temperature request - \n");
    // init_tmp_reading();
  }
  if(strncmp(objectID, "03304", 5) == 0) { //commandReceived request measurement
    LOG_INFO(" - Humidity request - \n");
    // init_hdc_reading();
  }
#endif

}



static void parse32001()
{
  //commandReceived:  100;1|033250;2|033251;3|033252;4|033253"
  char time_interval[4]; //até 999
  int i = 0;
  int j = 0;

  //*******************************************************************
  //Preencher intervalo de tempo
  for (i = 0; i < 3; i++) {
    time_interval[i] = commandReceived[i];
  }
  time_interval[i] = '\0';
  polling_interval = (int)strtol(time_interval, NULL, 10);
  LOG_DBG("Time interval: %d\n\n", polling_interval);

  conf.pub_interval = (polling_interval * CLOCK_SECOND); //(LWPUBSUB_POLLFREQUENCY * CLOCK_SECOND)
  //*******************************************************************

  sensorsNumber = ((commandReceivedlen - 3) / 9); //é o número de linhas da variável sensorList

  char sensorsreceived[10][7]; // são 10 linhas de 7 caracteres cada linha: 6 caract. + \0
  memset(sensorsreceived, 0, sizeof(sensorsreceived));

  // int line = 0;
  // int column = 0;
  // char pipe[2] = "|";
  // char semic[2] = ";";
  line = 0;
  column = 0;

  for (i = 5, j = 0; i < commandReceivedlen; i++, j++) {
    if ( (commandReceived[i] != pipe[0]) && (commandReceived[i] != semic[0]) && (commandReceived[i-1] != semic[0]) ) {
      sensorsreceived[line][column] = commandReceived[i];
      column++;
    }

    if ( (commandReceived[i] == semic[0]) ) { //pula pra próxima linha
      //sensorsreceived[line][column] = '\0';
      line++;
      column = 0;
    }
  }


  for (i = 0; i < sensorsNumber; i++) {
    sensorList[i] = strtol(sensorsreceived[i], NULL, 10);
    LOG_INFO("SensorList[%d]: %lu\n", i, sensorList[i]);
  }

#if BOARD_SENSORTAG
  if ( (sensorList[0] == 33130) && (sensorList[1] == 33131) && (sensorList[2] == 33132) ) { //real accelerometer data
    init_mpu_reading(NULL);
  }
#endif

}


static void linreg_predict()
{
  LOG_INFO("-------------------------- Linear Regression predict ---------------------------\n");
  #if (ENERGEST_CONF_ON == 1)
    energest_flush();
    LOG_INFO("MLModel %d predict start ", ml_model);
    printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
      energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
      energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
  #endif /* ENERGEST_CONF_ON */

  int n = 0; //índice do número de sensores (linhas dos weights)
  float result = 0.0; // para armazenar a multiplicação de new_observation[n] * weights[m][n]
  char reading[10];

  for (n = 0; n < sensorsNumber; n++) {
    result += new_observation[n] * lin_weights[n];
  }

  result += lin_bias;

  LOG_INFO_("[%.4f   ", result);
  LOG_INFO_("_  as string: %s]   \n", _float_to_char(result, reading));
  memset(reading, 0, sizeof(reading));
  reading[0] = '\0';

  if ((int)result > action) {
    LOG_INFO("A-L-E-R-T!\n");
    leds_on(LEDS_RED);
    #if BOARD_SENSORTAG
      buzzer_start(1000);
    #endif
  } else {
    LOG_INFO("Good condition. Result = %s, action = %d\n", _float_to_char(result, reading), action);
    leds_off(LEDS_RED);
    #if BOARD_SENSORTAG
      if(buzzer_state()) {
        buzzer_stop();
      }
    #endif
  }

  LOG_INFO("---------------------------------------------------------------------------\n");
  #if (ENERGEST_CONF_ON == 1)
    energest_flush();
    LOG_INFO("MLModel %d predict finish ", ml_model);
    printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
      energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
      energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
  #endif /* ENERGEST_CONF_ON */
}







static void kmeans_predict()
{
  LOG_INFO("------------------------------- K-means predict --------------------------------\n");
  #if (ENERGEST_CONF_ON == 1)
    energest_flush();
    LOG_INFO("MLModel %d predict start ", ml_model);
    printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
      energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
      energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
  #endif /* ENERGEST_CONF_ON */

  //LOG_DBG("commandReceived: %s", commandReceived);


  //*******************************************************************
  //Preencher matriz, em que cada linha é um centróide
  float dist_euclid[10];
  float middle_result[10][10];
  char reading[10];


  //sensorsNumber = 4; //only for testing!

  for (int i = 0; i < 10; i++ ) {
    for (int j = 0; j < 10; j++) {
      middle_result[i][j] = 0.0;
    }
    dist_euclid[i] = 0.0;
  }

  for (int i = 0; i < number_of_centroids; i++ ) { // para cada centróide
    for (int j = 0; j < sensorsNumber; j++) { // número de sensores (número de variáveis, cada sensor é uma variável que coleta dados)
      //LOG_DBG_("new_observation[%d]: %.2f", j, new_observation[j]);
      middle_result[i][j] = new_observation[j] - centroids[i][j];
      dist_euclid[i] += powf(middle_result[i][j],2);
    }
    dist_euclid[i] = sqrtf(dist_euclid[i]);

    LOG_INFO("Dist_euclid[%d]: %.4f  ", i, dist_euclid[i]);
    LOG_INFO_("_  as string: %s\n", _float_to_char(dist_euclid[i], reading));
    memset(reading, 0, sizeof(reading));
    reading[0] = '\0';

    // LOG_INFO_(" ---- as int: %d\n", (int)dist_euclid[i]);

  }

  // Verifica cluster
  int k = 0;
  for (int i = 0; i < number_of_centroids; i++) {
    if (dist_euclid[i] < dist_euclid[k]) { k = i; }
  }

  LOG_INFO("Measurement is closest to cluster (k): %d\n", k);

  if (k == action) {
    LOG_INFO("A-L-E-R-T!\n");
    leds_on(LEDS_RED);
    #if BOARD_SENSORTAG
      buzzer_start(1000);
    #endif
  } else {
    LOG_INFO("Nothing to do once k != target; result k = %d, target k = %d\n", k, action);
    leds_off(LEDS_RED);
    #if BOARD_SENSORTAG
      if(buzzer_state()) {
        buzzer_stop();
      }
    #endif
  }

  LOG_INFO("--------------------------------------------------------------------------------\n");
  #if (ENERGEST_CONF_ON == 1)
    energest_flush();
    LOG_INFO("MLModel %d predict finish ", ml_model);
    printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
      energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
      energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
  #endif /* ENERGEST_CONF_ON */
}




static void logreg_predict()
{
  //LOG_DBG("PREDICT! Regressão Logística!\n");
  LOG_INFO("------------------------- Logistic Regression predict --------------------------\n");
  #if (ENERGEST_CONF_ON == 1)
    energest_flush();
    LOG_INFO("MLModel %d predict start ", ml_model);
    printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
      energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
      energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
  #endif /* ENERGEST_CONF_ON */
  //printf("commandReceived: %s", commandReceived);

  int m = 0; //índice do número de classes (colunas dos weights)
  int n = 0; //índice do número de sensores (linhas dos weights)
  float mult_values_weights[10]; // para armazenar a multiplicação de new_observation[n] * weights[m][n]
  float exp_sum = 0; //somatório de e (início do Softmax)
  float logreg_prob[10]; //armazena as probabilidades de cada classe
  // char fbuf[15]; //buffer para armazenar o valor de float para log
  int logreg_class = 0; //armazena qual é a classe resultante da regressão logística
  char reading[10];




  //inicialização das variáveis:
  for (m = 0; m < 10; m++) {
    mult_values_weights[m] = 0.0;
    logreg_prob[m] = 0.0;
  }
  m = 0;

  /*FAZER PREVISÃO COM BASE NAS NOVAS MEDIÇÕES*/

  //FAZER OS CÁLCULOS AGORA!
  /*
  #z = X*w + b
  #X -> medições
  #w -> weights
  #b -> bias
  z = nova_medicao@clf.coef_.T + clf.intercept_

  Aqui:
  X é: new_observation[10], na verdade new_observation[4] (usando 33250, 33251, 33252, 33253), seria isso:
    (i == 4) new_observation[i] = {30.39, 89.86, 0.86, 42.86};
  w é: weights[10][10], no exemplo com 3 classes:
    Class [0]: [-0.0636] [-0.0477] [-1.0038] [-0.0266]
    Class [1]: [-0.0156] [0.0121] [0.1495] [0.0092]
    Class [2]: [0.0792] [0.0356] [0.8543] [0.0174]
  Então, X*w é:
  30.39 * -0.0636 + 89.86 * -0.0477 + 0.86 * -1.0038 + 42.86 * -0.0266 = -1.931434931 + -4.282786009  + -0.863268378  + -1.14031773 = -8.217807048


  */
  //Multiplicação X*w (new_observation[j] * weights[m][n])
  for (m = 0; m < number_of_classes; m++) {
    //printf("\n Class %d: ", m);
    for (n = 0; n < sensorsNumber; n++) {
      mult_values_weights[m] += new_observation[n] * weights[m][n];
    }
  }

  // LOG_INFO("Resultado de X*w: ");
  // for (int i = 0; i < number_of_classes ; i++) {
  //   LOG_INFO_("[%.4f] ", mult_values_weights[i]);
  // }
  // LOG_INFO_("\n");


  for (n = 0; n < number_of_classes ; n++) {
    mult_values_weights[n] += bias[n];
  }

  LOG_INFO("Result of X*w + b: ");
  for (int i = 0; i < number_of_classes ; i++) {
    LOG_INFO_("[%.4f   ", mult_values_weights[i]);
    LOG_INFO_("_  as string: %s]   ", _float_to_char(mult_values_weights[i], reading));
    memset(reading, 0, sizeof(reading));
    reading[0] = '\0';
  }
  LOG_INFO_("\n");

  // Softmax:
  //Exp_sum:
  for (n = 0; n < number_of_classes ; n++) {
    exp_sum += exp(mult_values_weights[n]);
  }

  // LOG_INFO("EXP_SUM: %.4f", exp_sum);
  // LOG_INFO_(" -- as int: %d\n", (int)exp_sum);

  for (n = 0; n < number_of_classes ; n++) {
    logreg_prob[n] = exp(mult_values_weights[n]) / exp_sum;
    // snprintf(fbuf, 14, "%g", logreg_prob[n]);
    LOG_INFO("logreg_prob[%d]: %.4f  ", n, logreg_prob[n]);
    LOG_INFO("_ as string: %s \n", _float_to_char(logreg_prob[n], reading));
    memset(reading, 0, sizeof(reading));
    reading[0] = '\0';
    //printf(" --- as int: %d.%d", (int)logreg_prob[n], ((int)(logreg_prob[n] * 1000)%1000));
    // memset(fbuf, 0, sizeof(fbuf));
    // LOG_INFO_(" as string: %s\n", fbuf);
    // fbuf[0] = '\0'; //zera a variável fbuf
  }

  for (n = 0; n < number_of_classes ; n++) {
    if (logreg_prob[n] > logreg_prob[logreg_class]) { logreg_class = n; }
  }

  LOG_INFO("Class: %d\n", logreg_class);

  if (logreg_class == action) {
    LOG_INFO("A-L-E-R-T!\n");
    leds_on(LEDS_RED);
    #if BOARD_SENSORTAG
      buzzer_start(1000);
    #endif
  } else {
    LOG_INFO("Class != ACTION; class = %d, action = %d\n", logreg_class, action);
    leds_off(LEDS_RED);
    #if BOARD_SENSORTAG
      if(buzzer_state()) {
        buzzer_stop();
      }
    #endif
  }

  LOG_INFO("---------------------------------------------------------------------------\n");
  #if (ENERGEST_CONF_ON == 1)
    energest_flush();
    LOG_INFO("MLModel %d predict finish ", ml_model);
    printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
      energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
      energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
  #endif /* ENERGEST_CONF_ON */


}





char sensor_value[10];
int measurement_type = 0;

static float read_33130()
{
  //Dummy:
  // 0 - start value (x_coord, y_coord, z_coord)
  /*  10 times not falling (x_coord max 2)
      1 time falling (class 0) ........: 3.5665, 0.8648, -0.3776
      REPEAT
  */
    memset(sensor_value, 0, 10);
    float value = 0.0;


    if (measurement_type == 0) { // fall
        //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
        //float value = strtof(sensor_value, NULL);
        // value = 0.980;
        snprintf(sensor_value, 10, "%d.%d", (2+rand()%2), (rand()%10000));
        value = strtof(sensor_value, NULL);
    } else {
      snprintf(sensor_value, 10, "%d.%d", (0+rand()%1), (rand()%10000));
      value = strtof(sensor_value, NULL);
    }
    //printf("sensor string: %s\n", sensor_value);
    //printf("sensor float: %f\n", value);

    // movement_type++;

    return value;
}

static float read_33131()
{
  //Dummy:
  // 0 - start value (x_coord, y_coord, z_coord)
  /*  10 times not falling (x_coord max 2)
      1 time falling (class 0) ........: 3.5665, 0.8648, -0.3776
      REPEAT
  */
    memset(sensor_value, 0, 10);
    float value = 0.0;

    if (measurement_type == 0) { // fall
        //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
        //float value = strtof(sensor_value, NULL);
        // value = 0.031;
        snprintf(sensor_value, 10, "-%d.%d", (rand()%1), (rand()%10000));
        value = strtof(sensor_value, NULL);
    } else {
      snprintf(sensor_value, 10, "%d.%d", 0, (rand()%10000));
      value = strtof(sensor_value, NULL);
    }
    //printf("sensor string: %s\n", sensor_value);
    //printf("sensor float: %f\n", value);

    return value;
}

static float read_33132()
{
  //Dummy:
  // 0 - start value (x_coord, y_coord, z_coord)
  /*  10 times not falling (x_coord max 2)
      1 time falling (class 0) ........: 3.5665, 0.8648, -0.3776
      REPEAT
  */
    memset(sensor_value, 0, 10);
    float value = 0.0;

    if (measurement_type == 0) { // fall
        //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
        //float value = strtof(sensor_value, NULL);
        // value = -0.121;
        snprintf(sensor_value, 10, "-%d.%d", (1+rand()%3), (rand()%10000));
        value = strtof(sensor_value, NULL);
    } else {
      snprintf(sensor_value, 10, "-%d.%d", 0, (rand()%10000));
      value = strtof(sensor_value, NULL);
    }
    //printf("sensor string: %s\n", sensor_value);
    //printf("sensor float: %f\n", value);

    return value;
}


static float read_33460()
{
  //Dummy:
  // 0 - start value (x_coord, y_coord, z_coord)
  /*  10 times not falling (x_coord max 2)
      1 time falling (class 0) ........: 3.5665, 0.8648, -0.3776
      REPEAT
  */
    memset(sensor_value, 0, 10);
    float value = 0.0;

    snprintf(sensor_value, 10, "%d.%d", (99 + rand() / (RAND_MAX / (20 - 99 + 1) + 1)), (rand()%10000));
    value = strtof(sensor_value, NULL);
    //printf("sensor string: %s\n", sensor_value);
    //printf("sensor float: %f\n", value);

    return value;
}


static float read_33250()
{
  //printf("\nRead 33250");
  memset(sensor_value, 0, 10);
  float value = 0.0;
  if (measurement_type == 0) { // fall
      //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
      //float value = strtof(sensor_value, NULL);
      // value = 0.980;
      snprintf(sensor_value, 10, "%d.%d", (100+rand()%100), (rand()%10000));
      value = strtof(sensor_value, NULL);
  } else {
    snprintf(sensor_value, 10, "%d.%d", (rand()%20), (rand()%10000));
    value = strtof(sensor_value, NULL);
  }

  // snprintf(sensor_value, 10, "%d.%d", (0+rand()%10), (rand()%10));
  // //printf("sensor string: %s\n", sensor_value);
  // float value = strtof(sensor_value, NULL);
  // //printf("sensor float: %f\n", value);
  return value;
}

static float read_33251()
{
  //printf("\nRead 33251");
  memset(sensor_value, 0, 10);
  float value = 0.0;
  if (measurement_type == 0) { // fall
      //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
      //float value = strtof(sensor_value, NULL);
      // value = 0.980;
      snprintf(sensor_value, 10, "%d.%d", (100+rand()%90), (rand()%10000));
      value = strtof(sensor_value, NULL);
  } else {
    snprintf(sensor_value, 10, "%d.%d", (rand()%20), (rand()%10000));
    value = strtof(sensor_value, NULL);
  }
  // snprintf(sensor_value, 10, "%d.%d", (10+rand()%5), (rand()%5));
  // //printf("sensor string: %s\n", sensor_value);
  // float value = strtof(sensor_value, NULL);
  // //printf("sensor float: %f\n", value);
  return value;
}

static float read_33252()
{
  //printf("\nRead 33252");
  memset(sensor_value, 0, 10);
  float value = 0.0;
  if (measurement_type == 0) { // fall
      //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
      //float value = strtof(sensor_value, NULL);
      // value = 0.980;
      snprintf(sensor_value, 10, "%d.%d", (rand()%1), (rand()%10000));
      value = strtof(sensor_value, NULL);
  } else {
    snprintf(sensor_value, 10, "%d.%d", 0, (rand()%10000));
    value = strtof(sensor_value, NULL);
  }
  // snprintf(sensor_value, 10, "%d.%d", (20+rand()%5), (rand()%5));
  // //printf("sensor string: %s\n", sensor_value);
  // float value = strtof(sensor_value, NULL);
  // //printf("sensor float: %f\n", value);
  return value;
}

static float read_33253()
{
  //printf("\nRead 33253");
  memset(sensor_value, 0, 10);
  float value = 0.0;
  if (measurement_type == 0) { // fall
      //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
      //float value = strtof(sensor_value, NULL);
      // value = 0.980;
      snprintf(sensor_value, 10, "%d.%d", (50+rand()%50), (rand()%10000));
      value = strtof(sensor_value, NULL);
  } else {
    snprintf(sensor_value, 10, "%d.%d", (rand()%2), (rand()%10000));
    value = strtof(sensor_value, NULL);
  }
  // snprintf(sensor_value, 10, "%d.%d", (30+rand()%5), (rand()%5));
  // //printf("sensor string: %s\n", sensor_value);
  // float value = strtof(sensor_value, NULL);
  // //printf("sensor float: %.2f\n", value);
  return value;
}

static float read_dummy()
{
  memset(sensor_value, 0, 10);
  float value = 0.0;
  if (measurement_type == 0) { // fall
      snprintf(sensor_value, 10, "%d.%d", (50+rand()%50), (rand()%10000));
      value = strtof(sensor_value, NULL);
  } else {
    snprintf(sensor_value, 10, "%d.%d", (rand()%2), (rand()%10000));
    value = strtof(sensor_value, NULL);
  }
  return value;
}

static float read_33254()
{
  float value = 0.0;
  value = read_dummy();
  return value;
}

static float read_33255()
{
  float value = 0.0;
  value = read_dummy();
  return value;
}

static float read_33256()
{
  float value = 0.0;
  value = read_dummy();
  return value;
}

static float read_33257()
{
  float value = 0.0;
  value = read_dummy();
  return value;
}

static float read_33258()
{
  float value = 0.0;
  value = read_dummy();
  return value;
}

static float read_33259()
{
  float value = 0.0;
  value = read_dummy();
  return value;
}





/*---------------------------------------------------------------------------*/














/*---------------------------------------------------------------------------*/
//reduzir tamanho da RAM
//PROCESS(mqtt_client_process, "LWPubSub-FedSensor-LWAIoT - MQTT client process");
PROCESS(mqtt_client_process, "LWPubSub-FedSensor");
/*---------------------------------------------------------------------------*/
static bool
have_connectivity(void)
{
  if(uip_ds6_get_global(ADDR_PREFERRED) == NULL ||
     uip_ds6_defrt_choose() == NULL) {
    return false;
  }
  return true;
}
/*---------------------------------------------------------------------------*/
/*
static int
ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
{
  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}
*/
/*---------------------------------------------------------------------------*/
static void
echo_reply_handler(uip_ipaddr_t *source, uint8_t ttl, uint8_t *data,
                   uint16_t datalen)
{
  if(uip_ip6addr_cmp(source, uip_ds6_defrt_choose())) {
    def_rt_rssi = (int16_t)uipbuf_get_attr(UIPBUF_ATTR_RSSI);
  }
}
/*---------------------------------------------------------------------------*/
static void
publish_led_off(void *d)
{
  leds_off(MQTT_CLIENT_STATUS_LED);
}
/*---------------------------------------------------------------------------*/






static void
publish(int is_measurement)
{

#if (ENERGEST_CONF_ON == 1)
  energest_flush();
  LOG_INFO("Get_measurement start ");
  printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
    energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
    energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
#endif /* ENERGEST_CONF_ON */

#if BOARD_SENSORTAG
  if ( (sensorList[0] == 33130) && (sensorList[1] == 33131) && (sensorList[2] == 33132) ) { //real accelerometer data
    init_mpu_reading(NULL);
  }

  if(strncmp(objectID, "03303", 5) == 0) { //commandReceived request measurement
    get_tmp_reading();
  }

  if(strncmp(objectID, "03304", 5) == 0) { //commandReceived request measurement
    get_hdc_reading();
  }
#endif

  LOG_INFO("RTIMER_NOW: %lu\n", RTIMER_NOW());

  //printf("\nTomada de decisão a seguir... \n");

  if (measurement_type > 4) {
      measurement_type = 0;
  }

  //Tomar a decisão
  //Pegar a medição

  //Limpa a variável new_observation
  int i = 0;
  for (i = 0; i < 10; i++) {
    new_observation[i] = 0.0;
  }
  i = 0;

  for (i = 0; i < sensorsNumber; i++) {

    switch(sensorList[i]) {
        case 33130: {
          new_observation[i] = read_33130();
          break;
        }

        case 33131: {
          new_observation[i] = read_33131();
          break;
        }

        case 33132: {
          new_observation[i] = read_33132();
          break;
        }

        case 33250: {
          new_observation[i] = read_33250();
          break;
        }

        case 33251: {
          new_observation[i] = read_33251();
          break;
        }

        case 33252: {
          new_observation[i] = read_33252();
          break;
        }

        case 33253: {
          new_observation[i] = read_33253();
          break;
        }

        case 33254: {
          new_observation[i] = read_33254();
          break;
        }

        case 33255: {
          new_observation[i] = read_33255();
          break;
        }

        case 33256: {
          new_observation[i] = read_33256();
          break;
        }

        case 33257: {
          new_observation[i] = read_33257();
          break;
        }

        case 33258: {
          new_observation[i] = read_33258();
          break;
        }

        case 33259: {
          new_observation[i] = read_33259();
          break;
        }

        case 33460: {
          new_observation[i] = read_33460();
          break;
        }

        default: {
          LOG_INFO("No sensor in the list of cases: break case!");
          break;
        }
    }

  }

#if BOARD_SENSORTAG
  if ( (sensorList[0] == 33130) && (sensorList[1] == 33131) && (sensorList[2] == 33132) ) { //real accelerometer data
    LOG_INFO("- Get MPU reading - \n");
    get_mpu_reading();
  }
#endif

  LOG_INFO("New observation/measurement collected: \n");
  for (i = 0; i < sensorsNumber; i++) {
    printf("%lu: %.4f - ", sensorList[i], new_observation[i]);
    char reading[10] = "\0";
    printf("as string: %s\n", _float_to_char(new_observation[i], reading));
  }

#if (ENERGEST_CONF_ON == 1)
  energest_flush();
  LOG_INFO("Get_measurement finish ");
  printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
    energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
    energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
#endif /* ENERGEST_CONF_ON */

  //Make prediction
  //Call the algorithm




  switch(strtol(objectID, NULL, 10)) {

      case 32102: {
        linreg_predict();
        break;
      }

      case 32103: {
        logreg_predict();
        break;
      }

      case 32106: {
        kmeans_predict();
        break;
      }

      default: {
        LOG_INFO("Not defined or incorrect ML model\n");
        break;
      }
  }

  measurement_type++;

  if (is_measurement == 1) {

    if (!publish_from_command) {
      #if (ENERGEST_CONF_ON == 1)
        energest_flush();
        LOG_INFO("Publish start ");
        printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
          energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
          energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
      #endif /* ENERGEST_CONF_ON */
    }

    /* LWPubSub System for IoT */
    int len;
    int remaining = APP_BUFFER_SIZE;
    int payload_size = 0;
    int pub_topic_size = strlen(pub_topic);
    #if MQTT_5
      static uint8_t prop_err = 1;
    #endif

    seq_nr_value++;

    buf_ptr = app_buffer;

    //LWPubSub - Experiment uses only temperature objectID/instanceID
    // if(LWPUBSUB_IS_ENCRYPTED) {
    //   len = snprintf(buf_ptr, remaining, "33030|");
    // } else {
    //   len = snprintf(buf_ptr, remaining, "033030|");
    // }

    len = snprintf(buf_ptr, remaining, "%s", objectID);

    if(len < 0 || len >= remaining) {
      LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining,
              len);
      return;
    }

    remaining -= len;
    buf_ptr += len;
    payload_size += len;

    len = snprintf(buf_ptr, remaining, "%s", instanceID);

    if(len < 0 || len >= remaining) {
      LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining,
              len);
      return;
    }

    remaining -= len;
    buf_ptr += len;
    payload_size += len;

    len = snprintf(buf_ptr, remaining, "|");

    if(len < 0 || len >= remaining) {
      LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining,
              len);
      return;
    }

    remaining -= len;
    buf_ptr += len;
    payload_size += len;

    // //LWPubSub - check board/extensions
    // if (mqtt_client_extension_count != 0) {
    //   len = snprintf(buf_ptr, remaining, "%s", mqtt_client_extensions[0]->value());
    //   LOG_DBG("Client has extensions\n");
    // } else {
    //   // Native target - send dummy measurement
    //   len = snprintf(buf_ptr, remaining, "%d.%d", (10+rand()%10), (rand()%10));
    // }

#if BOARD_SENSORTAG
    len = snprintf(buf_ptr, remaining, "%d.%02d", value_measurement / 100, value_measurement % 100);
#else
    len = snprintf(buf_ptr, remaining, "%d.%d", (10+rand()%10), (rand()%10));
#endif

    if(len < 0 || len >= remaining) {
      LOG_ERR("Buffer too short. Have %d, need %d + \\0\n", remaining,
              len);
      return;
    }

    remaining -= len;
    buf_ptr += len;
    payload_size += len;

    generate_nonce();

    #if (LOG_LEVEL == LOG_LEVEL_DBG)
      //LOG_DBG("\nIV: "); dump(iv, 13); printf("\n");
      LOG_DBG(" *** Pre-encryption: ***\n");
      LOG_DBG("Key: %s\n", key);
      LOG_DBG("Nonce: %s, nonce len: %d\n", nonce, NONCE_LEN);
      LOG_DBG("Pub_topic: %s, pub_topic_size: %d\n", pub_topic, pub_topic_size);
      LOG_DBG("App_Buffer.: %s, app_buffer_len: %d\n", app_buffer, payload_size); //printf("\n");

    #endif /* IF LOG_LEVEL_DBG */


    #if (LOG_LEVEL == LOG_LEVEL_DBG)
    #endif  /* IF LOG_LEVEL_DBG */


    static uint8_t nonce_bytes[NONCE_LEN];
    hexconv_unhexlify(nonce, strlen(nonce), nonce_bytes, sizeof(nonce_bytes));

    hexconv_unhexlify(key, strlen(key), key_bytes, sizeof(key_bytes));

    char hdr_hex[50] = {0};
    char cleartext_hex[50] = {0};

    //printf("\n\nPayload size: %d\n", payload_size);
    //printf("\n\nApp buffer: %s\n", app_buffer);
    //printf("\n\nPub topic : %s\n", pub_topic);

    for (int i = 0, j = 0; i < pub_topic_size; ++i, j += 2)
    {
      sprintf(hdr_hex + j, "%02x", pub_topic[i] & 0xff);
    }

    for (int i = 0, j = 0; i < payload_size; ++i, j += 2)
    {
      sprintf(cleartext_hex + j, "%02x", app_buffer[i] & 0xff);
    }


    //https://stackoverflow.com/questions/46210513/how-to-convert-a-string-to-hex-and-vice-versa-in-c
    //printf("'%s' in hex is %s.\n", cleartext_string, cleartext_hex);

    uint8_t buffer[100] = {0};
    size_t a_len = strlen(hdr_hex) / 2;
    size_t m_len = strlen(cleartext_hex) / 2;
    hexconv_unhexlify(hdr_hex, strlen(hdr_hex), buffer, sizeof(buffer));
    hexconv_unhexlify(cleartext_hex, strlen(cleartext_hex), buffer + a_len, sizeof(buffer) - a_len);


    //printf("TEST: encrypt in: %u + %u bytes\n", (unsigned)a_len, (unsigned)m_len);

    #if (ENERGEST_CONF_ON == 1)
      energest_flush();
      LOG_INFO("EncryptCCM start ");
      printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
        energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
        energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
    #endif /* ENERGEST_CONF_ON */

    CCM_STAR.set_key(key_bytes);
    CCM_STAR.aead(
        nonce_bytes,
        buffer + a_len,
        m_len,
        buffer,
        a_len,
        buffer + a_len + m_len,
        MICLEN,
        1
    );

    #if (ENERGEST_CONF_ON == 1)
      energest_flush();
      LOG_INFO("EncryptCCM finish ");
      printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
        energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
        energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
    #endif /* ENERGEST_CONF_ON */

    uint8_t ciphertext[payload_size];
    for (int i = 0; i < payload_size; i++) {
    	ciphertext[i] = buffer[i+pub_topic_size]; //descarto os primeiros 16 pq é o header (pub_topic)
    }

    uint8_t mic[MICLEN];
    for (int i = 0; i < MICLEN; i++) {
    	mic[i] = buffer[i+payload_size+pub_topic_size]; //descarto os primeiros 16 pq é o header
    }



    #if (LOG_LEVEL == LOG_LEVEL_DBG)
      LOG_DBG("Buffer after enc 1: "); dump(buffer, 100); printf("\n");
      LOG_DBG("Ciphertext: "); dump(ciphertext, payload_size); printf("\n");
      LOG_DBG("MIC: "); dump(mic, MICLEN); printf("\n");
    #endif /* IF LOG_LEVEL_DBG */


    // ATENÇÃO!!!!!!
    // O que falta fazer: preencher o hdr e o cleartext, verificar o tamanho do hdr
    // e do cleartext para na hora de compor o appbuffer já ter os tamanhos certos
    // Depois mexer na função que recebe isso lá no node.js (IoT Agent LWPubSub)



    /* Secure MQTT Payload construction
       App_buffer (LWPubSub):
         1st byte: encryption type
         2nd to 17th byte: IV
         18th to end: encrypted msg
    */

    if(LWPUBSUB_IS_ENCRYPTED) {

      // int i = 0;
      // int j = 0;

      uint8_t i = 0;
      uint8_t j = 0;

      for (i = 0; i < NONCE_LEN; i++) {
        //app_buffer[i] = iv[i];
        app_buffer[i] = nonce_bytes[i];
      }

      for (i = NONCE_LEN, j = 0; i < NONCE_LEN+MICLEN; i++, j++) {
        app_buffer[i] = mic[j];
      }

      for (i = NONCE_LEN+MICLEN, j = 0; i < NONCE_LEN+MICLEN+payload_size; i++, j++) {
        app_buffer[i] = ciphertext[j];
      }

    }

  #if (LOG_LEVEL == LOG_LEVEL_DBG)
    LOG_DBG("App_Buffer.: "); dump((uint8_t *)app_buffer, (NONCE_LEN+MICLEN+payload_size)); printf("\n");
  #endif

    //LWPubSub:
    //1. 1st byte:
    //   00 - no security
    //   01 - AES-CBC-128
    //   02 - AES-CBC-192
    //   03 - AES-CBC-256
    //   11 - AES-CTR-128
    //   12 - AES-CTR-192
    //   13 - AES-CTR-256
    //2. Cloud platform IoT Agent:
    //   Byte  1 --------- (security algorithm) +
    //   Bytes 2 a 17 ---- (random IV) +
    //   Bytes 18 a 64  -- (MQTT payload - LWSec PubSub System for IoT message)

    //GaD

    if(LWPUBSUB_IS_ENCRYPTED) {
      mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
                   (NONCE_LEN+MICLEN+payload_size), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
    } else {
      mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
                   strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);
    }

    LOG_DBG("Publish!\n");

    printf("RTIMER_NOW: %lu\n", RTIMER_NOW());


  }
}














/*---------------------------------------------------------------------------*/
static void
pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
            uint16_t chunk_len)
{
  LOG_DBG("Pub Handler: topic='%s' (len=%u), chunk_len=%u, chunk='%s'\n", topic,
          topic_len, chunk_len, chunk);


  /* If we don't like the length, ignore */
  /* Fixed topic_len for the article experiment */
  if(topic_len != 20) {
  //if(topic_len != 20) {
    //LWPubSub message has at least 16 bytes, due to use of AES on encryption
    LOG_ERR("Incorrect topic or chunk len. Ignored\n");
    return;
  }

  /* If the command != cmd, ignore */
  if(strncmp(&topic[topic_len - 3], "cmd", 3) != 0) {
    LOG_ERR("Incorrect command format\n");
  }

  if(strncmp(&topic[17], "cmd", 3) == 0) {
    //We accept only cmd last topic part

    #if (LOG_LEVEL == LOG_LEVEL_DBG)
      LOG_DBG("Received message - Chunk: "); dump((uint8_t *)chunk, chunk_len); printf("\n");
      //printf("LWPUBSUB_CRYPTO_MODE: %02X\n", (char)app_buffer[0]);
    #endif

    //Get only the need aad (/99/<devicdID>), discard ("/cmd") at the end
    char aad[LWPUBSUB_TOPIC_SIZE] = "0";
    strncpy(aad, topic, LWPUBSUB_TOPIC_SIZE);

    // LWPubSub Message:
    // NONCE (13 bytes)
    // MIC (8 bytes)
    // CIPHERTEXT (variable size)

    hexconv_unhexlify(key, strlen(key), key_bytes, sizeof(key_bytes));

    uint8_t received_nonce[NONCE_LEN];
    uint8_t received_tag[MICLEN];
    int received_message_len = chunk_len-NONCE_LEN-MICLEN;
    uint8_t received_message[received_message_len];
    //uint8_t received_message[APP_BUFFER_SIZE/2];
    //uint8_t received_message[200];
    LOG_INFO("Received message len: %d\n", received_message_len);

    /* ***** Print the payload ****** */

    /* Split message */
    int i = 0;
    int j = 0;
    for (i = 0; i < NONCE_LEN; i++) {
      received_nonce[i] = chunk[i];
    }

    for (i = NONCE_LEN, j = 0; i < NONCE_LEN+MICLEN; i++, j++) {
      received_tag[j] = chunk[i];
    }

    for (i = NONCE_LEN+MICLEN, j = 0; i < NONCE_LEN+MICLEN+received_message_len; i++, j++) {
      received_message[j] = chunk[i];
    }

    #if (LOG_LEVEL == LOG_LEVEL_DBG)
      LOG_DBG("Defined AAD: "); dump((uint8_t *)aad, 16); printf("\n");
      LOG_DBG("Received Nonce: "); dump(received_nonce, NONCE_LEN); printf("\n");
      LOG_DBG("Received tag: "); dump(received_tag, MICLEN); printf("\n");
      LOG_DBG("Received Encrypted LWPubSub message (AES-CCM-8): "); dump(received_message, received_message_len); printf("\n");
    #endif

    // DECRYPT!

    //bool success;
    bool auth_check;
    uint8_t generated_mic[MICLEN];
    uint8_t buffer[LWPUBSUB_TOPIC_SIZE+received_message_len+MICLEN];
    //uint8_t hdr_bytes[topic_len * 2];
    //uint8_t cleartext_bytes[received_message_len];

    char hdr_hex[50] = {0};
    for (i = 0, j = 0; i < LWPUBSUB_TOPIC_SIZE; ++i, j += 2)
    {
      sprintf(hdr_hex + j, "%02x", topic[i] & 0xff);
    }

    size_t a_len = LWPUBSUB_TOPIC_SIZE;
    size_t m_len = received_message_len;

    // Como deve ser construído o buffer para decriptar:
    // topic + ciphertext + tag

    //buffer = topic
    hexconv_unhexlify(hdr_hex, strlen(hdr_hex), buffer, sizeof(buffer));

    //buffer = buffer + ciphertext
    for (i = LWPUBSUB_TOPIC_SIZE, j = 0; i < LWPUBSUB_TOPIC_SIZE+received_message_len; i++, j++) {
      buffer[i] = received_message[j];
    }

    //buffer = buffer + tag
    for (i = LWPUBSUB_TOPIC_SIZE+received_message_len, j = 0; i < LWPUBSUB_TOPIC_SIZE+received_message_len+MICLEN; i++, j++) {
      buffer[i] = received_tag[j];
    }
    buffer[i] = '\0';

    #if (LOG_LEVEL == LOG_LEVEL_DBG)
      LOG_DBG("hdr_hex: %s\n", hdr_hex);
      LOG_DBG("Buffer: "); dump(buffer, LWPUBSUB_TOPIC_SIZE+received_message_len+MICLEN); printf("\n");
    #endif

    #if (ENERGEST_CONF_ON == 1)
      energest_flush();
      LOG_INFO("DecryptCCM start ");
      printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
        energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
        energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
    #endif /* ENERGEST_CONF_ON */

    CCM_STAR.set_key(key_bytes);
    CCM_STAR.aead(
        received_nonce,
        buffer + a_len,
        m_len,
        buffer,
        a_len,
        generated_mic,
        MICLEN,
        0
    );

    #if (ENERGEST_CONF_ON == 1)
      energest_flush();
      LOG_INFO("DecryptCCM finish ");
      printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
        energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
        energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
    #endif /* ENERGEST_CONF_ON */

    auth_check = !memcmp(generated_mic, buffer + a_len + m_len, MICLEN);
    LOG_INFO("Message AuthCheck: %s\n", auth_check ? "OK" : "FAIL");

    if (!auth_check) {
      LOG_ERR("Wrong authentication tag.\n");
      return;
    }

    /* ***** Print the decrypted payload ****** */

    #if (LOG_LEVEL == LOG_LEVEL_DBG)
      LOG_DBG("Buffer: "); dump(buffer, LWPUBSUB_TOPIC_SIZE+received_message_len+MICLEN); printf("\n");
    #endif

    // Topic has 16 Bytes. Plaintext start at byte 17
    uint8_t received_plaintext[received_message_len];
    //uint8_t received_plaintext[APP_BUFFER_SIZE] = {0};
    for (i = LWPUBSUB_TOPIC_SIZE, j = 0; i < LWPUBSUB_TOPIC_SIZE+received_message_len; i++, j++) {
      received_plaintext[j] = buffer[i];
    }
    received_plaintext[j] = '\0';

    #if (LOG_LEVEL == LOG_LEVEL_DBG)
      LOG_DBG("Received LWPubSub - Plaintext: "); dump(received_plaintext, received_message_len); printf("\n");
    #endif

    parsePayload(received_plaintext, received_message_len);

    #if (LOG_LEVEL == LOG_LEVEL_DBG)
      LOG_DBG("Chunk - objectID: %s\n", objectID);
      LOG_DBG("Chunk - instanceID: %c\n", instanceID[0]);
      LOG_DBG("Chunk - commandReceived: %s\n", commandReceived);
      LOG_DBG("Chunk - commandReceivedlen: %d\n\n", commandReceivedlen);

    #endif
    //http://www.openmobilealliance.org/wp/OMNA/LwM2M/LwM2MRegistry.html

    char temp_ml_model[6] = {0}; //Null char! Can't be 5
    strncpy(temp_ml_model, objectID, 5);
    temp_ml_model[5] = '\0';
    ml_model = (int)strtol(temp_ml_model, NULL, 10);
    LOG_DBG("ML Model configured: %d \n", ml_model);



    //*******************************************************************
    //Preencher action - sempre passando 3 casas inteiras
    for (i = 0; i < 3; i++) {
      temp[i] = commandReceived[i];
    }
    //printf("\n\ni do action: %d\n\n", i);
    temp[i] = '\0';
    action = (int)strtol(temp, NULL, 10);
    LOG_DBG("Action when: %d\n\n", action);
    //*******************************************************************


    if( (strncmp(objectID, "32001", 5) == 0) ) {
      LOG_INFO("Solicitou 32001 - config. sensores para ativar e poll frequency.\n");

      #if (ENERGEST_CONF_ON == 1)
        energest_flush();
        LOG_INFO("MLModel 32001 start ");
        printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
          energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
          energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
      #endif /* ENERGEST_CONF_ON */

      //Parse 32001
      parse32001(); //first LWAIoT message

      #if (ENERGEST_CONF_ON == 1)
        energest_flush();
        LOG_INFO("MLModel 32001 finish %d sensors ", sensorsNumber);
        printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
          energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
          energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
      #endif /* ENERGEST_CONF_ON */
      return;

    } else if ( (strncmp(objectID, "32002", 5) == 0) ) {
      LOG_INFO("NÃO EXISTE MAIS ISSO! - Solicitou 32002 - config. sensor para disparar ação.\n");
      //parse32002(commandReceived);
      return;








    } else if( (strncmp(objectID, "32102", 5) == 0) ) {
        LOG_INFO("--------------- FedSensor - LWPubSub[LWAIoT - Linear regression] ---------------\n");
        #if (ENERGEST_CONF_ON == 1)
          energest_flush();
          LOG_INFO("MLModel %d setup start ", ml_model);
          printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
            energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
            energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
        #endif /* ENERGEST_CONF_ON */

        //Mensagem:
        //"250;1|16.0126;2|0.8444#0.4488#8.0331#0.2608"
        int n = 0;
        lin_bias = 0.0;
        for (n = 0; n < 10; n++) {
            lin_weights[n] = 0.0;
        }
        n = 0;

        i = 0;
        j = 0;
        for (i = 6; i < commandReceivedlen; i++) {
          if ( (commandReceived[i] != pipe[0]) && (commandReceived[i] != semic[0]) && (commandReceived[i-1] != semic[0]) && (commandReceived[i] != hashtag[0]) ) {
              //printf("\ncommandReceived[%d]: %c", i, commandReceived[i]);
              valuereceived[j] = commandReceived[i];
              j++;
              //printf("valuereceived[%d]: %s\n", j, valuereceived);
          }

          if ( commandReceived[i] == semic[0] ) { //passou por um ponto e vírgula e, independentemente de ser o primeiro ou não, não é mais bias
              lin_bias = strtof(valuereceived, NULL);
              //printf("bias: %.4f\n", lin_bias);
              memset(valuereceived, 0, 10);
              valuereceived[9] = '\0';
              j = 0;
          }

          if ( (commandReceived[i] == hashtag[0]) || (i == commandReceivedlen-1) ) { //armazena o valor
              lin_weights[n] = strtof(valuereceived, NULL);
              printf("lin_weights[%d]: %.4f\n", n, lin_weights[n]);
              n++;
              j = 0;
              memset(valuereceived, 0, 10);
              valuereceived[9] = '\0';
          }

        }

        LOG_INFO("Maximum acceptable value: %d\n", action);

        LOG_INFO("Bias: [%.4f] ", lin_bias);

        //sensorsNumber = 4; //only for testing!
        LOG_INFO_("\n");
        LOG_INFO("Vetor de weights:");
        for (int j = 0; j < sensorsNumber; j++ ) {
          LOG_INFO_("[%.4f] ", lin_weights[j]);
        }
        LOG_INFO_("\n");
        LOG_INFO("--------------------------------------------------------------------------------\n");

        #if (ENERGEST_CONF_ON == 1)
          energest_flush();
          LOG_INFO("MLModel %d setup finish %d value ", ml_model, action);
          printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
            energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
            energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
        #endif /* ENERGEST_CONF_ON */

        return;











    } else if( (strncmp(objectID, "32103", 5) == 0) ) {
        LOG_INFO("-------------- FedSensor - LWPubSub[LWAIoT - Logistic regression] --------------\n");
        #if (ENERGEST_CONF_ON == 1)
          energest_flush();
          LOG_INFO("MLModel %d setup start ", ml_model);
          printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
            energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
            energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
        #endif /* ENERGEST_CONF_ON */


        //*******************************************************************

        //Mensagem:
        //002;1|10.8151#1.4182#-12.2333;2|-0.0636#-0.0477#-1.0038#-0.0266;3|-0.0156#0.0121#0.1495#0.0092;4|0.0792#0.0356#0.8543#0.0174

        //instanceID 1 é o bias
        //1|10.8151#1.4182#-12.2333 ->    classe 0: [10.8151]    classe 1: [1.4182]    classe 2: [-12.2333]
        //então, um primeiro 'for' para preencher o vetor de 'bias'

        //instanceID 2 até o fim são os weights
        //2|-0.0636#-0.0477#-1.0038#-0.0266;3|-0.0156#0.0121#0.1495#0.0092;4|0.0792#0.0356#0.8543#0.0174
        //instanceID 2 é a classe 0: [-0.0636],  [-0.0477],  [-1.0038],  [-0.0266]
        //instanceID 3 é a classe 1: [-0.0156],  [0.0121],   [0.1495],   [0.0092]
        //instanceID 4 é a classe 2: [0.0792],   [0.0356],   [0.8543],   [0.0174]

        //new_observation[] = [30.39, 89.86, 0.86, 42.86]

        /*Em resumo:
          Um vetor de 10 posições de bias: .............................................. bias[10]
          Uma matriz 10 x 10 de weights: ................................................ weights[10][10]
          Um vetor de 10 posições das novas medições: ................................... new_observation[10]
          Uma matriz 10 x 10 de resultado da multiplicação das medições vezes weitghs: .. mult_values_weights[10]
          Tem que identificar o número de classes que é enviado no bias e, depois, esperar que os instanceID
          seguintes mostrem os weights do número de classes esperado
        */

        int is_bias = 1;
        int m = 0;
        int n = 0;

        //Inicialização das variáveis bias[10] e weights[10][10]
        for (m = 0; m < 10; m++) {
          for (n = 0; n < 10; n++) {
            weights[m][n] = 0.0;
          }
          bias[m] = 0.0;
        }
        m = 0;
        n = 0;


        //i começa em 6, porque passa o action e o índice do bias (que é 1)
        //002;1|10.8151#1.4182#-12.2333;2|-0.0636#-0.0477#-1.0038#-0.0266;3|-0.0156#0.0121#0.1495#0.0092;4|0.0792#0.0356#0.8543#0.0174

        //is_bias = 1; //já começo no bias no byte 5
        i = 0;
        j = 0; // controla o índice do valor a ser recebido no char valuereceived[] que será valuereceived[j], o certo é não usar a variável column
        for (i = 6; i < commandReceivedlen; i++) {
          //if  ( (strncmp(commandReceived[i], "1", 1) == 0) && (commandReceived[i-1] == semic[0]) && (commandReceived[i+1] == pipe[0]) ) {
          //É instanceID 1, e não qualquer número 1 então preencher o bias

          if ( (commandReceived[i] != pipe[0]) && (commandReceived[i] != semic[0]) && (commandReceived[i-1] != semic[0]) && (commandReceived[i] != hashtag[0]) ) {
            //printf("\ncommandReceived[%d]: %c", i, commandReceived[i]);
            // printf("line: %d", line);
            // printf("column: %d\n", column);
            valuereceived[j] = commandReceived[i];
            j++;
            //printf("valuereceived[%d]: %s\n", j, valuereceived);
          }

          if ( (commandReceived[i] == hashtag[0]) || (i == commandReceivedlen-1) ) { //pula pra próxima coluna
            //printf("entrou hashtag\n");
            if (is_bias == 1) {
              bias[n] = strtof(valuereceived, NULL);
              //printf(" ....> bias[%d]: %.4f\n", n, bias[n]);
            } else {
              weights[m][n] = strtof(valuereceived, NULL);
              //printf(" ....> weights[%d][%d]: %.4f ", m, n, weights[m][n]);
            }
            n++;
            j = 0;
            memset(valuereceived, 0, 10);
            valuereceived[9] = '\0';
          }

          if ( commandReceived[i] == semic[0] ) { //passou por um ponto e vírgula e, independentemente de ser o primeiro ou não, não é mais bias
            //printf("entrou semic\n");
            //if ( (strncmp(commandReceived[i+1], "2", 1) == 0) ) { //acabou bias, vai começar weights
            if ( (commandReceived[i+1] == dois[0]) ) { //acabou bias, vai começar weights
              //printf("\n\n instanceID é 2! \n\n");
              bias[n] = strtof(valuereceived, NULL);
              //printf(" ....> bias[%d]: %.4f \n-- end bias\nStart weights:\n", n, bias[n]);
              is_bias = 0;
              number_of_classes = n + 1;
            } else {
              weights[m][n] = strtof(valuereceived, NULL);
              //printf(" ....> weights[%d][%d]: %.4f\n", m, n, weights[m][n]);
              m++;
            }
            n = 0;
            j = 0;
            memset(valuereceived, 0, 10);
            valuereceived[9] = '\0';
          }

        }


        LOG_INFO("Number of classes: %d\n", number_of_classes);


        LOG_INFO("Vetor de bias: ");
        for (int i = 0; i < number_of_classes ; i++) {
          LOG_INFO_("[%.4f] ", bias[i]);
        }

        //sensorsNumber = 4; //only for testing!
        LOG_INFO_("\n");
        LOG_INFO("Matriz de weights:");
        for (int i = 0; i < number_of_classes; i++ ) {
          LOG_INFO_("\nClass [%d]: ", i);
          for (int j = 0; j < sensorsNumber; j++ ) {
            LOG_INFO_("[%.4f] ", weights[i][j]);
          }
        }
        LOG_INFO_("\n");
        LOG_INFO("--------------------------------------------------------------------------------\n");

        #if (ENERGEST_CONF_ON == 1)
          energest_flush();
          LOG_INFO("MLModel %d setup finish %d classes ", ml_model, number_of_classes);
          printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
            energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
            energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
        #endif /* ENERGEST_CONF_ON */

        return;














    } else if( (strncmp(objectID, "32106", 5) == 0) ) {
          LOG_INFO("-------------------- FedSensor - LWPubSub[LWAIoT - K-means] --------------------\n");
          #if (ENERGEST_CONF_ON == 1)
            energest_flush();
            LOG_INFO("MLModel %d setup start ", ml_model);
            printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
              energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
              energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
          #endif /* ENERGEST_CONF_ON */

          // LOG_DBG("\nNew observation: ");
          // for (int i = 0; i < 10; i++) {
          //   LOG_DBG("[%.2f]", new_observation[i]);
          // }
          // LOG_DBG("\n");

          // não fazer predict aqui, esperar o tempo

          //kmeans_predict(); //K-means LWAIoT message
          LOG_INFO("Centroids: \n");

          line = 0;
          column = 0;
          m = 0;
          number_of_centroids = 0;
          memset(centroids, 0, sizeof(centroids));

          for (i = 6; i < commandReceivedlen; i++) {
            if ( (commandReceived[i] != pipe[0]) && (commandReceived[i] != semic[0]) && (commandReceived[i-1] != semic[0]) && (commandReceived[i] != hashtag[0]) ) {
              valuereceived[m] = commandReceived[i];
              m++;
            }

            if ( (commandReceived[i] == semic[0]) ) { //pula pra próxima linha
              centroids[line][column] = strtof(valuereceived, NULL);
              LOG_INFO_("[%d][%d]: %.4f\n", line, column, centroids[line][column]);
              line++;
              column = 0;
              m = 0;
              memset(valuereceived, 0, 10);
              valuereceived[9] = '\0';
            }

            if ( (commandReceived[i] == hashtag[0]) || (i == commandReceivedlen-1) ) { //pula pra próxima coluna
              centroids[line][column] = strtof(valuereceived, NULL);
              LOG_INFO_("[%d][%d]: %.4f     ", line, column, centroids[line][column]);
              column++;
              m = 0;
              memset(valuereceived, 0, 10);
              valuereceived[9] = '\0';
            }
          }

          number_of_centroids = line + 1;
          LOG_INFO_("\n");
          LOG_INFO("Number of centroids: %d\n", number_of_centroids);

          LOG_INFO("--------------------------------------------------------------------------------\n");
          #if (ENERGEST_CONF_ON == 1)
            energest_flush();
            LOG_INFO("MLModel %d setup finish %d centroids ", ml_model, number_of_centroids);
            printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
              energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
              energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
          #endif /* ENERGEST_CONF_ON */

          return;

    } else if( (strncmp(objectID, "32101", 5) == 0) ||
               (strncmp(objectID, "32102", 5) == 0) ||
               (strncmp(objectID, "32103", 5) == 0) ||
               (strncmp(objectID, "32104", 5) == 0) ||
               (strncmp(objectID, "32105", 5) == 0) ) {
        LOG_DBG("\nSolicitou outro MODELO DE ML (32101, 32102, 32104, 32105) - config. modelo de ML.\n\n");

        return;

    }

//3311 - Start
    if(strncmp(objectID, "03311", 5) == 0) { //IPSO Light Control
      if(strncmp(instanceID, "0", 1) == 0) { //LED vermelho
        if(strncmp(commandReceived, "1", 1) == 0) {
          LOG_INFO("CommandReceived type execute - red led (3311 0)\n");
          printf("\n> RED Led On <\n\n");
          leds_on(LEDS_RED);
        } else if(strncmp(commandReceived, "0", 1) == 0) {
          LOG_INFO("CommandReceived type execute - red led (3311 0)\n");
          printf("\n> RED Led Off <\n\n");
          leds_off(LEDS_RED);
        } else {
          LOG_ERR("--Wrong command--\n");
        }

      } else if(strncmp(instanceID, "1", 1) == 0) { //LED verde {
        if(strncmp(commandReceived, "1", 1) == 0) {
          LOG_INFO("CommandReceived type execute - green led (3311 1)\n");
          printf("\n> GREEN Led On <\n\n");
          leds_on(LEDS_GREEN);
        } else if(strncmp(commandReceived, "0", 1) == 0) {
          LOG_INFO("CommandReceived type execute - green led (3311 1)\n");
          printf("\n> GREEN Led Off <\n\n");
          leds_off(LEDS_GREEN);
        } else {
          LOG_ERR("--Wrong command--\n");
        }

      } else if(strncmp(instanceID, "2", 1) == 0) { //LED azul {
        if(strncmp(commandReceived, "1", 1) == 0) {
          LOG_INFO("CommandReceived type execute - blue led (3311 2)\n");
          printf("\n> BLUE Led On <\n\n");
          leds_on(LEDS_BLUE);
        } else if(strncmp(commandReceived, "0", 1) == 0) {
          LOG_INFO("CommandReceived type execute - blue led (3311 2)\n");
          printf("\n> BLUE Led Off <\n\n");
          leds_off(LEDS_BLUE);
        } else {
          LOG_ERR("--Wrong command--\n");
        }

      } else {
        LOG_ERR("Only leds: instanceID 0 - Red, instanceID 1 - Green, and instanceID 2 - Blue\n");
      }
//3311 - End

//3338 - Start
    } else if(strncmp(objectID, "03338", 5) == 0) { //IPSO Buzzer/Alarm
      if(strncmp(instanceID, "0", 1) == 0) { //The only buzzer in Sensortag
        if(strncmp(commandReceived, "1", 1) == 0) {
          LOG_INFO("CommandReceived type execute - buzzer (03338)\n");
          printf("\n> Buzzer On <\n\n");
#if BOARD_SENSORTAG
          buzzer_start(1000);
#endif
        } else if(strncmp(commandReceived, "0", 1) == 0) {
          LOG_INFO("CommandReceived type execute - buzzer (03338)\n");
          printf("\n> Buzzer Off <\n\n");
#if BOARD_SENSORTAG
          if(buzzer_state()) {
            buzzer_stop();
          }
#endif
        } else {
          LOG_ERR("--Wrong command--\n");
        }
      } else {
        LOG_ERR("Only one Buzzer instance - instanceID 0\n");
      }
//3338 - End

//3303 - Start
} else if(strncmp(objectID, "03303", 5) == 0) { //commandReceived request measurement
      if(strncmp(instanceID, "0", 1) == 0) { //The only temperature instance implemented
        if(strncmp(commandReceived, "0", 1) == 0) { //empty data
          LOG_INFO("CommandReceived type request - temperature (03303)\n");
          /* Fire publish from here! */
          LOG_INFO("Publish from commandReceived started\n");
          publish_from_command = 1;
#if BOARD_SENSORTAG
          init_tmp_reading(NULL);
#endif
          publish(1); //1 = is measurement
          LOG_INFO("Publish from commandReceived finished\n");
        } else {
          LOG_ERR("--Wrong commandReceived--\n");
        }
      } else {
        LOG_ERR("Only one temperature instance implemented - instanceID 0\n");
      }
//3303 - End

//3304 - Start
} else if(strncmp(objectID, "03304", 5) == 0) { //commandReceived request measurement
      if(strncmp(instanceID, "0", 1) == 0) { //The only temperature instance implemented
        if(strncmp(commandReceived, "0", 1) == 0) { //empty data
          LOG_INFO("CommandReceived type request - humidity (03304)\n");
          /* Fire publish from here! */
          LOG_INFO("Publish from commandReceived started\n");
          publish_from_command = 1;
#if BOARD_SENSORTAG
          init_hdc_reading(NULL);
#endif
          publish(1); //1 = is measurement
          LOG_INFO("Publish from commandReceived finished\n");
        } else {
          LOG_ERR("--Wrong commandReceived--\n");
        }
      } else {
        LOG_ERR("Only one humidity instance implemented - instanceID 0\n");
      }
//3304 - End

    } else {
      //LOG_ERR("Not implemented other objectID different than 03311 (led), 03338 (buzzer) and 03303 (temperature)!\n");
      LOG_DBG("ML model, not sent sensor in objectID\n");
    }
//IPSO 3311 and 3338 devices end

/* PENSAR!!!! Não mostrar energia de um commandReceived finish quando vi, porque no mqtt.c já tem um Publish finish
   Na análise do log observar pelo Publish finish e não commandReceived finish */
#if (ENERGEST_CONF_ON == 1)
    energest_flush();
    LOG_INFO("CommandReceived finish ");
    printf("E_CPU %llu E_LPM %llu E_DEEP_LPM %llu E_TX %llu E_RX %llu E_Total: %llu\n",
      energest_type_time(ENERGEST_TYPE_CPU), energest_type_time(ENERGEST_TYPE_LPM), energest_type_time(ENERGEST_TYPE_DEEP_LPM),
      energest_type_time(ENERGEST_TYPE_TRANSMIT), energest_type_time(ENERGEST_TYPE_LISTEN), ENERGEST_GET_TOTAL_TIME());
#endif /* ENERGEST_CONF_ON */

    publish_from_command = 0;

    return;

  } else {
    LOG_DBG("MQTT CommandReceived 'cmd' incorrect position\n");
    return;
  }

}



  // /* If we don't like the length, ignore */
  // if(topic_len != 23 || chunk_len != 1) {
  //   LOG_ERR("Incorrect topic or chunk len. Ignored\n");
  //   return;
  // }

  // /* If the format != json, ignore */
  // if(strncmp(&topic[topic_len - 4], "json", 4) != 0) {
  //   LOG_ERR("Incorrect format\n");
  // }

  // if(strncmp(&topic[10], "leds", 4) == 0) {
  //   LOG_DBG("Received MQTT SUB\n");
  //   if(chunk[0] == '1') {
  //     leds_on(LEDS_RED);
  //   } else if(chunk[0] == '0') {
  //     leds_off(LEDS_RED);
  //   }
  //   return;
  // }
// }
/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
  case MQTT_EVENT_CONNECTED: {
    LOG_DBG("Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    break;
  }
  case MQTT_EVENT_DISCONNECTED:
  case MQTT_EVENT_CONNECTION_REFUSED_ERROR: {
    LOG_DBG("MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

    state = STATE_DISCONNECTED;
    process_poll(&mqtt_client_process);
    break;
  }
  case MQTT_EVENT_PUBLISH: {
    msg_ptr = data;

    /* Implement first_flag in publish message? */
    if(msg_ptr->first_chunk) {
      msg_ptr->first_chunk = 0;
      LOG_DBG("Application received publish for topic '%s'. Payload "
              "size is %i bytes.\n", msg_ptr->topic, msg_ptr->payload_chunk_length);
    }

    pub_handler(msg_ptr->topic, strlen(msg_ptr->topic),
                msg_ptr->payload_chunk, msg_ptr->payload_chunk_length);
#if MQTT_5
    /* Print any properties received along with the message */
    mqtt_prop_print_input_props(m);
#endif
    break;
  }
  case MQTT_EVENT_SUBACK: {
#if MQTT_31
    LOG_DBG("Application is subscribed to topic successfully\n");
#else
    struct mqtt_suback_event *suback_event = (struct mqtt_suback_event *)data;

    if(suback_event->success) {
      LOG_DBG("Application is subscribed to topic successfully\n");
    } else {
      LOG_DBG("Application failed to subscribe to topic (ret code %x)\n", suback_event->return_code);
    }
#if MQTT_5
    /* Print any properties received along with the message */
    mqtt_prop_print_input_props(m);
#endif
#endif
    break;
  }
  case MQTT_EVENT_UNSUBACK: {
    LOG_DBG("Application is unsubscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_PUBACK: {
    LOG_DBG("Publishing complete.\n");
    break;
  }
#if MQTT_5_AUTH_EN
  case MQTT_EVENT_AUTH: {
    LOG_DBG("Continuing auth.\n");
    struct mqtt_prop_auth_event *auth_event = (struct mqtt_prop_auth_event *)data;
    break;
  }
#endif
  default:
    LOG_DBG("Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
static int
construct_pub_topic(void)
{
  // int len = snprintf(pub_topic, BUFFER_SIZE, "iot-2/evt/%s/fmt/json",
  //                    conf.event_type_id);
  //LWPubSub - The same topic used in the article scenario: /99/<client_id>
  int len = snprintf(pub_topic, BUFFER_SIZE, "/99/%s", client_id);
  LOG_INFO("Pub Topic: %s\n", pub_topic);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Pub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

#if MQTT_5
  PUB_TOPIC_ALIAS = 1;
#endif

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
construct_sub_topic(void)
{
  // int len = snprintf(sub_topic, BUFFER_SIZE, "iot-2/cmd/%s/fmt/json",
  //                    conf.cmd_type);
  //LWPubSub - subscribe topic: /99/<client_id>/cmd
  int len = snprintf(sub_topic, BUFFER_SIZE, "/99/%02x%02x%02x%02x%02x%02x/cmd",
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
  LOG_INFO("Sub Topic: %s\n", sub_topic);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_INFO("Sub Topic: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
construct_client_id(void)
{
  // int len = snprintf(client_id, BUFFER_SIZE, "d:%s:%s:%02x%02x%02x%02x%02x%02x",
  //                    conf.org_id, conf.type_id,
  //                    linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
  //                    linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
  //                    linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

  int len = snprintf(client_id, BUFFER_SIZE, "%02x%02x%02x%02x%02x%02x",
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);


  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    LOG_ERR("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }
  LOG_DBG("Client ID: %s\n", client_id);

  #define MQTT_CLIENT_USERNAME client_id

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
update_config(void)
{
  if(construct_client_id() == 0) {
    /* Fatal error. Client ID larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if(construct_sub_topic() == 0) {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if(construct_pub_topic() == 0) {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  /* Reset the counter */
  seq_nr_value = 0;

  state = STATE_INIT;

  /*
   * Schedule next timer event ASAP
   *
   * If we entered an error state then we won't do anything when it fires.
   *
   * Since the error at this stage is a config error, we will only exit this
   * error state if we get a new config.
   */
  etimer_set(&publish_periodic_timer, 0);

#if MQTT_5
  LIST_STRUCT_INIT(&(conn.will), properties);

  mqtt_props_init();
#endif

  return;
}
/*---------------------------------------------------------------------------*/
static int
init_config()
{
  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));

  memcpy(conf.org_id, MQTT_CLIENT_ORG_ID, strlen(MQTT_CLIENT_ORG_ID));
  memcpy(conf.type_id, DEFAULT_TYPE_ID, strlen(DEFAULT_TYPE_ID));
  memcpy(conf.auth_token, MQTT_CLIENT_AUTH_TOKEN,
         strlen(MQTT_CLIENT_AUTH_TOKEN));
  memcpy(conf.event_type_id, DEFAULT_EVENT_TYPE_ID,
         strlen(DEFAULT_EVENT_TYPE_ID));
  memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  memcpy(conf.cmd_type, DEFAULT_SUBSCRIBE_CMD_TYPE, 1);

  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;
  conf.def_rt_ping_interval = DEFAULT_RSSI_MEAS_INTERVAL;

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
subscribe(void)
{
  /* Publish MQTT topic in IBM quickstart format */
  mqtt_status_t status;

#if MQTT_5
  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0,
                          MQTT_NL_OFF, MQTT_RAP_OFF, MQTT_RET_H_SEND_ALL,
                          MQTT_PROP_LIST_NONE);
#else
  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);
#endif

  LOG_DBG("Subscribing!\n");
  if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
    LOG_ERR("Tried to subscribe but command queue was full!\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
  /* Connect to MQTT server */
  // mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
  //              (conf.pub_interval * 3) / CLOCK_SECOND,
  mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
               DEFAULT_KEEP_ALIVE_TIMER,
  #if MQTT_5
               MQTT_CLEAN_SESSION_ON,
               MQTT_PROP_LIST_NONE);
  #else
               MQTT_CLEAN_SESSION_ON);
  #endif

  state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/
#if MQTT_5_AUTH_EN
static void
send_auth(struct mqtt_prop_auth_event *auth_info, mqtt_auth_type_t auth_type)
{
  mqtt_prop_clear_prop_list(&auth_props);

  if(auth_info->auth_method.length) {
    (void)mqtt_prop_register(&auth_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_AUTH,
                             MQTT_VHDR_PROP_AUTH_METHOD,
                             auth_info->auth_method.string);
  }

  if(auth_info->auth_data.len) {
    (void)mqtt_prop_register(&auth_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_AUTH,
                             MQTT_VHDR_PROP_AUTH_DATA,
                             auth_info->auth_data.data,
                             auth_info->auth_data.len);
  }

  /* Connect to MQTT server */
  mqtt_auth(&conn, auth_type, auth_props);

  if(state != STATE_CONNECTING) {
    LOG_DBG("MQTT reauthenticating\n");
  }
}
#endif
/*---------------------------------------------------------------------------*/
static void
ping_parent(void)
{
  if(have_connectivity()) {
    uip_icmp6_send(uip_ds6_defrt_choose(), ICMP6_ECHO_REQUEST, 0,
                   ECHO_REQ_PAYLOAD_LEN);
  } else {
    LOG_WARN("ping_parent() is called while we don't have connectivity\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
state_machine(void)
{
  switch(state) {
  case STATE_INIT:
    /* If we have just been configured register MQTT connection */
    mqtt_register(&conn, &mqtt_client_process, client_id, mqtt_event,
                  MAX_TCP_SEGMENT_SIZE);

    /*
     * If we are not using the quickstart service (thus we are an IBM
     * registered device), we need to provide user name and password
     */
    if(strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) != 0) {
      if(strlen(conf.auth_token) == 0) {
        LOG_ERR("User name set, but empty auth token\n");
        state = STATE_ERROR;
        break;
      } else {
        mqtt_set_username_password(&conn, MQTT_CLIENT_USERNAME,
                                   conf.auth_token);
      }
    }

    /* _register() will set auto_reconnect. We don't want that. */
    conn.auto_reconnect = 0;
    connect_attempt = 1;

#if MQTT_5
    mqtt_prop_create_list(&publish_props);

    /* this will be sent with every publish packet */
    (void)mqtt_prop_register(&publish_props,
                             NULL,
                             MQTT_FHDR_MSG_TYPE_PUBLISH,
                             MQTT_VHDR_PROP_USER_PROP,
                             "Contiki", "v4.5+");

    mqtt_prop_print_list(publish_props, MQTT_VHDR_PROP_ANY);
#endif

    state = STATE_REGISTERED;
    LOG_DBG("Init MQTT version %d\n", MQTT_PROTOCOL_VERSION);
    /* Continue */
  case STATE_REGISTERED:
    if(have_connectivity()) {
      /* Registered and with a public IP. Connect */
      LOG_DBG("Registered. Connect attempt %u\n", connect_attempt);
      ping_parent();
      connect_to_broker();
    } else {
      leds_on(MQTT_CLIENT_STATUS_LED);
      ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
    }
    etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
    return;
    break;
  case STATE_CONNECTING:
    leds_on(MQTT_CLIENT_STATUS_LED);
    ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
    /* Not connected yet. Wait */
    LOG_DBG("Connecting (%u)\n", connect_attempt);
    break;
  case STATE_CONNECTED:
    /* Don't subscribe unless we are a registered device */
    if(strncasecmp(conf.org_id, QUICKSTART, strlen(conf.org_id)) == 0) {
      LOG_DBG("Using 'quickstart': Skipping subscribe\n");
      state = STATE_PUBLISHING;
    }
    /* Continue */
  case STATE_PUBLISHING:
    /* If the timer expired, the connection is stable. */
    if(timer_expired(&connection_life)) {
      /*
       * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
       * attempts if we disconnect after a successful connect
       */
      connect_attempt = 0;
    }

    if(mqtt_ready(&conn) && conn.out_buffer_sent) {
      /* Connected. Publish */
      if(state == STATE_CONNECTED) {
        subscribe();
        state = STATE_PUBLISHING;
      } else {
        // leds_on(MQTT_CLIENT_STATUS_LED);
        // ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
        LOG_DBG("Publishing\n");
        // FedSensor - periodically get measurement and take action
        publish(0);
      }
      etimer_set(&publish_periodic_timer, conf.pub_interval);
      /* Return here so we don't end up rescheduling the timer */
      return;
    } else {
      /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or that
       * simply there is some network delay. In both cases, we refuse to
       * trigger a new message and we wait for TCP to either ACK the entire
       * packet after retries, or to timeout and notify us.
       */
      LOG_DBG("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
              conn.out_queue_full);
    }
    break;
  case STATE_DISCONNECTED:
    LOG_DBG("Disconnected\n");
    if(connect_attempt < RECONNECT_ATTEMPTS ||
       RECONNECT_ATTEMPTS == RETRY_FOREVER) {
      /* Disconnect and backoff */
      clock_time_t interval;
#if MQTT_5
      mqtt_disconnect(&conn, MQTT_PROP_LIST_NONE);
#else
      mqtt_disconnect(&conn);
#endif
      connect_attempt++;

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
        RECONNECT_INTERVAL << 3;

      LOG_DBG("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);

      etimer_set(&publish_periodic_timer, interval);

      state = STATE_REGISTERED;
      return;
    } else {
      /* Max reconnect attempts reached. Enter error state */
      state = STATE_ERROR;
      LOG_DBG("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;
  case STATE_CONFIG_ERROR:
    /* Idle away. The only way out is a new config */
    LOG_ERR("Bad configuration.\n");
    return;
  case STATE_ERROR:
  default:
    leds_on(MQTT_CLIENT_STATUS_LED);
    /*
     * 'default' should never happen.
     *
     * If we enter here it's because of some error. Stop timers. The only thing
     * that can bring us out is a new config event
     */
    LOG_ERR("Default case: State=0x%02x\n", state);
    return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}
/*---------------------------------------------------------------------------*/
static void
init_extensions(void)
{
  int i;

  for(i = 0; i < mqtt_client_extension_count; i++) {
    if(mqtt_client_extensions[i]->init) {
      mqtt_client_extensions[i]->init();
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
init_sensor_readings(void)
{
#if BOARD_SENSORTAG
  SENSORS_ACTIVATE(hdc_1000_sensor);
  SENSORS_ACTIVATE(tmp_007_sensor);
  SENSORS_ACTIVATE(opt_3001_sensor);
  SENSORS_ACTIVATE(bmp_280_sensor);

  init_mpu_reading(NULL);
#endif
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_client_process, ev, data)
{

  PROCESS_BEGIN();

  printf("\nLWPubSub-FedSensor-LWAIoT - MQTT Client Process\n");



  /* PRINT USED CRYPTO in LOG */
  if(LWPUBSUB_CRYPTO_MODE == LWPUBSUB_AES_CBC_128) {
    LOG_INFO("Crypto Algorithm: AES-CBC-128\n");
  } else if(LWPUBSUB_CRYPTO_MODE == LWPUBSUB_AES_CBC_192) {
    LOG_INFO("Crypto Algorithm: AES-CBC-192\n");
  } else if(LWPUBSUB_CRYPTO_MODE == LWPUBSUB_AES_CBC_256) {
    LOG_INFO("Crypto Algorithm: AES-CBC-256\n");
  } else if(LWPUBSUB_CRYPTO_MODE == LWPUBSUB_AES_CTR_128) {
    LOG_INFO("Crypto Algorithm: AES-CTR-128\n");
  } else if(LWPUBSUB_CRYPTO_MODE == LWPUBSUB_AES_CTR_192) {
    LOG_INFO("Crypto Algorithm: AES-CTR-192\n");
  } else if(LWPUBSUB_CRYPTO_MODE == LWPUBSUB_AES_CTR_256) {
    LOG_INFO("Crypto Algorithm: AES-CTR-256\n");
  } else if(LWPUBSUB_CRYPTO_MODE == LWPUBSUB_AES_CCM) {
    LOG_INFO("Crypto Algorithm: AES-CCM-8\n");
  } else {
    LOG_ERR("Wrong crypto algorithm\n");
  }

  /* PRINT POLL FREQUENCY */
  //LOG_INFO("Poll frequency: %lu \n", LWPUBSUB_POLLFREQUENCY);

  if(LWPUBSUB_POLLFREQUENCY == 5) {
    LOG_INFO("Poll frequency: VERYHIGH\n");
  } else if(LWPUBSUB_POLLFREQUENCY == 15) {
    LOG_INFO("Poll frequency: HIGH\n");
  } else if(LWPUBSUB_POLLFREQUENCY == 900) {
    LOG_INFO("Poll frequency: MEDIUM\n");
  } else if(LWPUBSUB_POLLFREQUENCY == 21600) {
    LOG_INFO("Poll frequency: LOW\n");
  } else if(LWPUBSUB_POLLFREQUENCY == 86400) {
    LOG_INFO("Poll frequency: VERYLOW\n");
  } else if(LWPUBSUB_POLLFREQUENCY == 10) {
    LOG_INFO("Poll frequency: 10 secs.\n");
  } else {
    LOG_ERR("Wrong poll frequency\n");
  }

  /* PRINT TSCH SCHEDULE */
#ifdef LWPUBSUB_ORCHESTRA
  LOG_INFO("TSCH Schedule: Orchestra\n");
#else
  LOG_INFO("TSCH Schedule: Minimal\n");
#endif


  if(init_config() != 1) {
    PROCESS_EXIT();
  }

  init_extensions();

  init_sensor_readings();

  update_config();

  def_rt_rssi = 0x8000000;
  uip_icmp6_echo_reply_callback_add(&echo_reply_notification,
                                    echo_reply_handler);
  etimer_set(&echo_request_timer, conf.def_rt_ping_interval);

  /* Main loop */
  while(1) {

    PROCESS_YIELD();

    if(ev == button_hal_release_event &&
       ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO) {
      if(state == STATE_ERROR) {
        connect_attempt = 1;
        state = STATE_REGISTERED;
      }
    }

    if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
       ev == PROCESS_EVENT_POLL ||
       (ev == button_hal_release_event &&
        ((button_hal_button_t *)data)->unique_id == BUTTON_HAL_ID_BUTTON_ZERO)) {
      state_machine();
    }

    if(ev == PROCESS_EVENT_TIMER && data == &echo_request_timer) {
      ping_parent();
      etimer_set(&echo_request_timer, conf.def_rt_ping_interval);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

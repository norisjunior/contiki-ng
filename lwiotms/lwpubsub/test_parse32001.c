#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int sensorsNumber = 0;
static char valuereceived[10];


static unsigned long sensorList[10]; //33250\0

static int polling_interval = 0;

#define COMMAND_RECEIVED_BUFFER 300
char commandReceived[COMMAND_RECEIVED_BUFFER];
static int commandReceivedlen = 0;

static int line = 0;
static int column = 0;

static char pipe[2] = "|";
static char semic[2] = ";";
static char hashtag[2] = "#";






static void parse32001()
{
  //commandReceived:  100;1|033250;2|033251;3|033252;4|033253"

    char commandReceived[300] = "005;1|033250;2|033251;3|033252;4|033253;5|033254;6|033255;7|033256;8|033257;9|033258";
    int commandReceivedlen = strlen(commandReceived);
    printf("\nCommandReceivedLen: %d\n", commandReceivedlen);


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
  printf("Time interval: %d\n\n", polling_interval);

  //*******************************************************************

  sensorsNumber = ((commandReceivedlen - 3) / 9); //é o número de linhas da variável sensorList
  printf("\nsensorsNumber: %d\n\n", sensorsNumber);

  char sensorsreceived[10][7]; // são 10 linhas de 7 caracteres cada linha: 6 caract. + \0
  memset(sensorsreceived, 0, sizeof(sensorsreceived));

  // int line = 0;
  // int column = 0;
  // char pipe[2] = "|";
  // char semic[2] = ";";
  line = 0;
  column = 0;

  for (i = 5, j = 0; i < commandReceivedlen; i++, j++) {
    printf("\ncommandReceived:");
    if ( (commandReceived[i] != pipe[0]) && (commandReceived[i] != semic[0]) && (commandReceived[i-1] != semic[0]) ) {
      printf("[%d]: %c - ", i, commandReceived[i]);
      sensorsreceived[line][column] = commandReceived[i];
      column++;
    }

    if ( (commandReceived[i] == semic[0]) ) { //pula pra próxima linha
      //sensorsreceived[line][column] = '\0';
      printf("\ncommandReceived[%d]: %c - ", i, commandReceived[i]);
      line++;
      column = 0;
    }
  }


  for (i = 0; i < sensorsNumber; i++) {
    sensorList[i] = strtol(sensorsreceived[i], NULL, 10);
    printf("SensorList[%d]: %lu\n", i, sensorList[i]);
  }

}


void main() {
// #payload = "100;1|033250;2|033251;3|033252;4|033253"


  parse32001();

}
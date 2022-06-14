#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int sensorsNumber = 4;
static char valuereceived[10];


/*For linreg*/
float lin_bias = 0.0;
float lin_weights[10];


// #payload = "321030|250;1|16.0126;2|0.8444#0.4488#8.0331#0.2608"
#define CHAR_BUFF_SIZE 10
static char * _float_to_char(float x, char *p) {
    char *s = p + CHAR_BUFF_SIZE; // go to end of buffer
    long decimals;  // variable to store the decimals
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





void main() {
  int n = 0; //índice do número de sensores (linhas dos weights)
  float result; // para armazenar a multiplicação de new_observation[n] * weights[m][n]
  char fbuf[15]; //buffer para armazenar o valor de float para log
  int i, j = 0;

  static char pipe[2] = "|";
  static char semic[2] = ";";
  static char hashtag[2] = "#";


//   lin_bias = 16.0126;
//   lin_weights[0] = 0.8444;
//   lin_weights[1] = 0.4488;
//   lin_weights[2] = 8.0331;
//   lin_weights[3] = 0.2608;

  char commandReceived[300] = "250;1|16.0126;2|0.8444#0.4488#8.0331#0.2608";
  int commandReceivedlen = strlen(commandReceived);

  printf("CommandReceived: %s; Len: %d\n\n", commandReceived, commandReceivedlen);

  lin_bias = 0.0;
  for (n = 0; n < 10; n++) {
      lin_weights[n] = 0.0;
  }
  n = 0;

  i = 0;
  j = 0;
  for (i = 6; i < commandReceivedlen; i++) {
    if ( (commandReceived[i] != pipe[0]) && (commandReceived[i] != semic[0]) && (commandReceived[i-1] != semic[0]) && (commandReceived[i] != hashtag[0]) ) {
        printf("\ncommandReceived[%d]: %c - ", i, commandReceived[i]);
        valuereceived[j] = commandReceived[i];
        j++;
        printf("valuereceived[%d]: %s\n", j, valuereceived);
        
    }

    if ( commandReceived[i] == semic[0] ) { //passou por um ponto e vírgula e, independentemente de ser o primeiro ou não, não é mais bias
        lin_bias = strtof(valuereceived, NULL);
        printf("bias: %.4f\n", lin_bias);
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

        for (int j = 0; j < sensorsNumber; j++ ) {
          printf("[%.4f] ", lin_weights[j]);
        }




  float new_observation[10];
/*
  Aqui:
  X é: new_observation[10], na verdade new_observation[4] (usando 33250, 33251, 33252, 33253), seria isso:
    (i == 4) new_observation[i] = {30.39, 89.86, 0.86, 42.86};
*/
  new_observation[0] = 270.39;
  new_observation[1] = 350.86;
  new_observation[2] = 3.8;
  new_observation[3] = 42.86;
  
  for (n = 0; n < sensorsNumber; n++) {
    result += new_observation[n] * lin_weights[n];
  }

  result += lin_bias;

  printf("\n\nresult: %.4f", result);

  char reading[10] = "\0";
  printf("\n\n result as string: %s", _float_to_char(result, reading));


}
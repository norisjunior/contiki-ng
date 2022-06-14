#include <stdio.h> /* For printf() */
#include <stdlib.h>
#include <math.h>
#include <string.h>

char sensor_value[10];
int measurement_type = 0;

static void
publish(int is_measurement)
{

    if (is_measurement == 1) {
        printf("Is measurement");
    }

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
  float value = 0;
  value = read_dummy();
  return value;
}

static float read_33460()
{
    memset(sensor_value, 0, 10);
    float value = 0.0;

    // https://stackoverflow.com/questions/1202687/how-do-i-get-a-specific-range-of-numbers-from-rand
    // M + rand() / (RAND_MAX / (N - M + 1) + 1)
    snprintf(sensor_value, 10, "%d.%d", (99 + rand() / (RAND_MAX / (20 - 99 + 1) + 1)), (rand()%10000));
    value = strtof(sensor_value, NULL);
  
  
  return value;
}


char mpu_values[10];

static void get_mpu_reading() {
    int value_x = 1;
    int value_y = 3;
    int value_z = 4;
    char values[3];
    mpu_values[0] = value_x;
    mpu_values[1] = value_y;
    mpu_values[2] = value_z;
}

void main() {
    publish(1);

    int i = 0;

    float valor = 0;

    for (i = 0; i < 50; i++) {
        valor = read_33460();
        printf("\n Valor read_33460(): %.4f", valor);
    }

    

    printf("\n valor: %.4f\n", valor);

    get_mpu_reading();



    for (i = 0; i < 3; i++) {
        printf("\n values[%d]: %d", i, mpu_values[i]);
    }

    
}


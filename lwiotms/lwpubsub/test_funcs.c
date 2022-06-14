#include <stdio.h> /* For printf() */
#include <stdlib.h>
#include <math.h>
#include <string.h>


static unsigned long sensorList[10]; //33250\0
static int sensorsNumber = 0;

static float new_observation[10];

char sensor_value[10];
int acc_movement_type = 0;

// float bias[10];
// float weights[10][10]; // são 10 linhas de 10 colunas com valores float
// int number_of_classes = 2;

// //BIAS [-0.4563] [0.4563]
// bias[0] = -0.4563;
// bias[1] = 0.4563;

// weights[0][0] = -0.1821; weights[0][1] = -0.1279; weights[0][0] = -0.0153; 
// weights[1][0] = 0.1821; weights[1][1] = 0.1279; weights[1][0] = 0.0153; 

//Matriz de weights:
//Class [0]: [-0.1821] [-0.1279] [-0.0153]
//Class [1]: [0.1821] [0.1279] [0.0153]



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

    // if (acc_movement_type > 10) {
    //     acc_movement_type = 0;
    // }

    if (acc_movement_type == 0) { // fall
        //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
        //float value = strtof(sensor_value, NULL);
        value = 4.665;
    } else {
        snprintf(sensor_value, 10, "-%d.%d", 2, (rand()%10000));
        value = strtof(sensor_value, NULL);
    }
    //printf("sensor string: %s\n", sensor_value);
    //printf("sensor float: %f\n", value);

    // acc_movement_type++;

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

    if (acc_movement_type == 0) { // fall
        //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
        //float value = strtof(sensor_value, NULL);
        value = 0.5648;
    } else {
        snprintf(sensor_value, 10, "%d.%d", 1, (rand()%10000));
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

    if (acc_movement_type == 0) { // fall
        //snprintf(sensor_value, 10, "%d.%u", 3, (rand()%10000));
        //float value = strtof(sensor_value, NULL);
        value = -0.8776;
    } else {
        snprintf(sensor_value, 10, "%d.%d", 1, (rand()%10000));
        value = strtof(sensor_value, NULL);
    }
    //printf("sensor string: %s\n", sensor_value);
    //printf("sensor float: %f\n", value);

    return value;
}

/* static float read_33131()
{
  //Dummy:
  memset(sensor_value, 0, 10);
  snprintf(sensor_value, 10, "%d.%d", 0, (rand()%10000));
  float value = strtof(sensor_value, NULL);
  return value;
}

static float read_33132()
{
  //Dummy:
  memset(sensor_value, 0, 10);
  snprintf(sensor_value, 10, "-%d.%d", 0, (rand()%1000));
  float value = strtof(sensor_value, NULL);
  return value;
}
 */
static float read_33250()
{
  //printf("\nRead 33250");
  memset(sensor_value, 0, 10);
  snprintf(sensor_value, 10, "%d.%d", (0+rand()%10), (rand()%10));
  //printf("sensor string: %s\n", sensor_value);
  float value = strtof(sensor_value, NULL);
  //printf("sensor float: %f\n", value);
  return value;
}

static float read_33251()
{
  //printf("\nRead 33251");
  memset(sensor_value, 0, 10);
  snprintf(sensor_value, 10, "%d.%d", (10+rand()%5), (rand()%5));
  //printf("sensor string: %s\n", sensor_value);
  float value = strtof(sensor_value, NULL);
  //printf("sensor float: %f\n", value);
  return value;
}

static float read_33252()
{
  //printf("\nRead 33252");
  memset(sensor_value, 0, 10);
  snprintf(sensor_value, 10, "%d.%d", (20+rand()%5), (rand()%5));
  //printf("sensor string: %s\n", sensor_value);
  float value = strtof(sensor_value, NULL);
  //printf("sensor float: %f\n", value);
  return value;
}

static float read_33253()
{
  //printf("\nRead 33253");
  memset(sensor_value, 0, 10);
  snprintf(sensor_value, 10, "%d.%d", (30+rand()%5), (rand()%5));
  //printf("sensor string: %s\n", sensor_value);
  float value = strtof(sensor_value, NULL);
  //printf("sensor float: %.2f\n", value);
  return value;
}



void main() {
    int i = 0;
    int epoch = 0;

    sensorsNumber = 3;
    sensorList[0] = 33130;
    sensorList[1] = 33131;
    sensorList[2] = 33132;

    for (i = 0; i < sensorsNumber; i++) {
        printf("SensorList[%d]: %lu\n", i, sensorList[i]);
    }

    for (epoch = 0; epoch < 21; epoch++) {
        printf("\n\n----- Epoch: %d\n", epoch);
        printf("acc_movement_type: %d", acc_movement_type);

        if (acc_movement_type > 10) {
            acc_movement_type = 0;
        }

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

                default: {
                    printf("No sensor in the list of cases: break case!");
                    break;
                }
            }
        }
        
        printf("\nNew observation/measurement collected: \n");
        for (i = 0; i < sensorsNumber; i++) {
            printf("%lu: %.4f\n", sensorList[i], new_observation[i]);
        }
        
        acc_movement_type++;

        




    }


}
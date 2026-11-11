#ifndef SECOND_TASK_MPU6050_H
#define SECOND_TASK_MPU6050_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} Mpu6050Acceleration;

bool Mpu6050_Init(void);
bool Mpu6050_ReadAcceleration(Mpu6050Acceleration *acceleration);

#endif

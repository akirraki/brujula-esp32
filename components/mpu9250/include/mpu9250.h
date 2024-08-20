/**
 * @brief driver para el sensor mpu9250, usado en el proyecto "brujula electronica"
 * @authors Mariano Romagnoli & Iñaki Izaguirre
 */

#pragma once

#include "driver/i2c.h"
#include "esp_err.h"

#define MPU9250_TASK_PRIORITY 2
#define MPU_TASK_SIZE 4 * 1024
#define MPU9250_DATA_QUEUE_SIZE 128
#define MPU9250_TICK_PERIOD_MS 250

#define SDA_PIN 21
#define SCL_PIN 22
#define GYRO_ADDR 0x68
#define MAG_ADDR 0x0C
#define buf_size 6
#define promedio 2000

typedef struct mpu9250_data_t
{
    float nivel;
    float buzamiento;
    float dir_buzamiento; // inclinacion respecto al norte
} mpu9250_data_t;

esp_err_t mpu9250_init(void);

extern QueueHandle_t xMPU9250Queue;
extern TaskHandle_t xMPU9250ProcessingTaskHandle;
extern QueueHandle_t xDisplayQueueA;

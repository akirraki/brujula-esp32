/**
 * @brief driver para el sensor mpu9250, usado en el proyecto "brujula electronica"
 * @authors Mariano Romagnoli & Iñaki Izaguirre
 */

#pragma once

#include "driver/i2c.h"
#include "esp_err.h"

#define SEND_DATA_FLAG 0x03
#define CALIBRATE_FLAG 0x05

typedef struct mpu9250_data_t
{
    float nivel;
    float buzamiento;
    float dir_buzamiento; // inclinacion respecto al norte
} mpu9250_data_t;

esp_err_t mpu9250_init(void);
void Calibracion(void);

extern volatile uint8_t finished_cal;
extern QueueHandle_t xMPU9250Queue;
extern TaskHandle_t xMPU9250CalTaskHandle;
extern TaskHandle_t xMPU9250ProcessingTaskHandle;
extern QueueHandle_t xDisplayQueueA;

extern float RateCalibrationRoll, RateCalibrationPitch, RateCalibrationYaw;
extern float RateCalibrationAccX, RateCalibrationAccY, RateCalibrationAccZ;
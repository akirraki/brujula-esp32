#pragma once

#include <stdio.h>
#include "ui.h"
#include "ili9341-lvgl-solution.h"
#include "mpu9250.h"
#include "nmea_parser.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern QueueHandle_t xDisplayQueueA;
extern QueueHandle_t xDisplayQueueB;
extern QueueHandle_t xGPSDataQueue;

extern TaskHandle_t xDispMeasurementsTaskHandle;
extern TaskHandle_t xGPSTaskHandle;

extern TaskHandle_t xStoreFileTaskHandle;
extern TaskHandle_t xDeleteAllFilesTaskHandle;
extern TaskHandle_t xLoadCalFileTaskHandle;
extern TaskHandle_t xResetTaskHandle;

extern TaskHandle_t xMPU9250CalTaskHandle;

void init_tasks(void);
void init_queues(void);
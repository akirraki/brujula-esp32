#include "tasks.h"

// display
lv_disp_t *my_diplay;
QueueHandle_t xDisplayQueueA = NULL;
QueueHandle_t xDisplayQueueB = NULL;
TaskHandle_t xDispMeasurementsTaskHandle = NULL;
void xDispMeasurementsTask(void *pvParameter);

// gps
#define GPS_TASK_SIZE 1024 * 4
#define GPS_TASK_PRIORITY 2
QueueHandle_t xGPSDataQueue = NULL;
void xGPSTask(void *pvParameter);
TaskHandle_t xGPSTaskHandle = NULL;

// storage
TaskHandle_t xStoreFileTaskHandle = NULL;
TaskHandle_t xDeleteAllFilesTaskHandle = NULL;
TaskHandle_t xLoadCalFileTaskHandle = NULL;
TaskHandle_t xResetTaskHandle = NULL;
void xDeleteAllFilesTask(void *pvParameter);
void xStoreFileTask(void *pvParameter);
void xLoadCalFileTask(void *pvParameter);
void xResetTask(void *pvParameter);

void init_queues(void)
{
    xGPSDataQueue = xQueueCreate(8, sizeof(gps_t));
    xDisplayQueueA = xQueueCreate(16, sizeof(mpu9250_data_t));
    xDisplayQueueB = xQueueCreate(16, sizeof(gps_t));
}
void init_tasks(void)
{

    xTaskCreate(xLoadCalFileTask,
                "store_cakibration_file",
                1024 * 4,
                NULL,
                2,
                &xLoadCalFileTaskHandle);
    // gps
    xTaskCreate(xGPSTask,
                "gps_task",
                GPS_TASK_SIZE,
                NULL,
                GPS_TASK_PRIORITY,
                &xGPSTaskHandle);

    xTaskCreate(xDispMeasurementsTask,
                "disp_measurements",
                1024 * 8,
                NULL,
                1,
                &xDispMeasurementsTaskHandle);
    // storage tasks
    xTaskCreate(xStoreFileTask,
                "store_a_file",
                1024 * 4,
                NULL,
                3,
                &xStoreFileTaskHandle);

    xTaskCreate(xDeleteAllFilesTask,
                "deleteFiles",
                1024 * 4,
                NULL,
                3,
                &xDeleteAllFilesTaskHandle);

    xTaskCreate(xResetTask,
                "deleteFiles",
                1024 * 4,
                NULL,
                3,
                &xResetTaskHandle);
}
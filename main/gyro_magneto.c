#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include "mpu9250.h"
#include "esp_log.h"

static const char *TAG = "gyroMagneto_task";

void xGyroMagnetoTask(void *pvParameter)
{
    const char *csv_filepath = "/csvfiles";
    const char *gyroMagneto_file_header = "nivel, buzamiento, DB\n";
    const uint32_t max_filepath = 15 + 512;
    static BaseType_t status = pdFALSE;
    mpu9250_data_t gyroMagnetoData;
    char filepath[max_filepath] = {};
    int file_count = 0;
    for (;;)
    {
        status = xQueueReceive(xMPU9250Queue, &gyroMagnetoData, pdMS_TO_TICKS(250));
        if (status == pdTRUE)
        {
            // testing
            if (file_count == 10)
            {
                ESP_LOGI(TAG, "%d files were written. Dying...", file_count);
                vTaskDelete(NULL);
            }
            else
            {
                // write data to .csv file
                snprintf(filepath, sizeof(filepath), "%s/mpu9250_data%d.csv", csv_filepath, file_count);

                FILE *file = fopen(filepath, "a");
                if (file == NULL)
                {
                    ESP_LOGE(TAG, "Failed to open file");
                    taskYIELD();
                    continue;
                }

                // if file is empty (new file), write header
                if (ftell(file) == 0)
                {
                    fputs(gyroMagneto_file_header, file);
                }

                fprintf(file, "%.05f, %.05f, %.05f\n", gyroMagnetoData.nivel, gyroMagnetoData.buzamiento, gyroMagnetoData.dir_buzamiento);

                fclose(file);
                file_count++;
            }
        }
        taskYIELD();
    }
}
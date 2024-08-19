#include "nmea_parser.h"
#include "flash-storage.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

#define YEAR_BASE (2000)
#define TIME_ZONE (-3) // Buenos Aires

static const char *TAG = "gps_task";

extern QueueHandle_t xGPSDataQueue;
extern TaskHandle_t xGPSTaskHandle;

/**
 * @brief GPS Event Handler
 *
 * @param event_handler_arg handler specific arguments
 * @param event_base event base, here is fixed to ESP_NMEA_EVENT
 * @param event_id event id
 * @param event_data event specific arguments
 */
void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    gps_t *gps = NULL;
    switch (event_id)
    {
    case GPS_UPDATE:
        gps = (gps_t *)event_data;
        /* send information parsed from GPS statements */
        xQueueSendToBack(xGPSDataQueue, gps, pdMS_TO_TICKS(10));
        break;
    case GPS_UNKNOWN:
        /* print unknown statements */
        // ESP_LOGW(TAG, "Unknown statement:%s", (char *)event_data);
        break;
    default:
        break;
    }
}

void xGPSTask(void *pvParameter)
{
    const char *csv_filepath = "/csvfiles";
    const char *gpsfile_header = "date, time, latitude, longitude, altitude\n";
    const uint32_t max_filepath = 15 + 512;
    static BaseType_t status = pdFALSE;
    gps_t gpsData;
    char filepath[max_filepath];
    int file_count = 0;
    for (;;)
    {
        status = xQueueReceive(xGPSDataQueue, &gpsData, pdMS_TO_TICKS(250));
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
                snprintf(filepath, sizeof(filepath), "%s/gps_data%d.csv", csv_filepath, file_count);
                FILE *file = fopen(filepath, "a");
                if (file == NULL)
                {
                    ESP_LOGE(TAG, "Failed to open file %s", filepath);
                    taskYIELD();
                    continue;
                }
                // if file is empty (new file), write header
                if (ftell(file) == 0)
                {
                    fputs(gpsfile_header, file);
                }

                fprintf(file, "%d/%d/%d, %d:%d:%d, %.05f, %.05f, %.05f\n",
                        gpsData.date.year + YEAR_BASE, gpsData.date.month, gpsData.date.day,
                        gpsData.tim.hour + TIME_ZONE, gpsData.tim.minute, gpsData.tim.second,
                        gpsData.latitude, gpsData.longitude, gpsData.altitude);

                fclose(file);
                file_count++;
                taskYIELD();
            }
        }
    }
}

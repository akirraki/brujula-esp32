#include "nmea_parser.h"
#include "flash-storage.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>

extern QueueHandle_t xGPSDataQueue;
extern QueueHandle_t xDisplayQueueB;
extern TaskHandle_t xGPSTaskHandle;
extern TaskHandle_t xDispMeasurementsTaskHandle;

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
        xQueueSendToFront(xGPSDataQueue, gps, 0);
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
    static BaseType_t status = pdFALSE;
    uint32_t notified_value = 0x00;
    gps_t gpsData;
    for (;;)
    {
        status = xQueueReceive(xGPSDataQueue, &gpsData, pdMS_TO_TICKS(1000));
        if (status == pdTRUE)
        {
            xTaskNotifyWait(pdFALSE,
                            ULONG_MAX,
                            &notified_value,
                            portMAX_DELAY);
            if ((notified_value & 0x02) != 0)
            {
                xQueueSendToFront(xDisplayQueueB, &gpsData, 0);
            }
        }
        taskYIELD();
    }
}

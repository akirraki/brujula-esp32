#include "ui.h"
#include "ili9341-lvgl-solution.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "nmea_parser.h"
#include "mpu9250.h"
#include "wifi_ap.h"
#include "indev.h"

extern QueueHandle_t xDisplayQueueA;
extern QueueHandle_t xDisplayQueueB;
extern TaskHandle_t xDispMeasurementsTaskHandle;
extern TaskHandle_t xGPSTaskHandle;

#define YEAR_BASE (2000)
#define TIME_ZONE (-3) // Buenos Aires

void xDispMeasurementsTask(void *pvParameter);

void xDispMeasurementsTask(void *pvParameter)
{
    static BaseType_t status = pdFALSE;
    gps_t gpsData;
    mpu9250_data_t gyroMagnetoData;
    char buf[64];
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        if (lv_scr_act() == ui_Screen1)
        {
            xTaskNotify(xMPU9250ProcessingTaskHandle, 0x03, eSetBits);
            status = xQueueReceive(xDisplayQueueA, &gyroMagnetoData, pdMS_TO_TICKS(100));
            if (status == pdTRUE)
            {
                if (lvgl_lock(-1))
                {
                    lv_slider_set_value(ui_Nivel, gyroMagnetoData.nivel, LV_ANIM_OFF);
                    snprintf(buf, sizeof(buf), " %.01f°", gyroMagnetoData.buzamiento);
                    lv_label_set_text(ui_Buzamiento2, buf);
                    snprintf(buf, sizeof(buf), "%-.01f°", gyroMagnetoData.dir_buzamiento);
                    lv_obj_set_x(ui_Norte1, 47 + strlen(buf));
                    lv_label_set_text(ui_Norte1, buf);
                    lv_img_set_angle(ui_Image3, 10 * gyroMagnetoData.dir_buzamiento - 280);
                    lvgl_unlock();
                }
            }
        }
        if (lv_scr_act() == ui_Screen2)
        {
            xTaskNotify(xGPSTaskHandle, 0x02, eSetBits);
            status = xQueueReceive(xDisplayQueueB, &gpsData, pdMS_TO_TICKS(100));
            if (status == pdTRUE)
            {
                if (lvgl_lock(-1))
                {
                    snprintf(buf, sizeof(buf), " %.03f°", gpsData.latitude);
                    lv_label_set_text(ui_Label24, buf); // latitud
                    snprintf(buf, sizeof(buf), " %.03f°", gpsData.longitude);
                    lv_label_set_text(ui_Label23, buf); // longitud
                    snprintf(buf, sizeof(buf), " %.03f°", gpsData.altitude);
                    lv_label_set_text(ui_Label22, buf); // altura
                    snprintf(buf, sizeof(buf), "%d/%d/%d", gpsData.date.day, gpsData.date.month, gpsData.date.year + YEAR_BASE);
                    lv_label_set_text(ui_Label21, buf); // fecha
                    snprintf(buf, sizeof(buf), "%d:%d:%d", gpsData.tim.hour + TIME_ZONE, gpsData.tim.minute, gpsData.tim.second);
                    lv_label_set_text(ui_Label20, buf); // hora

                    lvgl_unlock();
                }
            }
        }
    }
}

void WIFION(lv_event_t *e)
{
    static uint8_t state = 0x00; // wifi on: 0x01
    if (state)
    {
        wifi_stop_softap();
        state = 0x00;
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_start());
        state = 0x01;
    }
}
void MIDIERON(lv_event_t *e)
{
}
void CALIBRARON(lv_event_t *e)
{
    Calibracion();
    xTaskNotify(xSwTaskHandle, 0x03, eSetBits);
}
void BORRARON(lv_event_t *e)
{
}
void NOPOFUNCION(lv_event_t *e)
{
}
void SIPIFUNCION(lv_event_t *e)
{
}

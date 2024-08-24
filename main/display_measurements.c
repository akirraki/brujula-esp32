#include "ui.h"
#include "ili9341-lvgl-solution.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nmea_parser.h"
#include "mpu9250.h"
#include "wifi_ap.h"
#include "indev.h"
#include <stdio.h>
#include "dirent.h"

extern QueueHandle_t xDisplayQueueA;
extern QueueHandle_t xDisplayQueueB;
extern TaskHandle_t xDispMeasurementsTaskHandle;
extern TaskHandle_t xGPSTaskHandle;
extern TaskHandle_t xStoreFileTaskHandle;
extern TaskHandle_t xMPU9250CalTaskHandle;

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
    const TickType_t xFrequency = pdMS_TO_TICKS(150);

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
    lv_group_remove_all_objs(my_group);
    lv_group_add_obj(my_group, ui_SIPI);
    lv_group_add_obj(my_group, ui_NOPO);
}
void CALIBRARON(lv_event_t *e)
{
    xTaskNotify(xMPU9250CalTaskHandle, CALIBRATE_FLAG, eSetBits);
    lv_event_send(ui_Salir1, LV_EVENT_CLICKED, NULL);
}
void BORRARON(lv_event_t *e)
{
}
void NOPOFUNCION(lv_event_t *e)
{
    lv_group_remove_all_objs(my_group);
    lv_group_add_obj(my_group, ui_Medir);
}
void SIPIFUNCION(lv_event_t *e)
{
    xTaskNotify(xStoreFileTaskHandle, 0x07, eSetBits);
    lv_group_remove_all_objs(my_group);
    lv_group_add_obj(my_group, ui_Medir);
}

void xStoreFileTask(void *pvParameter)
{
    gps_t gpsData;
    mpu9250_data_t brujulaData = {0, 0, 0};
    BaseType_t xStatus = pdFALSE;
    static int file_count = 0;
    const char *csv_filepath = "/csvfiles";
    const char *brujula_header = "Nivel, Buzamiento, DB\n";
    const char *gps_header = "Fecha, Hora, Latitud, Longitud, Altitud\n";
    char filepath[80] = {};
    char buffer[80] = {};
    for (;;)
    {
        // xStatus = xQueueReceive(xDisplayQueueA, &brujulaData, portMAX_DELAY);
        xStatus = pdFALSE;
        if (xStatus == pdTRUE)
        {
            DIR *dirp;
            struct dirent *entry;

            dirp = opendir(csv_filepath);
            if (dirp == NULL)
            {
                taskYIELD();
            }
            while ((entry = readdir(dirp)) != NULL)
            {
                if (entry->d_type == DT_REG)
                { /* If the entry is a regular file */
                    file_count++;
                }
            }
            closedir(dirp);
            // write data to.csv file
            snprintf(filepath, sizeof(filepath), "%s/brujula_data%d.csv", csv_filepath, file_count);

            FILE *file = fopen(filepath, "a");
            if (file == NULL)
            {
                taskYIELD();
            }

            // if file is empty (new file), write header
            if (ftell(file) == 0)
            {
                fputs(brujula_header, file);
            }

            fprintf(file, "%.05f, %.05f, %.05f\n", brujulaData.nivel, brujulaData.buzamiento, brujulaData.dir_buzamiento);
            fclose(file);
            file_count = 0;
            taskYIELD();
        }
    }
}
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
#include <sys/unistd.h>
#include "dirent.h"

static const char *TAG = "display_measurements";

extern QueueHandle_t xDisplayQueueA;
extern QueueHandle_t xDisplayQueueB;
extern TaskHandle_t xDispMeasurementsTaskHandle;
extern TaskHandle_t xGPSTaskHandle;
extern TaskHandle_t xStoreFileTaskHandle;
extern TaskHandle_t xDeleteAllFilesTaskHandle;
extern TaskHandle_t xStoreCalFileTaskHandle;
extern TaskHandle_t xResetTaskHandle;
extern TaskHandle_t xMPU9250CalTaskHandle;

#define YEAR_BASE (2000)
#define TIME_ZONE (-3) // Buenos Aires

volatile uint8_t finishedDeletingFiles = 0;
uint8_t loadResetFile = 0;

esp_err_t set_wasReset(const char *filename, const char option)
{
    FILE *file = fopen(filename, "r+");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for reading and writing");
        return ESP_FAIL;
    }

    char line[12]; // "wasReset: X\n" is 11 characters + null terminator
    if (fgets(line, sizeof(line), file) == NULL)
    {
        ESP_LOGE(TAG, "Failed to read from file");
        fclose(file);
        return ESP_FAIL;
    }

    if (strncmp(line, "wasReset: ", 10) != 0)
    {
        ESP_LOGE(TAG, "Unexpected file format");
        fclose(file);
        return ESP_FAIL;
    }

    // Toggle the value
    char new_value = option;

    // Go back to the start of the file
    fseek(file, 10, SEEK_SET);

    // Write the new value
    if (fputc(new_value, file) == EOF)
    {
        ESP_LOGE(TAG, "Failed to write to file");
        fclose(file);
        return ESP_FAIL;
    }

    fclose(file);
    ESP_LOGI(TAG, "wasReset value toggled to %c", new_value);
    return ESP_OK;
}

void xDispMeasurementsTask(void *pvParameter);

void xDispMeasurementsTask(void *pvParameter)
{
    static BaseType_t status = pdFALSE;
    gps_t gpsData;
    mpu9250_data_t gyroMagnetoData;
    char buf[64];
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(80);

    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        if (finished_cal == 1)
        {
            if (lvgl_lock(-1))
            {
                finished_cal = 0;
                lv_group_add_obj(my_group, ui_Wifi);
                lv_group_add_obj(my_group, ui_Calibrar);
                lv_group_add_obj(my_group, ui_Resetear);
                lv_group_add_obj(my_group, ui_Borrar_medidas);
                lv_event_send(ui_Salir1, LV_EVENT_CLICKED, NULL);
                lvgl_unlock();
            }
        }
        if (finishedDeletingFiles == 1)
        {
            if (lvgl_lock(-1))
            {
                finishedDeletingFiles = 0;
                lv_group_add_obj(my_group, ui_Wifi);
                lv_group_add_obj(my_group, ui_Calibrar);
                lv_group_add_obj(my_group, ui_Resetear);
                lv_group_add_obj(my_group, ui_Borrar_medidas);
                lv_event_send(ui_Salir2, LV_EVENT_CLICKED, NULL);
                lvgl_unlock();
            }
        }
        if (loadResetFile == 1)
        {
            if (lvgl_lock(-1))
            {
                loadResetFile = 0;
                lv_group_add_obj(my_group, ui_Wifi);
                lv_group_add_obj(my_group, ui_Calibrar);
                lv_group_add_obj(my_group, ui_Resetear);
                lv_group_add_obj(my_group, ui_Borrar_medidas);
                lv_event_send(ui_Salir3, LV_EVENT_CLICKED, NULL);
                lvgl_unlock();
            }
        }
        if (lv_scr_act() == ui_Screen1)
        {
            xTaskNotify(xMPU9250ProcessingTaskHandle, 0x03, eSetBits);
            status = xQueueReceive(xDisplayQueueA, &gyroMagnetoData, pdMS_TO_TICKS(50));
            if (status == pdTRUE)
            {
                if (lvgl_lock(100))
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
            status = xQueueReceive(xDisplayQueueB, &gpsData, pdMS_TO_TICKS(20));
            if (status == pdTRUE)
            {
                if (lvgl_lock(100))
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
            else
            {
                if (lvgl_lock(100))
                {
                    lv_label_set_text(ui_Label24, "ERROR"); // latitud
                    lv_label_set_text(ui_Label23, "ERROR"); // longitud
                    lv_label_set_text(ui_Label22, "ERROR"); // altura
                    lv_label_set_text(ui_Label21, "ERROR"); // fecha
                    lv_label_set_text(ui_Label20, "ERROR"); // hora
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
    xTaskNotify(xStoreFileTaskHandle, 0x06, eSetBits);
}
void CALIBRARON(lv_event_t *e)
{
    if (lvgl_lock(500))
    {
        lv_group_remove_all_objs(my_group);
        xTaskNotify(xMPU9250CalTaskHandle, CALIBRATE_FLAG, eSetBits);
        lvgl_unlock();
    }
}
void BORRARON(lv_event_t *e)
{
    if (lvgl_lock(500))
    {
        lv_group_remove_all_objs(my_group);
        xTaskNotify(xDeleteAllFilesTaskHandle, 0x0E, eSetBits);
        lvgl_unlock();
    }
}
void NOPOFUNCION(lv_event_t *e)
{
    if (lvgl_lock(500))
    {
        lv_group_remove_all_objs(my_group);
        lv_group_add_obj(my_group, ui_Medir);
        lvgl_unlock();
    }
}
void SIPIFUNCION(lv_event_t *e)
{

    if (lvgl_lock(500))
    {
        xTaskNotify(xStoreFileTaskHandle, 0x07, eSetBits);
        lv_group_remove_all_objs(my_group);
        lv_group_add_obj(my_group, ui_Medir);
        lvgl_unlock();
    }
}

void RESETEARON(lv_event_t *e)
{
    if (lvgl_lock(500))
    {
        xTaskNotifyGive(xResetTaskHandle);
        lv_group_remove_all_objs(my_group);
        lvgl_unlock();
    }
}

void xStoreFileTask(void *pvParameter)
{
    gps_t gpsData;
    mpu9250_data_t brujulaData = {0, 0, 0};
    BaseType_t xStatus = pdFALSE;
    static int file_count = 0;
    const char *csv_filepath = "/csvfiles";
    const char *brujula_header = "Latitud, Longitud, Altitud, Buzamiento, DB, Fecha, Hora\n";
    uint32_t xNotifiedValue = 0x00;
    int8_t dataIsValid = 0;
    char filepath[80] = {};
    char buffer[100] = {};
    DIR *dirp;
    struct dirent *entry;
    for (;;)
    {
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &xNotifiedValue, portMAX_DELAY);
        if ((xNotifiedValue & 0x06) != 0)
        {
            xTaskNotify(xMPU9250ProcessingTaskHandle, SEND_DATA_FLAG, eSetBits);
            xStatus = xQueueReceive(xDisplayQueueA, &brujulaData, pdMS_TO_TICKS(100));
            if (xStatus == pdTRUE)
            {
                dataIsValid++;
                if (lvgl_lock(500))
                {
                    snprintf(buffer, sizeof(buffer), " %.03f°", brujulaData.dir_buzamiento);
                    lv_label_set_text(ui_Label1, buffer); // brujula
                    snprintf(buffer, sizeof(buffer), " %.03f°", brujulaData.buzamiento);
                    lv_label_set_text(ui_Label2, buffer); // buzamiento
                    lvgl_unlock();
                }
            }
            else
            {
                dataIsValid--;
                if (lvgl_lock(500))
                {
                    lv_label_set_text(ui_Label1, "error."); // brujula
                    lv_label_set_text(ui_Label2, "error."); // buzamiento
                    lvgl_unlock();
                }
            }
            xTaskNotify(xGPSTaskHandle, 0x02, eSetBits);
            xStatus = xQueueReceive(xDisplayQueueB, &gpsData, pdMS_TO_TICKS(100));
            if (xStatus == pdTRUE)
            {
                dataIsValid++;
                if (lvgl_lock(500))
                {
                    snprintf(buffer, sizeof(buffer), " %.03f", gpsData.latitude);
                    lv_label_set_text(ui_Label3, buffer); // latitud
                    snprintf(buffer, sizeof(buffer), " %.03f", gpsData.longitude);
                    lv_label_set_text(ui_Label4, buffer); // longitud
                    snprintf(buffer, sizeof(buffer), " %.03f", gpsData.altitude);
                    lv_label_set_text(ui_Label5, buffer); // altura
                    snprintf(buffer, sizeof(buffer), "%d/%d/%d", gpsData.date.day, gpsData.date.month, gpsData.date.year + YEAR_BASE);
                    lv_label_set_text(ui_Label6, buffer); // fecha
                    snprintf(buffer, sizeof(buffer), "%d:%d:%d", gpsData.tim.hour + TIME_ZONE, gpsData.tim.minute, gpsData.tim.second);
                    lv_label_set_text(ui_Label7, buffer); // hora
                    lvgl_unlock();
                }
            }
            else
            {
                dataIsValid--;
                if (lvgl_lock(500))
                {
                    lv_label_set_text(ui_Label3, "error."); // latitud
                    lv_label_set_text(ui_Label4, "error."); // longitud
                    lv_label_set_text(ui_Label5, "error."); // altura
                    lv_label_set_text(ui_Label6, "error."); // fecha
                    lv_label_set_text(ui_Label7, "error."); // hora
                    lvgl_unlock();
                }
            }
        }
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &xNotifiedValue, portMAX_DELAY);
        if ((xNotifiedValue & 0x07) != 0)
        {
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
            ESP_LOGI(TAG, "Hay %d archivos guardados.", file_count);
            if (dataIsValid >= 0)
            {
                // write data to.csv file
                snprintf(filepath, sizeof(filepath), "%s/brujula_data%d.csv", csv_filepath, file_count);

                FILE *file = fopen(filepath, "a");
                if (file == NULL)
                {
                    taskYIELD();
                    continue;
                }

                // if file is empty (new file), write header
                if (ftell(file) == 0)
                {
                    fputs(brujula_header, file);
                }

                if (dataIsValid == 2) // all data is valid
                {
                    fprintf(file, "%.05f, %.05f, %.05f, %.05f, %.05f, %d/%d/%d, %d:%d:%d\n",
                            gpsData.longitude, gpsData.latitude, gpsData.altitude, brujulaData.buzamiento,
                            brujulaData.dir_buzamiento, gpsData.date.day, gpsData.date.month,
                            gpsData.date.year + YEAR_BASE, gpsData.tim.hour + TIME_ZONE,
                            gpsData.tim.minute, gpsData.tim.second);
                }

                else // gps probably failed
                {
                    fprintf(file, "ERROR, ERROR, ERROR, %.05f, %.05f, ERROR, ERROR\n",
                            brujulaData.buzamiento,
                            brujulaData.dir_buzamiento);
                }
                fclose(file);
            }
        }
        file_count = 0;
        dataIsValid = 0;
    }
}

void xDeleteAllFilesTask(void *pvParameter)
{
    const char *csv_filepath = "/csvfiles";
    char filepath[256 + 15] = {};
    DIR *dir;
    struct dirent *entry;
    uint32_t xNotifiedValue = 0x00;
    for (;;)
    {
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &xNotifiedValue, portMAX_DELAY);
        if ((xNotifiedValue & 0x0E) != 0)
        {
            dir = opendir(csv_filepath);
            if (dir == NULL)
            {
                finishedDeletingFiles = 1;
                continue;
            }
            while ((entry = readdir(dir)) != NULL)
            {
                snprintf(filepath, sizeof(filepath), "%s/%s", csv_filepath, entry->d_name);
                unlink(filepath);
            }
            closedir(dir);
        }
        finishedDeletingFiles = 1;
    }
}

void xResetTask(void *pvParameter)
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        set_wasReset("/calib/reset.txt", '1');
        loadResetFile = 1;
        xTaskNotifyGive(xStoreCalFileTaskHandle);
    }
}
#include "tasks.h"
#include "indev.h"

static const char *TAG = "display_measurements";

#define YEAR_BASE (2000)
#define TIME_ZONE (-3) // Buenos Aires

uint8_t finishedDeletingFiles = 0;
uint8_t loadResetFile = 0;

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

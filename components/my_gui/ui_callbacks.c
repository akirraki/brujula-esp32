#include "ui.h"
#include "ili9341-lvgl-solution.h"
#include "esp_wifi.h"

extern lv_group_t *my_group;

extern void wifi_stop_softap(void);
extern TaskHandle_t xStoreFileTaskHandle;
extern TaskHandle_t xDeleteAllFilesTaskHandle;
extern TaskHandle_t xLoadCalFileTaskHandle;
extern TaskHandle_t xResetTaskHandle;

extern TaskHandle_t xMPU9250CalTaskHandle;

#define CALIBRATE_FLAG 0x05

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
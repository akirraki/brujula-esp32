#include <stdio.h>
#include "botonera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "iot_button.h"

extern QueueHandle_t xKeypadQueue;
extern TaskHandle_t xSwTaskHandle;

static void button_event_cb(void *arg, void *data);

static void button_event_cb(void *arg, void *data)
{
    uint32_t key_pressed = (botonera_id_t)data;
    xQueueSendToBack(xKeypadQueue, &key_pressed, 10);
    switch (key_pressed)
    {
    case DERECHA:
    {
        xTaskNotify(xSwTaskHandle, 0x01, eSetBits);
        break;
    }
    case IZQUIERDA:
    {
        xTaskNotify(xSwTaskHandle, 0x02, eSetBits);
        break;
    }
    default:
        break;
    }
}

void button_init(uint32_t button_num, botonera_id_t id)
{
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = button_num,
            .active_level = BUTTON_ACTIVE_LEVEL,
        }};

    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    esp_err_t err = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void *)id);
    ESP_ERROR_CHECK(err);
}
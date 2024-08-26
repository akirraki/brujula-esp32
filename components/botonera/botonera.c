#include <stdio.h>
#include "botonera.h"
#include "driver/gpio.h"

void button_init(uint32_t button_num, botonera_id_t id, button_event_t event, button_cb_t callback)
{
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = button_num,
            .active_level = BUTTON_ACTIVE_LEVEL,
        }};

    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    esp_err_t err = iot_button_register_cb(btn, event, callback, (void *)id);
    ESP_ERROR_CHECK(err);
}
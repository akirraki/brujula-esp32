#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "mdns.h"
#include "flash-storage.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "lwip/apps/netbiosns.h"

#include "driver/gpio.h"
#include "ili9341-lvgl-solution.h"
#include "ui.h"
#include "lvgl.h"
#include "botonera.h"

#include "nmea_parser.h"
#include "mpu9250.h"

#define R_FLAG (0x01)
#define L_FLAG (0x02)

typedef enum
{
    screen1 = 1,
    screen2,
    screen3,
} screens_t;

static void xSwitchScreenTask(void *pvParameter);

QueueHandle_t xKeypadQueue;
TaskHandle_t xSwTaskHandle = NULL;

static void keyboard_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static uint32_t last_key = 0;
    static BaseType_t status = pdFALSE;
    /*Get whether the a key is pressed and save the pressed key*/
    uint32_t act_key;
    status = xQueueReceive(xKeypadQueue, &act_key, 0);
    if (status != pdFALSE)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        /*Translate the keys to LVGL control characters according to your key definitions*/
        switch (act_key)
        {
        case ARRIBA:
            act_key = LV_KEY_PREV;
            break;
        case ABAJO:
            act_key = LV_KEY_NEXT;
            break;
        case ENTER:
            act_key = LV_KEY_ENTER;
            break;
        default:
            break;
        }
        last_key = act_key;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->key = last_key;
}

#define MDNS_INSTANCE "esp brujula web server"

#define WEB_MOUNT_POINT "/www"
#define CSV_MOUNT_POINT "/csvfiles"

static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set("brujula");
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}};

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

// wifi
void wifi_init_softap(void); // defined inside wifi_ap.c

// display
QueueHandle_t xDisplayQueueA = NULL;
QueueHandle_t xDisplayQueueB = NULL;
TaskHandle_t xDispMeasurementsTaskHandle = NULL;
void xDispMeasurementsTask(void *pvParameter);

// gps
#define GPS_TASK_SIZE 1024 * 4
#define GPS_TASK_PRIORITY 2
void gps_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
QueueHandle_t xGPSDataQueue = NULL;
void xGPSTask(void *pvParameter);
TaskHandle_t xGPSTaskHandle = NULL;

// utilidad - pasar a archivo aparte eventualmente

lv_group_t *my_group;

void toggle_group_visibility(lv_group_t *group, BaseType_t enable)
{
    if (enable == pdFALSE)
    {
        if (lvgl_lock(-1))
        {
            lv_group_focus_freeze(group, true);
            lvgl_unlock();
        }
    }
    else
    {
        if (lvgl_lock(-1))
        {
            lv_group_focus_freeze(group, false);
            lvgl_unlock();
        }
    }
}

static const char *TAG = "protoMedidas";

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi (already implemented in wifi-ap.h)
    wifi_init_softap();

    // Initialize SPIFFS for web files
    esp_vfs_spiffs_conf_t web_conf = {
        .base_path = WEB_MOUNT_POINT,
        .partition_label = "www",
        .max_files = 5,
        .format_if_mount_failed = true};
    ESP_ERROR_CHECK(spiffs_init(&web_conf));

    // Initialize SPIFFS for CSV files
    esp_vfs_spiffs_conf_t csv_conf = {
        .base_path = CSV_MOUNT_POINT,
        .partition_label = "csvdata",
        .max_files = 5,
        .format_if_mount_failed = true};
    ESP_ERROR_CHECK(spiffs_init(&csv_conf));

    xKeypadQueue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(xSwitchScreenTask,
                "screensTask",
                1024,
                NULL,
                2,
                &xSwTaskHandle);

    lv_disp_t *my_diplay = setup_display();
    if (lvgl_lock(-1))
    {
        ui_init();
        lvgl_unlock();
    }

    button_init(PIN_BOTON_ENTER, ENTER);
    button_init(PIN_BOTON_DER, DERECHA);
    button_init(PIN_BOTON_IZQ, IZQUIERDA);
    button_init(PIN_BOTON_ARRIBA, ARRIBA);
    button_init(PIN_BOTON_ABAJO, ABAJO);

    static lv_indev_drv_t indev_drv;

    lv_indev_drv_init(&indev_drv); /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.disp = my_diplay;
    indev_drv.read_cb = keyboard_read;
    /*Register the driver in LVGL and save the created input device object*/
    lv_indev_t *my_indev = lv_indev_drv_register(&indev_drv);

    my_group = lv_group_create();

    lv_group_add_obj(my_group, ui_Wifi);
    lv_group_add_obj(my_group, ui_Calibrar);
    lv_group_add_obj(my_group, ui_Borrar_medidas);
    lv_indev_set_group(my_indev, my_group);
    toggle_group_visibility(my_group, pdFALSE);

    // Initialize mDNS
    initialise_mdns();

    // Initialize NetBIOS
    netbiosns_init();
    netbiosns_set_name("brujula");

    /* NMEA parser configuration */
    xGPSDataQueue = xQueueCreate(16, sizeof(gps_t));

    xTaskCreate(
        xGPSTask,
        "gps_task",
        GPS_TASK_SIZE,
        NULL,
        GPS_TASK_PRIORITY,
        &xGPSTaskHandle);

    nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
    /* init NMEA parser library */
    nmea_parser_handle_t nmea_hdl = nmea_parser_init(&config);
    /* register event handler for NMEA parser library */
    nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);

    // mpu init
    mpu9250_init();
    // display task
    xDisplayQueueA = xQueueCreate(16, sizeof(mpu9250_data_t));
    xDisplayQueueB = xQueueCreate(16, sizeof(gps_t));
    xTaskCreate(
        xDispMeasurementsTask,
        "disp_measurements",
        1024 * 2,
        NULL,
        2,
        &xDispMeasurementsTaskHandle);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void xSwitchScreenTask(void *pvParameter)
{
    BaseType_t result;
    static uint32_t notifiedValue = 0;
    static screens_t screenContext = screen1;
    for (;;)
    {
        result = xTaskNotifyWait(pdFALSE,
                                 ULONG_MAX,
                                 &notifiedValue,
                                 portMAX_DELAY);
        if (result == pdPASS)
        {
            if (lvgl_lock(-1))
            {
                if ((notifiedValue & R_FLAG) != 0)
                {
                    switch (screenContext)
                    {
                    case screen1:
                    {
                        screenContext = screen2;
                        lv_event_send(ui_De1a2, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    case screen2:
                    {
                        screenContext = screen3;
                        toggle_group_visibility(my_group, pdTRUE);
                        lv_event_send(ui_De2a3, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    case screen3:
                    {
                        screenContext = screen1;
                        toggle_group_visibility(my_group, pdFALSE);
                        lv_event_send(ui_De3a1, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    default:
                        break;
                    }
                }

                if ((notifiedValue & L_FLAG) != 0)
                {
                    switch (screenContext)
                    {
                    case screen1:
                    {
                        screenContext = screen3;
                        toggle_group_visibility(my_group, pdTRUE);
                        lv_event_send(ui_De1a3, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    case screen2:
                    {
                        screenContext = screen1;
                        lv_event_send(ui_De2a1, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    case screen3:
                    {
                        screenContext = screen2;
                        toggle_group_visibility(my_group, pdFALSE);
                        lv_event_send(ui_De3a2, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    default:
                        break;
                    }
                }
                lvgl_unlock();
            }
        }
        taskYIELD();
    }
}
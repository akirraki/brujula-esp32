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
#include "wifi_ap.h"
#include "indev.h"

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

// display
lv_disp_t *my_diplay;
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

// storage
TaskHandle_t xStoreFileTaskHandle = NULL;
void xStoreFileTask(void *pvParameter);

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

    // initialize LCD
    my_diplay = setup_display();

    if (lvgl_lock(-1))
    {
        ui_init();
        lvgl_unlock();
    }
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

    // initialize lvgl input device (5 buttons)
    indev_init();

    xTaskCreate(
        xDispMeasurementsTask,
        "disp_measurements",
        1024 * 2,
        NULL,
        1,
        &xDispMeasurementsTaskHandle);
    // storage tasks
    xTaskCreate(
        xStoreFileTask,
        "store_a_file",
        1024 * 2,
        NULL,
        3,
        &xStoreFileTaskHandle);
}
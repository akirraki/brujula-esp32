#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

esp_err_t spiffs_init(esp_vfs_spiffs_conf_t *conf);
esp_err_t flash_storage_unmount(esp_vfs_spiffs_conf_t *conf);
void simple_file_test(void);
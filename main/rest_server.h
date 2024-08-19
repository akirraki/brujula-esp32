#pragma once

#include "esp_http_server.h"
#include "esp_vfs.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 512)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context
{
    char web_base_path[ESP_VFS_PATH_MAX + 1];
    char csv_base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

httpd_handle_t start_rest_server(const char *web_base_path, const char *csv_base_path, rest_server_context_t **out_rest_context);
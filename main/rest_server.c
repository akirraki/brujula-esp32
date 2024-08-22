#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include "unistd.h"
#include "sys/stat.h"
#include "rest_server.h"
#include "esp_log.h"
#include "cJSON.h"

httpd_handle_t server = NULL;
rest_server_context_t *rest_context = NULL;

static const char *REST_TAG = "esp-rest";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html"))
    {
        type = "text/html";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".js"))
    {
        type = "application/javascript";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".csv"))
    {
        type = "text/csv";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".css"))
    {
        type = "text/css";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".png"))
    {
        type = "image/png";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".ico"))
    {
        type = "image/x-icon";
    }
    else if (CHECK_FILE_EXTENSION(filepath, ".svg"))
    {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];

    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    strlcpy(filepath, rest_context->web_base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/')
    {
        strlcat(filepath, "/index.html", sizeof(filepath));
    }
    else
    {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1)
    {
        ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    ssize_t read_bytes;
    do
    {
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1)
        {
            ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        }
        else if (read_bytes > 0)
        {
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK)
            {
                close(fd);
                ESP_LOGE(REST_TAG, "File sending failed!");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    close(fd);
    ESP_LOGI(REST_TAG, "File sending complete");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler for getting file list */
static esp_err_t file_list_get_handler(httpd_req_t *req)
{
    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;

    DIR *dir = opendir(rest_context->csv_base_path);
    if (!dir)
    {
        ESP_LOGE(REST_TAG, "Failed to open directory");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open directory");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateArray();
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (CHECK_FILE_EXTENSION(entry->d_name, ".csv"))
        {
            cJSON_AddItemToArray(root, cJSON_CreateString(entry->d_name));
        }
    }
    closedir(dir);

    char *json_string = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_string);

    free(json_string);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Handler for downloading a file from the list */
static esp_err_t download_file_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;

    const char *filename = req->uri + strlen("/download/");
    if (!CHECK_FILE_EXTENSION(filename, ".csv"))
    {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Access denied");
        return ESP_FAIL;
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", rest_context->csv_base_path, filename);

    struct stat file_stat;
    if (stat(filepath, &file_stat) == -1)
    {
        ESP_LOGE(REST_TAG, "File does not exist : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    FILE *fd = fopen(filepath, "r");
    if (!fd)
    {
        ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }

    ESP_LOGI(REST_TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);
    char content_disposition[100];
    snprintf(content_disposition, sizeof(content_disposition), "attachment; filename=\"%s\"", filename);
    httpd_resp_set_hdr(req, "Content-Disposition", content_disposition);

    char *chunk = rest_context->scratch;
    size_t chunksize;
    do
    {
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);
        if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK)
        {
            fclose(fd);
            ESP_LOGE(REST_TAG, "File sending failed!");
            httpd_resp_sendstr_chunk(req, NULL);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            return ESP_FAIL;
        }
    } while (chunksize != 0);

    fclose(fd);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Handler for deleting a file from the list */
static esp_err_t delete_file_handler(httpd_req_t *req)
{
    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    char filepath[FILE_PATH_MAX];

    // Get the filename from the URI
    const char *filename = req->uri + strlen("/api/delete/");

    // Check if it's a CSV file
    if (!CHECK_FILE_EXTENSION(filename, ".csv"))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Only CSV files can be deleted");
        return ESP_FAIL;
    }

    // Construct the full file path
    snprintf(filepath, sizeof(filepath), "%s/%s", rest_context->csv_base_path, filename);

    // Try to delete the file
    if (unlink(filepath) == 0)
    {
        httpd_resp_sendstr(req, "File deleted successfully");
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(REST_TAG, "Error deleting file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to delete file");
        return ESP_FAIL;
    }
}

/* Handler for deleting all files */
static esp_err_t delete_all_files_handler(httpd_req_t *req)
{
    rest_server_context_t *rest_context = (rest_server_context_t *)req->user_ctx;
    DIR *dir;
    struct dirent *entry;
    char filepath[FILE_PATH_MAX];
    int files_deleted = 0;
    int delete_failed = 0;

    dir = opendir(rest_context->csv_base_path);
    if (!dir)
    {
        ESP_LOGE(REST_TAG, "Failed to open directory");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open directory");
        return ESP_FAIL;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (CHECK_FILE_EXTENSION(entry->d_name, ".csv"))
        {
            snprintf(filepath, sizeof(filepath), "%s/%s", rest_context->csv_base_path, entry->d_name);
            if (unlink(filepath) == 0)
            {
                files_deleted++;
            }
            else
            {
                delete_failed++;
                ESP_LOGE(REST_TAG, "Failed to delete file: %s", filepath);
            }
        }
    }

    closedir(dir);

    char response[100];
    snprintf(response, sizeof(response), "Deleted %d files. Failed to delete %d files.", files_deleted, delete_failed);
    httpd_resp_sendstr(req, response);

    return ESP_OK;
}

httpd_handle_t start_rest_server(const char *web_base_path, const char *csv_base_path, rest_server_context_t **out_rest_context)
{
    REST_CHECK(web_base_path, "wrong web base path", err);
    REST_CHECK(csv_base_path, "wrong csv base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->web_base_path, web_base_path, sizeof(rest_context->web_base_path));
    strlcpy(rest_context->csv_base_path, csv_base_path, sizeof(rest_context->csv_base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    /* URI handler for getting file list */
    httpd_uri_t file_list_get_uri = {
        .uri = "/api/file-list",
        .method = HTTP_GET,
        .handler = file_list_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &file_list_get_uri);

    /*URI handler for downloading files */
    httpd_uri_t file_download = {
        .uri = "/download/*",
        .method = HTTP_GET,
        .handler = download_file_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &file_download);

    /* URI handler for deleting a file */
    httpd_uri_t file_delete = {
        .uri = "/api/delete/*",
        .method = HTTP_DELETE,
        .handler = delete_file_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &file_delete);

    /* URI handler for deleting all files */
    httpd_uri_t delete_all_files = {
        .uri = "/api/full-clean",
        .method = HTTP_DELETE,
        .handler = delete_all_files_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &delete_all_files);

    /* URI handler for getting web server files */
    httpd_uri_t common_get_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = rest_common_get_handler,
        .user_ctx = rest_context};
    httpd_register_uri_handler(server, &common_get_uri);

    *out_rest_context = rest_context;
    return server;
err_start:
    free(rest_context);
err:
    return NULL;
}
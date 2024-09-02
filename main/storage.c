#include "tasks.h"
#include "indev.h"
#include "dirent.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "storage";

extern uint8_t finishedDeletingFiles;
extern uint8_t loadResetFile;

#define YEAR_BASE (2000)
#define TIME_ZONE (-3) // Buenos Aires

typedef struct cal_data
{
    int wasReset;
    float roll;
    float pitch;
    float yaw;
    float accX;
    float accY;
    float accZ;
} calfile_data_t;

calfile_data_t parseFileData(char *filename)
{
    FILE *file = fopen(filename, "r");
    calfile_data_t data = {0}; // Initialize all fields to 0
    char line[64];

    if (file == NULL)
    {
        data.wasReset = -1;
        return data;
    }

    while (fgets(line, sizeof(line), file))
    {
        char *key = strtok(line, ":");
        if (key == NULL)
            continue;

        char *value = strtok(NULL, "\n");
        if (value == NULL)
            continue;

        // Remove leading whitespace from value
        while (*value == ' ')
            value++;

        if (strcmp(key, "wasReset") == 0)
        {
            data.wasReset = atoi(value);
        }
        else if (strcmp(key, "Roll") == 0)
        {
            data.roll = atof(value);
        }
        else if (strcmp(key, "Pitch") == 0)
        {
            data.pitch = atof(value);
        }
        else if (strcmp(key, "Yaw") == 0)
        {
            data.yaw = atof(value);
        }
        else if (strcmp(key, "AccX") == 0)
        {
            data.accX = atof(value);
        }
        else if (strcmp(key, "AccY") == 0)
        {
            data.accY = atof(value);
        }
        else if (strcmp(key, "AccZ") == 0)
        {
            data.accZ = atof(value);
        }
    }

    fclose(file);
    return data;
}

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

void xLoadCalFileTask(void *pvParameter)
{
    calfile_data_t file_data = {0};
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        file_data = parseFileData("/calib/reset.txt");
        if (file_data.wasReset == -1)
        {
            taskYIELD();
            continue;
        }
        if (file_data.wasReset == 1) // setear wasReset a 0 despues de calibrar
        {
            RateCalibrationRoll = file_data.roll;
            RateCalibrationPitch = file_data.pitch;
            RateCalibrationYaw = file_data.yaw;
            RateCalibrationAccX = file_data.accX;
            RateCalibrationAccY = file_data.accY;
            RateCalibrationAccZ = file_data.accZ;
        }
        else // wasReset = 0
        {
            file_data = parseFileData("/calib/calibration.txt");
            RateCalibrationRoll = file_data.roll;
            RateCalibrationPitch = file_data.pitch;
            RateCalibrationYaw = file_data.yaw;
            RateCalibrationAccX = file_data.accX;
            RateCalibrationAccY = file_data.accY;
            RateCalibrationAccZ = file_data.accZ;
        }
    }
}

void xResetTask(void *pvParameter)
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        set_wasReset("/calib/reset.txt", '1');
        loadResetFile = 1;
        xTaskNotifyGive(xLoadCalFileTaskHandle);
    }
}
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <math.h>
#include "mpu9250.h"
#include "moving_average.h"

QueueHandle_t xMPU9250Queue = NULL;
TaskHandle_t xMPU9250ProcessingTaskHandle = NULL;

static const char *TAG = "mpu-9250";

static uint8_t powermode_gyro[2] = {0x6B, 0x00};    // powermode gyro
static uint8_t comando_filtro[2] = {0x1A, 0x05};    // filtro pasa bajos
static uint8_t sensibilidad_giro[2] = {0x1B, 0x08}; // sensibilidad del giroscopio
static uint8_t lectura_giro = 0x43;                 // Primera medida del giroscopio
static uint8_t giro_buffer[buf_size] = {0};
static uint8_t sensibilidad_acel[2] = {0x1C, 0x10}; // sensibilidad del acelerometro
static uint8_t lectura_acel = 0x3B;                 // Primera medida del acelerometro
static uint8_t acel_buffer[buf_size] = {0};
static uint8_t yoquese[2] = {0x37, 0x02};          // se supone que habilita al magnetometro--------------------------------
static uint8_t powermode_mag[2] = {0x0A, 0x00};    // powermode magnetometro
static uint8_t sensibilidad_mag[2] = {0x0A, 0x12}; // sensibilidad del magnetometro
static uint8_t lectura_mag = 0x02;                 // Primera medida del magnetometro
static uint8_t mag_buffer[8] = {0};

static float MagX, MagY, MagZ; // Declaro las variabels del magnetómetro
static float RateMagX, RateMagY, RateMagZ;
static float Brujula;                      // Declaro el ángulo respecto al norte
static float RateRoll, RatePitch, RateYaw; // Declaro las variables del giroscopio
static float AccX, AccY, AccZ;             // Declaro las variables del acelerometro
static float AngleRoll, AnglePitch;        // Declaro los angulos respecto al eje
// Declaro las variables de calibracion
static float RateCalibrationRoll, RateCalibrationPitch, RateCalibrationYaw;
static float RateCalibrationAccX, RateCalibrationAccY, RateCalibrationAccZ;
static float RateCalibrationMagX, RateCalibrationMagY, RateCalibrationMagZ;
//-----------------------------------------------------------------------------------------------------------------
static float A[3][3] = {{2.570416, 0.134024, -0.054982}, {0.134024, 2.777057, 0.022216}, {-0.054982, 0.114052, 2.843312}}; // Corrección de hierro dulce y desalineación (fila, columna)
static float B[3] = {-6.243407, 36.634592, 15.059727};                                                                     // Corrección de hierro duro
//-----------------------------------------------------------------------------------------------------------------

static const i2c_port_t i2c_master_port = 0;

static void MPU_medidas(void)
{
    // Extraigo medidas del giroscopio
    i2c_master_write_read_device(i2c_master_port, GYRO_ADDR, &lectura_giro, 1, giro_buffer, buf_size, pdMS_TO_TICKS(60));

    int16_t GyroX = giro_buffer[0] << 8 | giro_buffer[1];
    int16_t GyroY = giro_buffer[2] << 8 | giro_buffer[3];
    int16_t GyroZ = giro_buffer[4] << 8 | giro_buffer[5];
    // Convierto las medidas a grados/segundos
    RateRoll = -(float)GyroX / 65.5;
    RatePitch = (float)GyroY / 65.5;
    RateYaw = (float)GyroZ / 65.5;

    // Extraigo medidas del acelerometro
    i2c_master_write_read_device(i2c_master_port, GYRO_ADDR, &lectura_acel, 1, acel_buffer, buf_size, pdMS_TO_TICKS(60));
    int16_t AccXLSB = acel_buffer[0] << 8 | acel_buffer[1];
    int16_t AccYLSB = acel_buffer[2] << 8 | acel_buffer[3];
    int16_t AccZLSB = acel_buffer[4] << 8 | acel_buffer[5];
    // Convierto las medidas del acelerometro
    AccX = (float)AccXLSB / 4096;
    AccY = (float)AccYLSB / 4096;
    AccZ = -(float)AccZLSB / 4096;

    // Extraigo medidas del magnetómetro
    i2c_master_write_read_device(i2c_master_port, MAG_ADDR, &lectura_mag, 1, mag_buffer, 8, pdMS_TO_TICKS(125));
    if (mag_buffer[0] & 0x01)
    {
        int16_t Xraw = mag_buffer[2] << 8 | mag_buffer[1];
        int16_t Yraw = mag_buffer[4] << 8 | mag_buffer[3];
        int16_t Zraw = mag_buffer[6] << 8 | mag_buffer[5];
        // Convierto las medidas del magnetómetro
        RateMagX = (float)Xraw / 6.67;
        RateMagY = (float)Yraw / 6.67;
        RateMagZ = (float)Zraw / 6.67;
    }
    RateCalibrationMagX = RateMagX - B[0];
    RateCalibrationMagY = RateMagY - B[1];
    RateCalibrationMagZ = RateMagZ - B[2];

    MagX = A[0][0] * RateCalibrationMagX + A[0][1] * RateCalibrationMagY + A[0][2] * RateCalibrationMagZ;
    MagY = A[1][0] * RateCalibrationMagX + A[1][1] * RateCalibrationMagY + A[1][2] * RateCalibrationMagZ;
    MagZ = A[2][0] * RateCalibrationMagX + A[2][1] * RateCalibrationMagY + A[2][2] * RateCalibrationMagZ;
}

void Calibracion(void)
{
    for (int i = 0; i < promedio; i++)
    {
        MPU_medidas();
        RateCalibrationRoll += RateRoll;
        RateCalibrationPitch += RatePitch;
        RateCalibrationYaw += RateYaw;

        RateCalibrationAccX += AccX;
        RateCalibrationAccY += AccY;
        RateCalibrationAccZ += AccZ;

        vTaskDelay(pdMS_TO_TICKS(1));
    }
    RateCalibrationRoll /= promedio;
    RateCalibrationPitch /= promedio;
    RateCalibrationYaw /= promedio;
    RateCalibrationAccX /= promedio;
    RateCalibrationAccY /= promedio;
    RateCalibrationAccZ = (RateCalibrationAccZ / promedio) - 1;
}

void xMPU9250ProcessingTask(void *arg)
{
    mpu9250_data_t datos = {
        .nivel = 0.00f,
        .buzamiento = 0.00f,
        .dir_buzamiento = 0.00f,
    };
    FilterTypeDef Nivel_Filter;
    FilterTypeDef Buzamiento_Filter;
    FilterTypeDef Dir_Buzamiento_Filter;
    Moving_Average_Init(&Nivel_Filter);
    Moving_Average_Init(&Buzamiento_Filter);
    Moving_Average_Init(&Dir_Buzamiento_Filter);
    uint32_t notified_value = 0U;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(125);
    for (;;)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        MPU_medidas();

        //-----------------------------------------------------------------------------------------------------------------
        // PAGINA DE CALIBRACION https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml#igrfwmm
        // Latitude: 38.732326579428076   S
        // Longitude: 62.27987319700427   W
        // Mean sea level 18 Meters
        // Total field 23.5304 uT
        // plot-calibration-data.py

        Brujula = -atan2(MagX, MagY) * (180.0 / M_PI);
        if (Brujula < 0)
        {
            Brujula = 360 + Brujula;
        }

        RateRoll -= RateCalibrationRoll;
        RatePitch -= RateCalibrationPitch;
        RateYaw -= RateCalibrationYaw;

        AccX -= RateCalibrationAccX;
        AccY -= RateCalibrationAccY;
        AccZ -= RateCalibrationAccZ;

        // Calculo e imprimo los angulos que forman con respecto al eje Z
        AngleRoll = atan(AccY / sqrt(AccX * AccX + AccZ * AccZ)) * 1 / (M_PI / 180);  // buzamiento
        AnglePitch = atan(AccX / sqrt(AccY * AccY + AccZ * AccZ)) * 1 / (M_PI / 180); // nivel

        // Send MPU data
        datos.dir_buzamiento = Moving_Average_Compute(Brujula, &Dir_Buzamiento_Filter);
        datos.buzamiento = Moving_Average_Compute(AngleRoll, &Buzamiento_Filter);
        datos.nivel = Moving_Average_Compute(AnglePitch, &Nivel_Filter);
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &notified_value, pdMS_TO_TICKS(5));
        if ((notified_value & 0x03) != 0)
            xQueueSendToFront(xDisplayQueueA, &datos, pdMS_TO_TICKS(5));
        taskYIELD();
    }
}

esp_err_t mpu9250_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN, // select GPIO specific to your project
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = SCL_PIN, // select GPIO specific to your project
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000, // select frequency specific to your project
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };

    if (i2c_param_config(i2c_master_port, &conf) != ESP_OK)
        return ESP_FAIL;

    if (i2c_driver_install(i2c_master_port, I2C_MODE_MASTER, 0, 0, ESP_INTR_FLAG_LEVEL1) != ESP_OK)
    {
        i2c_driver_delete(i2c_master_port);
        return ESP_FAIL;
    }
    i2c_master_write_to_device(i2c_master_port, GYRO_ADDR, powermode_gyro, 2, pdMS_TO_TICKS(100));
    i2c_master_write_to_device(i2c_master_port, GYRO_ADDR, comando_filtro, 2, pdMS_TO_TICKS(100));
    i2c_master_write_to_device(i2c_master_port, GYRO_ADDR, sensibilidad_giro, 2, pdMS_TO_TICKS(100));
    i2c_master_write_to_device(i2c_master_port, GYRO_ADDR, sensibilidad_acel, 2, pdMS_TO_TICKS(100));

    Calibracion();
    //  Inicio del magnetometro
    //  https://www.luisllamas.es/usar-arduino-con-los-imu-de-9dof-mpu-9150-y-mpu-9250/
    i2c_master_write_to_device(i2c_master_port, GYRO_ADDR, yoquese, 2, pdMS_TO_TICKS(100));
    i2c_master_write_to_device(i2c_master_port, MAG_ADDR, powermode_mag, 2, pdMS_TO_TICKS(100));
    i2c_master_write_to_device(i2c_master_port, MAG_ADDR, sensibilidad_mag, 2, pdMS_TO_TICKS(100));

    xMPU9250Queue = xQueueCreate(MPU9250_DATA_QUEUE_SIZE, sizeof(mpu9250_data_t));
    BaseType_t err = xTaskCreate(
        xMPU9250ProcessingTask,
        "mpu_task",
        MPU_TASK_SIZE,
        NULL,
        MPU9250_TASK_PRIORITY,
        &xMPU9250ProcessingTaskHandle);
    if (err != pdTRUE)
        return ESP_FAIL;

    return ESP_OK;
}

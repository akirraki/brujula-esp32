
void xMPUCalTask(void *pvParameter)
{
    for (;;)
    {
        xTaskNotifyWait(pdFALSE, ULONG_MAX, &xNotifiedValue, portMAX_DELAY);
        if (xSemaphoreTake(xI2CMutex, portMAX_DELAY) == pdPASS)
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
            }
            RateCalibrationRoll /= promedio;
            RateCalibrationPitch /= promedio;
            RateCalibrationYaw /= promedio;
            RateCalibrationAccX /= promedio;
            RateCalibrationAccY /= promedio;
            RateCalibrationAccZ = (RateCalibrationAccZ / promedio) - 1;
            xSemaphoreGive(xI2CMutex);
        }
    }
}
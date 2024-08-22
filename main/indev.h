#pragma once

extern lv_disp_t *my_diplay;
extern TaskHandle_t xSwTaskHandle;
extern QueueHandle_t xKeypadQueue;
extern lv_group_t *my_group;
extern lv_disp_t *my_diplay;
extern lv_indev_drv_t indev_drv;
void indev_init(void);
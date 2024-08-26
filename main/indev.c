#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ili9341-lvgl-solution.h"
#include "ui.h"
#include "botonera.h"
#include "mpu9250.h"
#include "indev.h"

#define R_FLAG (0x01)
#define L_FLAG (0x02)
#define ENTER_FLAG (0x03)

lv_group_t *my_group;
lv_indev_drv_t indev_drv;

#define SCR1_OBJ_AMOUNT 1
#define SCR3_OBJ_AMOUNT 3

static lv_obj_t *scr1_objects[SCR1_OBJ_AMOUNT];
static lv_obj_t *scr3_objects[SCR3_OBJ_AMOUNT];

typedef struct pulsador_t
{
    botonera_id_t button_id;
    uint8_t group_id;
    uint8_t isLongPress;

} pulsador_t;

typedef enum
{
    SCREEN1 = 1,
    SCREEN2,
    SCREEN3,
} screens_t;

// This one's for the up/down/enter keys only
static void button_clicked_event_cb1(void *arg, void *data)
{
    uint32_t key_pressed = (botonera_id_t)data;
    xQueueSendToBack(xKeypadQueue, &key_pressed, 0);
}

// this one's for left/right keys only
static void button_clicked_event_cb2(void *arg, void *data)
{
    uint32_t key_pressed = (botonera_id_t)data;
    switch (key_pressed)
    {
    case DERECHA:
    {
        xTaskNotify(xSwTaskHandle, R_FLAG, eSetBits);
        break;
    }
    case IZQUIERDA:
    {
        xTaskNotify(xSwTaskHandle, L_FLAG, eSetBits);
        break;
    }
    default:
        break;
    }
}
// This one's called when pressing and holding the enter key only
static void button_longpress_event_cb(void *arg, void *data)
{
    uint32_t key_pressed = (botonera_id_t)data;
}

static void xSwitchScreenTask(void *pvParameter);
TaskHandle_t xSwTaskHandle = NULL;
QueueHandle_t xKeypadQueue = NULL;

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

static void add_lvgl_objects(lv_obj_t **obj_arr, uint8_t n)
{
    // Si te llaman es que ya tienen el mutex
    for (uint8_t i = 0; i < n; i++)
    {
        lv_group_add_obj(my_group, obj_arr[i]);
    }
}

static void xSwitchScreenTask(void *pvParameter)
{
    BaseType_t result;
    static uint32_t notifiedValue = 0;
    static screens_t screenContext = SCREEN1;
    for (;;)
    {
        result = xTaskNotifyWait(pdFALSE,
                                 ULONG_MAX,
                                 &notifiedValue,
                                 portMAX_DELAY);
        if (result == pdPASS)
        {
            if (lvgl_lock(250))
            {
                if ((notifiedValue & R_FLAG) != 0)
                {
                    lv_group_remove_all_objs(my_group);
                    switch (screenContext)
                    {
                    case SCREEN1:
                    {
                        screenContext = SCREEN2;
                        lv_event_send(ui_De1a2, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    case SCREEN2:
                    {
                        screenContext = SCREEN3;
                        add_lvgl_objects(scr3_objects, SCR3_OBJ_AMOUNT);
                        lv_event_send(ui_De2a3, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    case SCREEN3:
                    {
                        screenContext = SCREEN1;
                        add_lvgl_objects(scr1_objects, SCR1_OBJ_AMOUNT);
                        lv_event_send(ui_De3a1, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    default:
                        break;
                    }
                }

                if ((notifiedValue & L_FLAG) != 0)
                {
                    lv_group_remove_all_objs(my_group);
                    switch (screenContext)
                    {
                    case SCREEN1:
                    {
                        screenContext = SCREEN3;
                        add_lvgl_objects(scr3_objects, SCR3_OBJ_AMOUNT);
                        lv_event_send(ui_De1a3, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    case SCREEN2:
                    {
                        screenContext = SCREEN1;
                        add_lvgl_objects(scr1_objects, SCR1_OBJ_AMOUNT);
                        lv_event_send(ui_De2a1, LV_EVENT_CLICKED, NULL);
                        break;
                    }
                    case SCREEN3:
                    {
                        screenContext = SCREEN2;
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

void indev_init(void)
{
    xKeypadQueue = xQueueCreate(16, sizeof(uint32_t));
    xTaskCreate(xSwitchScreenTask,
                "screensTask",
                1024,
                NULL,
                2,
                &xSwTaskHandle);
    // el compilador no me deja inicializar arreglos con variables
    // tampoco me deja inicializar arreglos si su tamaño esta dado
    // por una const, reemplazando por un #define con el tamaño lo resuelve
    // en fin, el gcc maneja el patrullero
    // muy mala solucion:
    scr1_objects[0] = ui_Medir;
    scr3_objects[0] = ui_Wifi;
    scr3_objects[1] = ui_Calibrar;
    scr3_objects[2] = ui_Borrar_medidas;

    button_init(PIN_BOTON_ENTER, ENTER, BUTTON_SINGLE_CLICK, button_clicked_event_cb1);
    button_init(PIN_BOTON_ENTER, ENTER, BUTTON_LONG_PRESS_START, button_longpress_event_cb);
    button_init(PIN_BOTON_ARRIBA, ARRIBA, BUTTON_SINGLE_CLICK, button_clicked_event_cb1);
    button_init(PIN_BOTON_ABAJO, ABAJO, BUTTON_SINGLE_CLICK, button_clicked_event_cb1);
    button_init(PIN_BOTON_DER, DERECHA, BUTTON_SINGLE_CLICK, button_clicked_event_cb2);
    button_init(PIN_BOTON_IZQ, IZQUIERDA, BUTTON_SINGLE_CLICK, button_clicked_event_cb2);

    if (lvgl_lock(-1))
    {
        lv_indev_drv_init(&indev_drv); /*Basic initialization*/
        indev_drv.type = LV_INDEV_TYPE_KEYPAD;
        indev_drv.disp = my_diplay;
        indev_drv.read_cb = keyboard_read;
        /*Register the driver in LVGL and save the created input device object*/
        lv_indev_t *my_indev = lv_indev_drv_register(&indev_drv);

        my_group = lv_group_create();
        add_lvgl_objects(scr1_objects, SCR1_OBJ_AMOUNT);
        lv_indev_set_group(my_indev, my_group);
        lvgl_unlock();
    }
}
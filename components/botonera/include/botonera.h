#ifndef BOTONERA_H
#define BOTONERA_H

#include "iot_button.h"
typedef enum
{
    ARRIBA = 0,
    ABAJO,
    IZQUIERDA,
    DERECHA,
    ENTER,
} botonera_id_t;

#define PIN_BOTON_ENTER 27
#define PIN_BOTON_DER 33
#define PIN_BOTON_IZQ 26
#define PIN_BOTON_ARRIBA 14
#define PIN_BOTON_ABAJO 25
#define BUTTON_ACTIVE_LEVEL 0

button_handle_t my_button_init(uint32_t button_num, botonera_id_t id, button_event_t event, button_cb_t callback);

#endif

#ifndef BOTONERA_H
#define BOTONERA_H

typedef enum
{
    ARRIBA = 0,
    ABAJO,
    IZQUIERDA,
    DERECHA,
    ENTER,
} botonera_id_t;

#define PIN_BOTON_ENTER 27
#define PIN_BOTON_DER 1
#define PIN_BOTON_IZQ 26
#define PIN_BOTON_ARRIBA 14
#define PIN_BOTON_ABAJO 25
#define BUTTON_ACTIVE_LEVEL 0

// static void button_event_cb(void *arg, void *data);
void button_init(uint32_t button_num, botonera_id_t id);

#endif

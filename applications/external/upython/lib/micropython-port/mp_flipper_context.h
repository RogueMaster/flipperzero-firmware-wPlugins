#include <stdint.h>

#include <furi.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include <mp_flipper_modflipperzero.h>

#define MP_FLIPPER_GPIO_PIN_OFF     (1 << 15)
#define MP_FLIPPER_GPIO_PIN_BLOCKED (1 << 7)
#define MP_FLIPPER_GPIO_PIN_PWM     ((MP_FLIPPER_GPIO_PIN_BLOCKED) | (1 << 8))

typedef uint16_t mp_flipper_gpio_pin_t;

#define MP_FLIPPER_INFRARED_RX_BUFFER_SIZE (1024)

typedef struct {
    uint16_t size;
    uint32_t* buffer;
    uint16_t pointer;
    bool running;
} mp_flipper_infrared_rx_t;

typedef struct {
    size_t size;
    void* signal;
    size_t index;
    uint32_t repeat;
    bool level;
    mp_flipper_infrared_signal_tx_provider provider;
} mp_flipper_infrared_tx_t;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    Canvas* canvas;
    FuriPubSub* input_event_queue;
    FuriPubSubSubscription* input_event;
    DialogMessage* dialog_message;
    const char* dialog_message_button_left;
    const char* dialog_message_button_center;
    const char* dialog_message_button_right;
    Storage* storage;
    FuriHalAdcHandle* adc_handle;
    mp_flipper_gpio_pin_t* gpio_pins;
    mp_flipper_infrared_rx_t* infrared_rx;
    mp_flipper_infrared_tx_t* infrared_tx;
} mp_flipper_context_t;

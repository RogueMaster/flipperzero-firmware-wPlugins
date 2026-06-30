#ifndef GPIO_HEADERS
#define GPIO_HEADERS

#include "flipper.h"
#include "logic_general.h"

typedef enum {
    Demo,
    GPIO,
    __lwc_number_of_run_modes
} LWCRunMode;

typedef enum {
    Regular,
    Inverted,
    __lwc_number_of_data_modes
} LWCDataMode;

typedef enum {
    GPIOPinA7,
    GPIOPinA4,
    GPIOPinB2,
    GPIOPinC1,
    GPIOPinC0,
    __lwc_number_of_data_pins
} LWCDataPin;

typedef struct {
    const GpioPin* pin;
    void* context;
    bool inverted;
    FuriMessageQueue* queue;
} GPIOContext;

typedef struct {
    bool shift_up;
    uint32_t time_passed_up;
    uint32_t time_passed_down;
} GPIOEvent;

GPIOContext*
    gpio_start_listening(const LWCDataPin data_pin, bool inverted, void* callback_context);
void gpio_stop_listening(GPIOContext* context);
bool gpio_callback_with_event(GPIOContext* context, void (*callback)(GPIOEvent, void*));

#endif

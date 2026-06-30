#include "gpio.h"

uint32_t last_switch = 0;
uint32_t last_time_up_tick = 0;
uint32_t last_time_down_tick = 0;

static void gpio_interrupt_callback(void* context) {
    furi_assert(context);
    GPIOContext* gpio_context = context;

    uint32_t current_tick = furi_get_tick();

    if(last_switch > 0) {
        bool shift_up = furi_hal_gpio_read(gpio_context->pin) ^ gpio_context->inverted;

        if(shift_up) {
            last_time_down_tick = current_tick - last_switch;
        } else {
            last_time_up_tick = current_tick - last_switch;
        }

        GPIOEvent event = {
            .shift_up = shift_up,
            .time_passed_down = last_time_down_tick,
            .time_passed_up = last_time_up_tick};
        furi_message_queue_put(gpio_context->queue, &event, 0);
    }

    last_switch = current_tick;
}

static const GpioPin* data_pins_to_gpio_pins[] =
    {&gpio_ext_pa7, &gpio_ext_pa4, &gpio_ext_pb2, &gpio_ext_pc1, &gpio_ext_pc0};

GPIOContext*
    gpio_start_listening(const LWCDataPin data_pin, bool inverted, void* callback_context) {
    GPIOContext* result = malloc(sizeof(GPIOContext));

    result->inverted = inverted;
    result->pin = data_pins_to_gpio_pins[data_pin];
    result->context = callback_context;
    result->queue = furi_message_queue_alloc(32, sizeof(GPIOEvent));

    furi_hal_gpio_add_int_callback(result->pin, gpio_interrupt_callback, result);
    furi_hal_gpio_enable_int_callback(result->pin);
    furi_hal_gpio_init(result->pin, GpioModeInterruptRiseFall, GpioPullUp, GpioSpeedVeryHigh);

    return result;
}

void gpio_stop_listening(GPIOContext* context) {
    furi_hal_gpio_disable_int_callback(context->pin);
    furi_hal_gpio_remove_int_callback(context->pin);
    furi_hal_gpio_init_simple(context->pin, GpioModeAnalog);
}

bool gpio_callback_with_event(GPIOContext* context, void (*callback)(GPIOEvent, void*)) {
    GPIOEvent event;
    FuriStatus event_status = furi_message_queue_get(context->queue, &event, 0);

    if(event_status == FuriStatusOk) {
        callback(event, context->context);
        return true;
    } else {
        return false;
    }
}

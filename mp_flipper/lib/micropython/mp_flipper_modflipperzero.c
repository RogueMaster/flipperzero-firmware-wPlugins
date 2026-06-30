#include <stdint.h>
#include <stdio.h>

#include "py/mperrno.h"
#include "py/objint.h"
#include "py/objfun.h"
#include "py/obj.h"
#include "py/stream.h"
#include "py/runtime.h"
#include <string.h>

#include "mp_flipper_modflipperzero.h"

static mp_obj_t flipperzero_light_set(mp_obj_t light_obj, mp_obj_t brightness_obj) {
    mp_int_t light = mp_obj_get_int(light_obj);
    mp_int_t brightness = mp_obj_get_int(brightness_obj);

    mp_flipper_light_set(light, brightness);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(flipperzero_light_set_obj, flipperzero_light_set);

static mp_obj_t flipperzero_light_blink_start(size_t n_args, const mp_obj_t* args) {
    if(n_args != 4) {
        return mp_const_none;
    }

    mp_int_t light = mp_obj_get_int(args[0]);
    mp_int_t brightness = mp_obj_get_int(args[1]);
    mp_int_t on_time = mp_obj_get_int(args[2]);
    mp_int_t period = mp_obj_get_int(args[3]);

    mp_flipper_light_blink_start(light, brightness, on_time, period);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_light_blink_start_obj, 4, 4, flipperzero_light_blink_start);

static mp_obj_t flipperzero_light_blink_stop() {
    mp_flipper_light_blink_stop();

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_light_blink_stop_obj, flipperzero_light_blink_stop);

static mp_obj_t flipperzero_light_blink_set_color(mp_obj_t light_obj) {
    mp_int_t light = mp_obj_get_int(light_obj);

    mp_flipper_light_blink_set_color(light);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_light_blink_set_color_obj, flipperzero_light_blink_set_color);

static mp_obj_t flipperzero_vibro_set(mp_obj_t state) {
    bool state_bool = mp_obj_is_true(state);

    mp_flipper_vibro(state_bool);

    return state_bool ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_vibro_set_obj, flipperzero_vibro_set);

typedef struct _mp_obj_float_t {
    mp_obj_base_t base;
    mp_float_t value;
} mp_obj_float_t;

static const struct _mp_obj_float_t flipperzero_speaker_volume_min_obj = {
    {&mp_type_float},
    (mp_float_t)MP_FLIPPER_SPEAKER_VOLUME_MIN};
static const struct _mp_obj_float_t flipperzero_speaker_volume_max_obj = {
    {&mp_type_float},
    (mp_float_t)MP_FLIPPER_SPEAKER_VOLUME_MAX};

static mp_obj_t flipperzero_speaker_start(mp_obj_t frequency_obj, mp_obj_t volume_obj) {
    mp_float_t frequency = mp_obj_get_float(frequency_obj);
    mp_float_t volume = mp_obj_get_float(volume_obj);

    return mp_flipper_speaker_start(frequency, volume) ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_2(flipperzero_speaker_start_obj, flipperzero_speaker_start);

static mp_obj_t flipperzero_speaker_set_volume(mp_obj_t volume_obj) {
    mp_float_t volume = mp_obj_get_float(volume_obj);

    return mp_flipper_speaker_set_volume(volume) ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_speaker_set_volume_obj, flipperzero_speaker_set_volume);

static mp_obj_t flipperzero_speaker_stop() {
    mp_flipper_speaker_stop();

    return mp_flipper_speaker_stop() ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_speaker_stop_obj, flipperzero_speaker_stop);

static mp_obj_t flipperzero_canvas_width() {
    uint8_t width = mp_flipper_canvas_width();

    return mp_obj_new_int(width);
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_canvas_width_obj, flipperzero_canvas_width);

static mp_obj_t flipperzero_canvas_height() {
    uint8_t height = mp_flipper_canvas_height();

    return mp_obj_new_int(height);
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_canvas_height_obj, flipperzero_canvas_height);

static mp_obj_t flipperzero_canvas_text_width(mp_obj_t text_obj) {
    const char* text = mp_obj_str_get_str(text_obj);

    uint8_t width = mp_flipper_canvas_text_width(text);

    return mp_obj_new_int(width);
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_canvas_text_width_obj, flipperzero_canvas_text_width);

static mp_obj_t flipperzero_canvas_text_height() {
    uint8_t height = mp_flipper_canvas_text_height();

    return mp_obj_new_int(height);
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_canvas_text_height_obj, flipperzero_canvas_text_height);

static mp_obj_t flipperzero_canvas_draw_dot(mp_obj_t x_obj, mp_obj_t y_obj) {
    mp_int_t x = mp_obj_get_int(x_obj);
    mp_int_t y = mp_obj_get_int(y_obj);

    mp_flipper_canvas_draw_dot(x, y);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(flipperzero_canvas_draw_dot_obj, flipperzero_canvas_draw_dot);

static mp_obj_t flipperzero_canvas_draw_box(size_t n_args, const mp_obj_t* args) {
    if(n_args < 4) {
        return mp_const_none;
    }

    mp_int_t x = mp_obj_get_int(args[0]);
    mp_int_t y = mp_obj_get_int(args[1]);
    mp_int_t width = mp_obj_get_int(args[2]);
    mp_int_t height = mp_obj_get_int(args[3]);
    mp_int_t radius = n_args == 5 ? mp_obj_get_int(args[4]) : 0;

    mp_flipper_canvas_draw_box(x, y, width, height, radius);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_canvas_draw_box_obj, 4, 5, flipperzero_canvas_draw_box);

static mp_obj_t flipperzero_canvas_draw_frame(size_t n_args, const mp_obj_t* args) {
    if(n_args < 4) {
        return mp_const_none;
    }

    mp_int_t x = mp_obj_get_int(args[0]);
    mp_int_t y = mp_obj_get_int(args[1]);
    mp_int_t width = mp_obj_get_int(args[2]);
    mp_int_t height = mp_obj_get_int(args[3]);
    mp_int_t radius = n_args == 5 ? mp_obj_get_int(args[4]) : 0;

    mp_flipper_canvas_draw_frame(x, y, width, height, radius);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_canvas_draw_frame_obj, 4, 5, flipperzero_canvas_draw_frame);

static mp_obj_t flipperzero_canvas_draw_line(size_t n_args, const mp_obj_t* args) {
    if(n_args != 4) {
        return mp_const_none;
    }

    mp_int_t x0 = mp_obj_get_int(args[0]);
    mp_int_t y0 = mp_obj_get_int(args[1]);
    mp_int_t x1 = mp_obj_get_int(args[2]);
    mp_int_t y1 = mp_obj_get_int(args[3]);

    mp_flipper_canvas_draw_line(x0, y0, x1, y1);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_canvas_draw_line_obj, 4, 4, flipperzero_canvas_draw_line);

static mp_obj_t flipperzero_canvas_draw_circle(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t r_obj) {
    mp_int_t x = mp_obj_get_int(x_obj);
    mp_int_t y = mp_obj_get_int(y_obj);
    mp_int_t r = mp_obj_get_int(r_obj);

    mp_flipper_canvas_draw_circle(x, y, r);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(flipperzero_canvas_draw_circle_obj, flipperzero_canvas_draw_circle);

static mp_obj_t flipperzero_canvas_draw_disc(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t r_obj) {
    mp_int_t x = mp_obj_get_int(x_obj);
    mp_int_t y = mp_obj_get_int(y_obj);
    mp_int_t r = mp_obj_get_int(r_obj);

    mp_flipper_canvas_draw_disc(x, y, r);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(flipperzero_canvas_draw_disc_obj, flipperzero_canvas_draw_disc);

static mp_obj_t flipperzero_canvas_set_font(mp_obj_t font_obj) {
    mp_int_t font = mp_obj_get_int(font_obj);

    mp_flipper_canvas_set_font(font);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_canvas_set_font_obj, flipperzero_canvas_set_font);

static mp_obj_t flipperzero_canvas_set_color(mp_obj_t color_obj) {
    mp_int_t color = mp_obj_get_int(color_obj);

    mp_flipper_canvas_set_color(color);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_canvas_set_color_obj, flipperzero_canvas_set_color);

static mp_obj_t flipperzero_canvas_set_text(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t str_obj) {
    mp_int_t x = mp_obj_get_int(x_obj);
    mp_int_t y = mp_obj_get_int(y_obj);

    const char* str = mp_obj_str_get_str(str_obj);

    mp_flipper_canvas_set_text(x, y, str);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(flipperzero_canvas_set_text_obj, flipperzero_canvas_set_text);

static mp_obj_t flipperzero_canvas_set_text_align(mp_obj_t x_obj, mp_obj_t y_obj) {
    mp_int_t x = mp_obj_get_int(x_obj);
    mp_int_t y = mp_obj_get_int(y_obj);

    mp_flipper_canvas_set_text_align(x, y);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(flipperzero_canvas_set_text_align_obj, flipperzero_canvas_set_text_align);

static mp_obj_t flipperzero_canvas_update() {
    mp_flipper_canvas_update();

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_canvas_update_obj, flipperzero_canvas_update);

static mp_obj_t flipperzero_canvas_clear() {
    mp_flipper_canvas_clear();

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_canvas_clear_obj, flipperzero_canvas_clear);

static void* mp_flipper_on_input_callback = NULL;

static mp_obj_t flipperzero_on_input(mp_obj_t callback_obj) {
    mp_flipper_on_input_callback = callback_obj;

    return callback_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_on_input_obj, flipperzero_on_input);

static mp_obj_t flipperzero_input_trigger_handler(mp_obj_t flags_obj) {
    if(mp_flipper_on_input_callback != NULL) {
        mp_int_t flags = mp_obj_get_int(flags_obj);

        mp_obj_t button_obj = mp_obj_new_int(flags & MP_FLIPPER_INPUT_BUTTON);
        mp_obj_t type_obj = mp_obj_new_int(flags & MP_FLIPPER_INPUT_TYPE);

        mp_call_function_2_protected(mp_flipper_on_input_callback, button_obj, type_obj);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_input_trigger_handler_obj, flipperzero_input_trigger_handler);

static mp_obj_t flipperzero_dialog_message_set_text(size_t n_args, const mp_obj_t* args) {
    if(n_args < 3) {
        return mp_const_none;
    }

    const char* text = mp_obj_str_get_str(args[0]);

    mp_int_t x = mp_obj_get_int(args[1]);
    mp_int_t y = mp_obj_get_int(args[2]);
    mp_int_t h = n_args > 3 ? mp_obj_get_int(args[3]) : MP_FLIPPER_ALIGN_BEGIN;
    mp_int_t v = n_args > 4 ? mp_obj_get_int(args[4]) : MP_FLIPPER_ALIGN_BEGIN;

    mp_flipper_dialog_message_set_text(text, x, y, h, v);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_dialog_message_set_text_obj, 3, 5, flipperzero_dialog_message_set_text);

static mp_obj_t flipperzero_dialog_message_set_header(size_t n_args, const mp_obj_t* args) {
    if(n_args < 3) {
        return mp_const_none;
    }

    const char* text = mp_obj_str_get_str(args[0]);

    mp_int_t x = mp_obj_get_int(args[1]);
    mp_int_t y = mp_obj_get_int(args[2]);
    mp_int_t h = n_args > 3 ? mp_obj_get_int(args[3]) : MP_FLIPPER_ALIGN_BEGIN;
    mp_int_t v = n_args > 4 ? mp_obj_get_int(args[4]) : MP_FLIPPER_ALIGN_BEGIN;

    mp_flipper_dialog_message_set_header(text, x, y, h, v);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_dialog_message_set_header_obj, 3, 5, flipperzero_dialog_message_set_header);

static mp_obj_t flipperzero_dialog_message_set_button(mp_obj_t text_obj, mp_obj_t button_obj) {
    const char* text = mp_obj_str_get_str(text_obj);

    mp_int_t button = mp_obj_get_int(button_obj);

    mp_flipper_dialog_message_set_button(text, button);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(flipperzero_dialog_message_set_button_obj, flipperzero_dialog_message_set_button);

static mp_obj_t flipperzero_dialog_message_show() {
    mp_int_t button = mp_flipper_dialog_message_show();

    return mp_obj_new_int(button);
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_dialog_message_show_obj, flipperzero_dialog_message_show);

static mp_obj_t flipperzero_dialog_message_clear() {
    mp_flipper_dialog_message_clear();

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_dialog_message_clear_obj, flipperzero_dialog_message_clear);

static void* mp_flipper_on_gpio_callback = NULL;

static mp_obj_t flipperzero_gpio_init_pin(size_t n_args, const mp_obj_t* args) {
    if(n_args < 2) {
        return mp_const_false;
    }

    mp_int_t pin = mp_obj_get_int(args[0]);
    mp_int_t mode = mp_obj_get_int(args[1]);
    mp_int_t pull = n_args > 2 ? mp_obj_get_int(args[2]) : MP_FLIPPER_GPIO_PULL_NO;
    mp_int_t speed = n_args > 3 ? mp_obj_get_int(args[3]) : MP_FLIPPER_GPIO_SPEED_LOW;

    bool success = mp_flipper_gpio_init_pin(pin, mode, pull, speed);

    return success ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_gpio_init_pin_obj, 2, 4, flipperzero_gpio_init_pin);

static mp_obj_t flipperzero_gpio_deinit_pin(mp_obj_t pin_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);

    mp_flipper_gpio_deinit_pin(pin);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_gpio_deinit_pin_obj, flipperzero_gpio_deinit_pin);

static mp_obj_t flipperzero_gpio_set_pin(mp_obj_t pin_obj, mp_obj_t state_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);
    bool state = mp_obj_is_true(state_obj);

    mp_flipper_gpio_set_pin(pin, state);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(flipperzero_gpio_set_pin_obj, flipperzero_gpio_set_pin);

static mp_obj_t flipperzero_gpio_get_pin(mp_obj_t pin_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);

    bool state = mp_flipper_gpio_get_pin(pin);

    return state ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_gpio_get_pin_obj, flipperzero_gpio_get_pin);

static mp_obj_t flipperzero_on_gpio(mp_obj_t callback_obj) {
    mp_flipper_on_gpio_callback = callback_obj;

    return callback_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_on_gpio_obj, flipperzero_on_gpio);

static mp_obj_t flipperzero_gpio_trigger_handler(mp_obj_t pin_obj) {
    if(mp_flipper_on_gpio_callback != NULL) {
        mp_call_function_1_protected(mp_flipper_on_gpio_callback, pin_obj);
    }

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_gpio_trigger_handler_obj, flipperzero_gpio_trigger_handler);

static mp_obj_t flipperzero_adc_read_pin_value(mp_obj_t pin_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);

    mp_int_t value = mp_flipper_adc_read_pin(pin);

    return mp_obj_new_int(value);
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_adc_read_pin_value_obj, flipperzero_adc_read_pin_value);

static mp_obj_t flipperzero_adc_read_pin_voltage(mp_obj_t pin_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);

    uint16_t value = mp_flipper_adc_read_pin(pin);
    float voltage = mp_flipper_adc_convert_to_voltage(value);

    return mp_obj_new_float(voltage);
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_adc_read_pin_voltage_obj, flipperzero_adc_read_pin_voltage);

static mp_obj_t flipperzero_pwm_start(mp_obj_t pin_obj, mp_obj_t frequency_obj, mp_obj_t duty_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);
    mp_int_t frequency = mp_obj_get_int(frequency_obj);
    mp_int_t duty = mp_obj_get_int(duty_obj);

    bool success = mp_flipper_pwm_start(pin, frequency, duty);

    return success ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_3(flipperzero_pwm_start_obj, flipperzero_pwm_start);

static mp_obj_t flipperzero_pwm_stop(mp_obj_t pin_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);

    mp_flipper_pwm_stop(pin);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_pwm_stop_obj, flipperzero_pwm_stop);

static mp_obj_t flipperzero_pwm_is_running(mp_obj_t pin_obj) {
    mp_int_t pin = mp_obj_get_int(pin_obj);

    return mp_flipper_pwm_is_running(pin) ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_1(flipperzero_pwm_is_running_obj, flipperzero_pwm_is_running);

static mp_obj_t flipperzero_infrared_receive(size_t n_args, const mp_obj_t* args) {
    mp_int_t timeout = n_args > 0 ? mp_obj_get_int(args[0]) : MP_FLIPPER_INFRARED_RX_DEFAULT_TIMEOUT;

    size_t length = 0;
    uint32_t* buffer = mp_flipper_infrared_receive(timeout, &length);
    mp_obj_t* signal = length > 0 ? malloc(length * sizeof(mp_obj_t)) : NULL;

    for(uint16_t i = 0; i < length; i++) {
        signal[i] = mp_obj_new_int(buffer[i]);
    }

    return mp_obj_new_list(length, signal);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_infrared_receive_obj, 0, 1, flipperzero_infrared_receive);

inline static uint32_t flipperzero_infrared_tx_signal_provider(mp_obj_t* signal, const size_t index) {
    return mp_obj_get_int(signal[index]);
}

static mp_obj_t flipperzero_infrared_transmit(size_t n_args, const mp_obj_t* args) {
    size_t length = 0;
    mp_obj_t* signal;

    mp_obj_get_array(args[0], &length, &signal);

    mp_int_t repeat = n_args > 1 ? mp_obj_get_int(args[1]) : 1;
    bool use_external_pin = n_args > 2 ? mp_obj_is_true(args[2]) : false;
    mp_int_t frequency = n_args > 3 ? mp_obj_get_int(args[3]) : MP_FLIPPER_INFRARED_TX_DEFAULT_FREQUENCY;
    mp_float_t duty_cycle = n_args > 4 ? mp_obj_get_float(args[4]) : MP_FLIPPER_INFRARED_TX_DEFAULT_DUTY_CYCLE;

    return mp_flipper_infrared_transmit(
               signal, length, flipperzero_infrared_tx_signal_provider, repeat, frequency, duty_cycle, use_external_pin) ?
               mp_const_true :
               mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(flipperzero_infrared_transmit_obj, 1, 5, flipperzero_infrared_transmit);

static mp_obj_t flipperzero_infrared_is_busy() {
    return mp_flipper_infrared_is_busy() ? mp_const_true : mp_const_false;
}
static MP_DEFINE_CONST_FUN_OBJ_0(flipperzero_infrared_is_busy_obj, flipperzero_infrared_is_busy);

extern const mp_obj_type_t flipperzero_uart_connection_type;

typedef struct _flipperzero_uart_connection_t {
    mp_obj_base_t base;
    void* handle;
    mp_obj_t mode;
    mp_obj_t baud_rate;
} flipperzero_uart_connection_t;

static mp_obj_t flipperzero_uart_open(mp_obj_t raw_mode, mp_obj_t raw_baud_rate) {
    uint8_t mode = mp_obj_get_int(raw_mode);
    uint32_t baud_rate = mp_obj_get_int(raw_baud_rate);

    void* handle = mp_flipper_uart_open(mode, baud_rate);

    if(handle == NULL) {
        mp_flipper_raise_os_error(MP_EBUSY);

        return mp_const_none;
    }

    flipperzero_uart_connection_t* connection =
        mp_obj_malloc_with_finaliser(flipperzero_uart_connection_t, &flipperzero_uart_connection_type);

    connection->handle = handle;
    connection->mode = raw_mode;
    connection->baud_rate = raw_baud_rate;

    return connection;
}
static MP_DEFINE_CONST_FUN_OBJ_2(flipperzero_uart_open_obj, flipperzero_uart_open);

static mp_uint_t flipperzero_uart_read(mp_obj_t self, void* buf, mp_uint_t size, int* errcode) {
    flipperzero_uart_connection_t* connection = MP_OBJ_TO_PTR(self);

    if(connection->handle == NULL) {
        *errcode = MP_EIO;

        return MP_STREAM_ERROR;
    }

    return mp_flipper_uart_read(connection->handle, buf, size, errcode);
}

static mp_uint_t flipperzero_uart_write(mp_obj_t self, const void* buf, mp_uint_t size, int* errcode) {
    flipperzero_uart_connection_t* connection = MP_OBJ_TO_PTR(self);

    if(connection->handle == NULL) {
        *errcode = MP_EIO;

        return MP_STREAM_ERROR;
    }

    return mp_flipper_uart_write(connection->handle, buf, size, errcode);
}

static mp_uint_t flipperzero_uart_ioctl(mp_obj_t self, mp_uint_t request, uintptr_t arg, int* errcode) {
    flipperzero_uart_connection_t* connection = MP_OBJ_TO_PTR(self);

    if(connection->handle == NULL) {
        return 0;
    }

    if(request == MP_STREAM_SEEK) {
        return 0;
    }

    if(request == MP_STREAM_FLUSH) {
        if(!mp_flipper_uart_sync(connection->handle)) {
            *errcode = MP_EIO;

            return MP_STREAM_ERROR;
        }

        return 0;
    }

    if(request == MP_STREAM_CLOSE) {
        if(!mp_flipper_uart_close(connection->handle)) {
            *errcode = MP_EIO;

            connection->handle = NULL;

            return MP_STREAM_ERROR;
        }

        connection->handle = NULL;

        return 0;
    }

    *errcode = MP_EINVAL;

    return MP_STREAM_ERROR;
}

static const mp_map_elem_t flipperzero_uart_connection_locals_dict_table[] = {
    {MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj)},
    {MP_ROM_QSTR(MP_QSTR_readlines), MP_ROM_PTR(&mp_stream_unbuffered_readlines_obj)},
    {MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj)},
    {MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj)},
    {MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj)},
    {MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj)},
    {MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&mp_stream___exit___obj)},
};
static MP_DEFINE_CONST_DICT(flipperzero_uart_connection_locals_dict, flipperzero_uart_connection_locals_dict_table);

static const mp_stream_p_t flipperzero_uart_connection_stream_p = {
    .read = flipperzero_uart_read,
    .write = flipperzero_uart_write,
    .ioctl = flipperzero_uart_ioctl,
    .is_text = false,
};

MP_DEFINE_CONST_OBJ_TYPE(
    flipperzero_uart_connection_type,
    MP_QSTR_UART,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    protocol,
    &flipperzero_uart_connection_stream_p,
    locals_dict,
    &flipperzero_uart_connection_locals_dict);

static const char* notes = "CCDDEFFGGAAB";
static const float base_frequency = 16.3515979;
static const float const_factor = 1.05946309436;

static inline float get_frequency_by_note(const uint8_t octave, const char note, const bool is_sharp) {
    float frequency = base_frequency;

    for(size_t i = 0; i < octave * 12; i++) {
        frequency *= const_factor;
    }

    for(size_t j = 0; j < 12; j++) {
        if(notes[j] == note) {
            frequency *= (is_sharp ? const_factor : 1.0);

            return frequency;
        }

        frequency *= const_factor;
    }

    return -1.0;
}

void flipperzero_module_attr(mp_obj_t self_in, qstr attr, mp_obj_t* dest) {
    if(dest[0] == MP_OBJ_NULL) {
        // load attribute

        const char* attribute = qstr_str(attr);

        if(strstr(attribute, "SPEAKER_NOTE_") == &attribute[0]) {
            size_t len = strlen(attribute);
            uint8_t octave = attribute[len - 1] - '0';
            bool is_sharp = attribute[len - 2] == 'S';
            size_t note_index = len - (is_sharp ? 3 : 2);
            uint8_t i_note = UINT8_MAX;

            float frequency = get_frequency_by_note(octave, attribute[note_index], is_sharp);

            if(octave > 8 || frequency < 0.0) {
                dest[0] = mp_const_none;
            } else {
                dest[0] = mp_obj_new_float(frequency);
            }

            return;
        }
    } else if(dest[1] == MP_OBJ_NULL) {
        // delete attribute
    } else {
        // store attribute
    }
}

static const mp_rom_map_elem_t flipperzero_module_globals_table[] = {
    // light
    {MP_OBJ_NEW_QSTR(MP_QSTR___name__), MP_OBJ_NEW_QSTR(MP_QSTR_flipperzero)},
    {MP_ROM_QSTR(MP_QSTR_LIGHT_RED), MP_ROM_INT(MP_FLIPPER_LED_RED)},
    {MP_ROM_QSTR(MP_QSTR_LIGHT_GREEN), MP_ROM_INT(MP_FLIPPER_LED_GREEN)},
    {MP_ROM_QSTR(MP_QSTR_LIGHT_BLUE), MP_ROM_INT(MP_FLIPPER_LED_BLUE)},
    {MP_ROM_QSTR(MP_QSTR_LIGHT_BACKLIGHT), MP_ROM_INT(MP_FLIPPER_LED_BACKLIGHT)},
    {MP_ROM_QSTR(MP_QSTR_light_set), MP_ROM_PTR(&flipperzero_light_set_obj)},
    {MP_ROM_QSTR(MP_QSTR_light_blink_start), MP_ROM_PTR(&flipperzero_light_blink_start_obj)},
    {MP_ROM_QSTR(MP_QSTR_light_blink_set_color), MP_ROM_PTR(&flipperzero_light_blink_set_color_obj)},
    {MP_ROM_QSTR(MP_QSTR_light_blink_stop), MP_ROM_PTR(&flipperzero_light_blink_stop_obj)},
    // vibro
    {MP_ROM_QSTR(MP_QSTR_vibro_set), MP_ROM_PTR(&flipperzero_vibro_set_obj)},
    // speaker
    {MP_ROM_QSTR(MP_QSTR_SPEAKER_VOLUME_MIN), MP_ROM_PTR(&flipperzero_speaker_volume_min_obj)},
    {MP_ROM_QSTR(MP_QSTR_SPEAKER_VOLUME_MAX), MP_ROM_PTR(&flipperzero_speaker_volume_max_obj)},
    {MP_ROM_QSTR(MP_QSTR_speaker_start), MP_ROM_PTR(&flipperzero_speaker_start_obj)},
    {MP_ROM_QSTR(MP_QSTR_speaker_set_volume), MP_ROM_PTR(&flipperzero_speaker_set_volume_obj)},
    {MP_ROM_QSTR(MP_QSTR_speaker_stop), MP_ROM_PTR(&flipperzero_speaker_stop_obj)},
    // canvas
    {MP_ROM_QSTR(MP_QSTR_CANVAS_BLACK), MP_ROM_INT(MP_FLIPPER_COLOR_BLACK)},
    {MP_ROM_QSTR(MP_QSTR_CANVAS_WHITE), MP_ROM_INT(MP_FLIPPER_COLOR_WHITE)},
    {MP_ROM_QSTR(MP_QSTR_canvas_width), MP_ROM_PTR(&flipperzero_canvas_width_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_height), MP_ROM_PTR(&flipperzero_canvas_height_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_text_width), MP_ROM_PTR(&flipperzero_canvas_text_width_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_text_height), MP_ROM_PTR(&flipperzero_canvas_text_height_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_draw_dot), MP_ROM_PTR(&flipperzero_canvas_draw_dot_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_draw_box), MP_ROM_PTR(&flipperzero_canvas_draw_box_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_draw_frame), MP_ROM_PTR(&flipperzero_canvas_draw_frame_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_draw_line), MP_ROM_PTR(&flipperzero_canvas_draw_line_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_draw_circle), MP_ROM_PTR(&flipperzero_canvas_draw_circle_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_draw_disc), MP_ROM_PTR(&flipperzero_canvas_draw_disc_obj)},
    {MP_ROM_QSTR(MP_QSTR_FONT_PRIMARY), MP_ROM_INT(MP_FLIPPER_FONT_PRIMARY)},
    {MP_ROM_QSTR(MP_QSTR_FONT_SECONDARY), MP_ROM_INT(MP_FLIPPER_FONT_SECONDARY)},
    {MP_ROM_QSTR(MP_QSTR_canvas_set_font), MP_ROM_PTR(&flipperzero_canvas_set_font_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_set_color), MP_ROM_PTR(&flipperzero_canvas_set_color_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_set_text), MP_ROM_PTR(&flipperzero_canvas_set_text_obj)},
    {MP_ROM_QSTR(MP_QSTR_ALIGN_BEGIN), MP_ROM_INT(MP_FLIPPER_ALIGN_BEGIN)},
    {MP_ROM_QSTR(MP_QSTR_ALIGN_CENTER), MP_ROM_INT(MP_FLIPPER_ALIGN_CENTER)},
    {MP_ROM_QSTR(MP_QSTR_ALIGN_END), MP_ROM_INT(MP_FLIPPER_ALIGN_END)},
    {MP_ROM_QSTR(MP_QSTR_canvas_set_text_align), MP_ROM_PTR(&flipperzero_canvas_set_text_align_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_update), MP_ROM_PTR(&flipperzero_canvas_update_obj)},
    {MP_ROM_QSTR(MP_QSTR_canvas_clear), MP_ROM_PTR(&flipperzero_canvas_clear_obj)},
    // input
    {MP_ROM_QSTR(MP_QSTR_on_input), MP_ROM_PTR(&flipperzero_on_input_obj)},
    {MP_ROM_QSTR(MP_QSTR__input_trigger_handler), MP_ROM_PTR(&flipperzero_input_trigger_handler_obj)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_BUTTON_BACK), MP_ROM_INT(MP_FLIPPER_INPUT_BUTTON_BACK)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_BUTTON_OK), MP_ROM_INT(MP_FLIPPER_INPUT_BUTTON_OK)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_BUTTON_LEFT), MP_ROM_INT(MP_FLIPPER_INPUT_BUTTON_LEFT)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_BUTTON_RIGHT), MP_ROM_INT(MP_FLIPPER_INPUT_BUTTON_RIGHT)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_BUTTON_UP), MP_ROM_INT(MP_FLIPPER_INPUT_BUTTON_UP)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_BUTTON_DOWN), MP_ROM_INT(MP_FLIPPER_INPUT_BUTTON_DOWN)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_TYPE_PRESS), MP_ROM_INT(MP_FLIPPER_INPUT_TYPE_PRESS)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_TYPE_RELEASE), MP_ROM_INT(MP_FLIPPER_INPUT_TYPE_RELEASE)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_TYPE_SHORT), MP_ROM_INT(MP_FLIPPER_INPUT_TYPE_SHORT)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_TYPE_LONG), MP_ROM_INT(MP_FLIPPER_INPUT_TYPE_LONG)},
    {MP_ROM_QSTR(MP_QSTR_INPUT_TYPE_REPEAT), MP_ROM_INT(MP_FLIPPER_INPUT_TYPE_REPEAT)},
    // dialog
    {MP_ROM_QSTR(MP_QSTR_dialog_message_set_text), MP_ROM_PTR(&flipperzero_dialog_message_set_text_obj)},
    {MP_ROM_QSTR(MP_QSTR_dialog_message_set_header), MP_ROM_PTR(&flipperzero_dialog_message_set_header_obj)},
    {MP_ROM_QSTR(MP_QSTR_dialog_message_set_button), MP_ROM_PTR(&flipperzero_dialog_message_set_button_obj)},
    {MP_ROM_QSTR(MP_QSTR_dialog_message_show), MP_ROM_PTR(&flipperzero_dialog_message_show_obj)},
    {MP_ROM_QSTR(MP_QSTR_dialog_message_clear), MP_ROM_PTR(&flipperzero_dialog_message_clear_obj)},
    // gpio - pins
    {MP_ROM_QSTR(MP_QSTR_GPIO_PIN_PC0), MP_ROM_INT(MP_FLIPPER_GPIO_PIN_PC0)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PIN_PC1), MP_ROM_INT(MP_FLIPPER_GPIO_PIN_PC1)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PIN_PC3), MP_ROM_INT(MP_FLIPPER_GPIO_PIN_PC3)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PIN_PB2), MP_ROM_INT(MP_FLIPPER_GPIO_PIN_PB2)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PIN_PB3), MP_ROM_INT(MP_FLIPPER_GPIO_PIN_PB3)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PIN_PA4), MP_ROM_INT(MP_FLIPPER_GPIO_PIN_PA4)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PIN_PA6), MP_ROM_INT(MP_FLIPPER_GPIO_PIN_PA6)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PIN_PA7), MP_ROM_INT(MP_FLIPPER_GPIO_PIN_PA7)},
    // gpio - modes
    {MP_ROM_QSTR(MP_QSTR_GPIO_MODE_INPUT), MP_ROM_INT(MP_FLIPPER_GPIO_MODE_INPUT)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_MODE_OUTPUT_PUSH_PULL), MP_ROM_INT(MP_FLIPPER_GPIO_MODE_OUTPUT_PUSH_PULL)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_MODE_OUTPUT_OPEN_DRAIN), MP_ROM_INT(MP_FLIPPER_GPIO_MODE_OUTPUT_OPEN_DRAIN)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_MODE_ANALOG), MP_ROM_INT(MP_FLIPPER_GPIO_MODE_ANALOG)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_MODE_INTERRUPT_RISE), MP_ROM_INT(MP_FLIPPER_GPIO_MODE_INTERRUPT_RISE)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_MODE_INTERRUPT_FALL), MP_ROM_INT(MP_FLIPPER_GPIO_MODE_INTERRUPT_FALL)},
    // gpio - pull
    {MP_ROM_QSTR(MP_QSTR_GPIO_PULL_NO), MP_ROM_INT(MP_FLIPPER_GPIO_PULL_NO)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PULL_UP), MP_ROM_INT(MP_FLIPPER_GPIO_PULL_UP)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_PULL_DOWN), MP_ROM_INT(MP_FLIPPER_GPIO_PULL_DOWN)},
    // gpio - speed
    {MP_ROM_QSTR(MP_QSTR_GPIO_SPEED_LOW), MP_ROM_INT(MP_FLIPPER_GPIO_SPEED_LOW)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_SPEED_MEDIUM), MP_ROM_INT(MP_FLIPPER_GPIO_SPEED_MEDIUM)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_SPEED_HIGH), MP_ROM_INT(MP_FLIPPER_GPIO_SPEED_HIGH)},
    {MP_ROM_QSTR(MP_QSTR_GPIO_SPEED_VERY_HIGH), MP_ROM_INT(MP_FLIPPER_GPIO_SPEED_VERY_HIGH)},
    // gpio - functions
    {MP_ROM_QSTR(MP_QSTR_gpio_init_pin), MP_ROM_PTR(&flipperzero_gpio_init_pin_obj)},
    {MP_ROM_QSTR(MP_QSTR_gpio_deinit_pin), MP_ROM_PTR(&flipperzero_gpio_deinit_pin_obj)},
    {MP_ROM_QSTR(MP_QSTR_gpio_set_pin), MP_ROM_PTR(&flipperzero_gpio_set_pin_obj)},
    {MP_ROM_QSTR(MP_QSTR_gpio_get_pin), MP_ROM_PTR(&flipperzero_gpio_get_pin_obj)},
    {MP_ROM_QSTR(MP_QSTR_on_gpio), MP_ROM_PTR(&flipperzero_on_gpio_obj)},
    {MP_ROM_QSTR(MP_QSTR__gpio_trigger_handler), MP_ROM_PTR(&flipperzero_gpio_trigger_handler_obj)},
    // adc - functions
    {MP_ROM_QSTR(MP_QSTR_adc_read_pin_value), MP_ROM_PTR(&flipperzero_adc_read_pin_value_obj)},
    {MP_ROM_QSTR(MP_QSTR_adc_read_pin_voltage), MP_ROM_PTR(&flipperzero_adc_read_pin_voltage_obj)},
    // pwm - functions
    {MP_ROM_QSTR(MP_QSTR_pwm_start), MP_ROM_PTR(&flipperzero_pwm_start_obj)},
    {MP_ROM_QSTR(MP_QSTR_pwm_stop), MP_ROM_PTR(&flipperzero_pwm_stop_obj)},
    {MP_ROM_QSTR(MP_QSTR_pwm_is_running), MP_ROM_PTR(&flipperzero_pwm_is_running_obj)},
    // infrared - functions
    {MP_ROM_QSTR(MP_QSTR_infrared_receive), MP_ROM_PTR(&flipperzero_infrared_receive_obj)},
    {MP_ROM_QSTR(MP_QSTR_infrared_transmit), MP_ROM_PTR(&flipperzero_infrared_transmit_obj)},
    {MP_ROM_QSTR(MP_QSTR_infrared_is_busy), MP_ROM_PTR(&flipperzero_infrared_is_busy_obj)},
    // UART
    {MP_ROM_QSTR(MP_QSTR_UART), MP_ROM_PTR(&flipperzero_uart_connection_type)},
    {MP_ROM_QSTR(MP_QSTR_UART_MODE_LPUART), MP_ROM_INT(MP_FLIPPER_UART_MODE_LPUART)},
    {MP_ROM_QSTR(MP_QSTR_UART_MODE_USART), MP_ROM_INT(MP_FLIPPER_UART_MODE_USART)},
    {MP_ROM_QSTR(MP_QSTR_uart_open), MP_ROM_PTR(&flipperzero_uart_open_obj)},
};
static MP_DEFINE_CONST_DICT(flipperzero_module_globals, flipperzero_module_globals_table);

const mp_obj_module_t flipperzero_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t*)&flipperzero_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_flipperzero, flipperzero_module);
MP_REGISTER_MODULE_DELEGATION(flipperzero_module, flipperzero_module_attr);

void mp_flipper_on_input(uint16_t button, uint16_t type) {
    if(mp_flipper_on_input_callback != NULL) {
        uint16_t flags = button | type;
        mp_obj_t flags_obj = mp_obj_new_int_from_uint(flags);

        mp_sched_schedule(&flipperzero_input_trigger_handler_obj, flags_obj);
    }
}

void mp_flipper_on_gpio(void* ctx) {
    if(mp_flipper_on_gpio_callback != NULL) {
        mp_obj_t pin_obj = mp_obj_new_int_from_uint((uint8_t)ctx);

        mp_sched_schedule(&flipperzero_gpio_trigger_handler_obj, pin_obj);
    }
}

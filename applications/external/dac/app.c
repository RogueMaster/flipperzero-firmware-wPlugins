#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_pwm.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "AnalogOutput"

typedef enum {
    DacSubmenuIndexVoltage,
    DacSubmenuIndexAbout,
} DacSubmenuIndex;

// Each view is a screen we show the user.
typedef enum {
    DacViewSubmenu, // The menu when the app starts
    DacViewVoltage, // The main screen
    DacViewAbout, // The about screen with directions, link to social channel, etc.
} DacView;

typedef enum {
    DacEventIdSampleAdc, // Custom event to sample the ADC
    DacEventIdUpdatePWM, // Custom event to update the PWM output
    DacEventIdUpdateMcp4725, // Custom event to update the MCP4725 output
} DacEventId;

typedef enum {
    DacVoltageItemFreq,
    DacVoltageItemDuty,
    DacVoltageItemMcp4725,
    DacVoltageItemCount
} DacVoltageItem;

typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    Submenu* submenu; // The application menu
    View* view_voltage; // The main screen
    Widget* widget_about; // The about screen
    FuriTimer* timer; // Timer for redrawing the screen
} DacApp;

typedef struct {
    DacVoltageItem item;
    uint8_t i2c_address;
    bool confirm_i2c_ready;
    bool write_eeprom;
    uint16_t pwm_frequency;
    uint8_t pwm_duty;
    uint16_t mcp_4725_value;
    float adc_value;
    FuriHalAdcHandle* adc_handle;
} VoltageModel;

// For our PWM output we use pin A7, which is FuriHalPwmOutputIdTim1PA7.
const FuriHalPwmOutputId channel_pwm = FuriHalPwmOutputIdTim1PA7;

// For our I2C output we scan the external I2C bus to find a device.
FuriHalI2cBusHandle* i2c_bus = &furi_hal_i2c_handle_external;

// For our ADC input we use pin C3, which is FuriHalAdcChannel4.
const GpioPin* adc_input_gpio = &gpio_ext_pc3;
const FuriHalAdcChannel adc_input_channel = FuriHalAdcChannel4;

/**
 * @brief      Find an I2C device on the external bus.
 * @details    This function scans the external I2C bus to find a device.  
 * Ideally only a MCP4725 would be connected.  It returns the first address of any device found.
 * @return     The address of the device, or 0 if not found.
 */
static uint8_t i2c_find_device() {
    uint8_t addr = 0;
    furi_hal_i2c_acquire(i2c_bus);
    for(uint8_t try_addr = 0x20; try_addr != 0xff; try_addr++) {
        if(furi_hal_i2c_is_device_ready(i2c_bus, try_addr, 10)) {
            addr = try_addr;
            FURI_LOG_D(TAG, "Found device at %02x", addr);
            break;
        }
    }
    if(addr == 0) {
        FURI_LOG_D(TAG, "Failed to find device.");
    }
    furi_hal_i2c_release(i2c_bus);
    return addr;
}

/**
 * @brief      Set the voltage on the MCP4725.
 * @details    This function sets the voltage on the MCP4725.  It writes the value to the DAC register.
 * @param      voltage_model  The voltage model object.
 * @param      amount         The amount to set the voltage register to (0 to 4095).
 */
static void i2c_set_voltage(VoltageModel* voltage_model, uint16_t amount) {
    furi_assert(voltage_model != NULL);
    if(amount > 4095) {
        amount = 4095;
    }

    uint32_t timeout = 100;
    uint8_t buffer[3] = {0x0};

    // We use the 3-bytes to transfer data.
    // There is also a 2-byte transfer which should be able to send the data slightly quicker?
    buffer[0] = 0; /// PD1/PD0 = Normal Mode.
    buffer[0] |= 0b01000000; // Write DAC register.
    if(voltage_model->write_eeprom) {
        // If you set this, the voltage will be set at power up.
        buffer[0] |= 0b01100000; // Write DAC register + EEPROM.
    }
    buffer[1] = amount >> 4;
    buffer[2] = amount << 4;

    // If we don't know the address, we need to find it.
    if(voltage_model->i2c_address == 0) {
        uint8_t addr = i2c_find_device();
        if(addr == 0) {
            FURI_LOG_E(TAG, "Device not ready.");
            return;
        }
        voltage_model->i2c_address = addr;
    }

    furi_hal_i2c_acquire(i2c_bus);

    furi_hal_i2c_tx(i2c_bus, voltage_model->i2c_address, buffer, COUNT_OF(buffer), timeout);

    // If we need to confirm the DAC is ready, we wait for ready bit to get set.
    if(voltage_model->confirm_i2c_ready) {
        for(int i = 0; i < 20; i++) {
            furi_hal_i2c_rx(i2c_bus, voltage_model->i2c_address, buffer, 2, timeout);
            if(buffer[0] & 0x80) { // DAC ready bit.
                break;
            }
            FURI_LOG_D(TAG, "Waiting for DAC to be ready.  %02x %02x", buffer[0], buffer[1]);
        }
    }

    furi_hal_i2c_release(i2c_bus);
}

/**
 * @brief      Callback for exiting the application.
 * @details    Return VIEW_NONE to indicate that we want to exit the application.
 * @param      _context  The context - unused
 * @return     next view id (VIEW_NONE)
*/
static uint32_t navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return DacViewSubmenu to
 *            indicate that we want to navigate to the submenu.
 * @param      _context  The context - unused
 * @return     next view id (DacViewSubmenu)
*/
static uint32_t navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return DacViewSubmenu;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - DacApp object.
 * @param      index     The SubmenuIndex item that was clicked.
*/
static void submenu_selection_callback(void* context, uint32_t index) {
    DacApp* app = (DacApp*)context;
    switch(index) {
    case DacSubmenuIndexVoltage:
        view_dispatcher_switch_to_view(app->view_dispatcher, DacViewVoltage);
        break;
    case DacSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, DacViewAbout);
        break;
    default:
        break;
    }
}

/**
 * @brief      Callback for drawing the voltage screen.
 * @details    This function is called when the screen needs to be redrawn, like when the model gets updated.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
static void voltage_view_draw_callback(Canvas* canvas, void* model) {
    VoltageModel* my_model = (VoltageModel*)model;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 10, 1, AlignLeft, AlignTop, "Voltage");
    canvas_set_font(canvas, FontSecondary);

    FuriString* str = furi_string_alloc();
    furi_string_printf(str, "PWM Freq: %u Hz", my_model->pwm_frequency);
    canvas_draw_str_aligned(canvas, 10, 15, AlignLeft, AlignTop, furi_string_get_cstr(str));

    furi_string_printf(str, "PWM Duty: %u %%", my_model->pwm_duty);
    canvas_draw_str_aligned(canvas, 10, 25, AlignLeft, AlignTop, furi_string_get_cstr(str));

    furi_string_printf(
        str,
        "MCP4725: %u (%u %%)",
        my_model->mcp_4725_value,
        my_model->mcp_4725_value * 100 / 4095);
    canvas_draw_str_aligned(canvas, 10, 35, AlignLeft, AlignTop, furi_string_get_cstr(str));

    furi_string_printf(str, "ADC Input: %1.3f V", (double)(my_model->adc_value / 1000.0f));
    canvas_draw_str_aligned(canvas, 10, 45, AlignLeft, AlignTop, furi_string_get_cstr(str));

    // canvas_draw_str_aligned(canvas, 10, 57, AlignLeft, AlignTop, "CAPS STATUS MESSAGE!!!");

    canvas_draw_str_aligned(canvas, 1, 15 + (10 * my_model->item), AlignLeft, AlignTop, ">");

    furi_string_free(str);
}

/**
 * @brief      Callback for timer elapsed.
 * @details    This function is called when the timer is elapsed.  We use this to queue a redraw event.
 * @param      context  The context - DacApp object.
*/
static void dac_timer_callback(void* context) {
    DacApp* app = (DacApp*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DacEventIdSampleAdc);
}

/**
 * @brief      Callback when the user starts the voltage screen.
 * @details    This function is called when the user enters the voltage screen.  We start a timer to
 *           redraw the screen periodically (so the random number is refreshed).
 * @param      context  The context - DacApp object.
*/
static void voltage_view_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(200);
    DacApp* app = (DacApp*)context;
    furi_assert(app->timer == NULL);
    app->timer = furi_timer_alloc(dac_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer, period);
    view_dispatcher_send_custom_event(app->view_dispatcher, DacEventIdUpdatePWM);
    view_dispatcher_send_custom_event(app->view_dispatcher, DacEventIdUpdateMcp4725);
}

/**
 * @brief      Callback when the user exits the voltage screen.
 * @details    This function is called when the user exits the voltage screen.  We stop the timer.
 * @param      context  The context - DacApp object.
*/
static void voltage_view_exit_callback(void* context) {
    DacApp* app = (DacApp*)context;
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - DacEventId value.
 * @param      context  The context - DacApp object.
*/
static bool voltage_view_custom_event_callback(uint32_t event, void* context) {
    DacApp* app = (DacApp*)context;
    bool redraw = false;

    switch(event) {
    case DacEventIdSampleAdc:
        // Read a new ADC voltage value.
        {
            redraw = true;
            with_view_model(
                app->view_voltage,
                VoltageModel * model,
                {
                    uint16_t adc_value = furi_hal_adc_read(model->adc_handle, adc_input_channel);
                    model->adc_value =
                        furi_hal_adc_convert_to_voltage(model->adc_handle, adc_value);
                },
                redraw);
            return true;
        }
    case DacEventIdUpdatePWM: {
        with_view_model(
            app->view_voltage,
            VoltageModel * model,
            { furi_hal_pwm_set_params(channel_pwm, model->pwm_frequency, model->pwm_duty); },
            redraw);
        return true;
    }
    case DacEventIdUpdateMcp4725: {
        with_view_model(
            app->view_voltage,
            VoltageModel * model,
            { i2c_set_voltage(model, model->mcp_4725_value); },
            redraw);
        return true;
    }
    default:
        return false;
    }
}

/**
 * @brief      Callback for voltage screen input.
 * @details    This function is called when the user presses a button while on the voltage screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - DacApp object.
 * @return     true if the event was handled, false otherwise.
*/
static bool voltage_view_input_callback(InputEvent* event, void* context) {
    DacApp* app = (DacApp*)context;
    if(event->type == InputTypeShort) {
        bool redraw = true;
        if(event->key == InputKeyLeft) {
            // Left button clicked, reduce x coordinate.
            with_view_model(
                app->view_voltage,
                VoltageModel * model,
                {
                    switch(model->item) {
                    case DacVoltageItemFreq:
                        model->pwm_frequency = model->pwm_frequency / 1.5;
                        if(model->pwm_frequency < 1) {
                            model->pwm_frequency = 1;
                        }
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, DacEventIdUpdatePWM);
                        break;
                    case DacVoltageItemDuty:
                        if(model->pwm_duty <= 1) {
                            model->pwm_duty = 1;
                        } else if(model->pwm_duty < 10) {
                            model->pwm_duty = model->pwm_duty - 1;
                        } else {
                            model->pwm_duty = model->pwm_duty - 5;
                        }
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, DacEventIdUpdatePWM);
                        break;
                    case DacVoltageItemMcp4725:
                        if(model->mcp_4725_value <= 0) {
                            model->mcp_4725_value = 0;
                        } else if(model->mcp_4725_value < 20) {
                            model->mcp_4725_value = model->mcp_4725_value - 1;
                        } else if(model->mcp_4725_value < 200) {
                            model->mcp_4725_value = model->mcp_4725_value - 10;
                        } else {
                            model->mcp_4725_value -= 100;
                        }
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, DacEventIdUpdateMcp4725);
                        break;
                    default:
                        break;
                    }
                },
                redraw);
        } else if(event->key == InputKeyRight) {
            // Right button clicked, increase x coordinate.
            with_view_model(
                app->view_voltage,
                VoltageModel * model,
                {
                    switch(model->item) {
                    case DacVoltageItemFreq:
                        if(model->pwm_frequency > 40000) {
                            model->pwm_frequency = 65535;
                        } else {
                            model->pwm_frequency = model->pwm_frequency * 1.5;
                        }
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, DacEventIdUpdatePWM);
                        break;
                    case DacVoltageItemDuty:
                        if((model->pwm_duty >= 90) ||
                           ((model->pwm_duty >= 30 && model->pwm_duty <= 55))) {
                            model->pwm_duty = model->pwm_duty + 1;
                        } else {
                            model->pwm_duty = model->pwm_duty + 5;
                        }
                        if(model->pwm_duty >= 100) {
                            model->pwm_duty = 100;
                        }
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, DacEventIdUpdatePWM);
                        break;
                    case DacVoltageItemMcp4725:
                        if(model->mcp_4725_value >= 4095) {
                            model->mcp_4725_value = 4095;
                        } else if(
                            (model->mcp_4725_value < 20) || (model->mcp_4725_value > 4095 - 20) ||
                            (model->mcp_4725_value > 2048 - 20 &&
                             model->mcp_4725_value < 2048 + 20)) {
                            model->mcp_4725_value = model->mcp_4725_value + 1;
                        } else if(
                            (model->mcp_4725_value < 200) ||
                            (model->mcp_4725_value > 4095 - 200) ||
                            (model->mcp_4725_value > 2048 - 200 &&
                             model->mcp_4725_value < 2048 + 200)) {
                            model->mcp_4725_value = model->mcp_4725_value + 10;
                        } else {
                            model->mcp_4725_value += 100;
                        }
                        if(model->mcp_4725_value >= 4095) {
                            model->mcp_4725_value = 4095;
                        }
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, DacEventIdUpdateMcp4725);
                        break;
                    default:
                        break;
                    }
                },
                redraw);
        } else if(event->key == InputKeyUp) {
            // Choose previous item.
            with_view_model(
                app->view_voltage,
                VoltageModel * _model,
                {
                    if(_model->item > 0) {
                        _model->item--;
                    }
                },
                redraw);
        } else if(event->key == InputKeyDown) {
            // Choose next item.
            with_view_model(
                app->view_voltage,
                VoltageModel * _model,
                {
                    if(_model->item < DacVoltageItemCount - 1) {
                        _model->item++;
                    }
                },
                redraw);
        }
    }

    return false;
}

/**
 * @brief      Allocate the dac application.
 * @details    This function allocates the dac application resources.
 * @return     DacApp object.
*/
static DacApp* dac_app_alloc() {
    DacApp* app = (DacApp*)malloc(sizeof(DacApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Voltage", DacSubmenuIndexVoltage, submenu_selection_callback, app);
    submenu_add_item(app->submenu, "About", DacSubmenuIndexAbout, submenu_selection_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), navigation_exit_callback);

    view_dispatcher_add_view(app->view_dispatcher, DacViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, DacViewSubmenu);

    app->view_voltage = view_alloc();
    view_set_draw_callback(app->view_voltage, voltage_view_draw_callback);
    view_set_input_callback(app->view_voltage, voltage_view_input_callback);
    view_set_previous_callback(app->view_voltage, navigation_submenu_callback);
    view_set_enter_callback(app->view_voltage, voltage_view_enter_callback);
    view_set_exit_callback(app->view_voltage, voltage_view_exit_callback);
    view_set_context(app->view_voltage, app);
    view_set_custom_callback(app->view_voltage, voltage_view_custom_event_callback);
    view_allocate_model(app->view_voltage, ViewModelTypeLockFree, sizeof(VoltageModel));
    VoltageModel* model = view_get_model(app->view_voltage);

    model->item = DacVoltageItemFreq;
    model->i2c_address = 0;
    model->confirm_i2c_ready = false;
    model->write_eeprom = false;
    model->pwm_frequency = 1000;
    model->pwm_duty = 20;
    model->mcp_4725_value = 500;
    model->adc_value = 0;
    model->adc_handle = furi_hal_adc_acquire();
    furi_hal_adc_configure_ex(
        model->adc_handle,
        FuriHalAdcScale2048,
        FuriHalAdcClockSync64,
        FuriHalAdcOversample64,
        FuriHalAdcSamplingtime247_5);
    furi_hal_gpio_init(adc_input_gpio, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_pwm_start(channel_pwm, model->pwm_frequency, model->pwm_duty);

    view_dispatcher_add_view(app->view_dispatcher, DacViewVoltage, app->view_voltage);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Analog output and input!\n---\n"
        "MCP4725 on I2C:\n"
        "  MCP SDA=PC1\n"
        "  MCP SCL=PC0\n"
        "  MCP VCC=+5\n"
        "  MCP GND=GND\n"
        "PWM on A7:\n"
        "  A7 to 10K-Ohm Resistor\n"
        "  to 10uF Capacitor\n"
        "  to GND\n"
        "   output on cap/res\n"
        "OpAmp (LM324N) for load:\n"
        "  LM324 4 VCC=+5\n"
        "  LM324 11 GND=GND\n"
        "  LM324 1=LM324 2 (out1)\n"
        "  LM324 3=(Input 1)\n"
        "  WARN! Input 3.3v max!\n"
        "  LM324 6=LM324 7 (out2)\n"
        "  LM324 5=(Input 2)\n"
        "  WARN! Input 3.3v max!\n"
        "  LM324 9=LM324 8 (out3)\n"
        "  LM324 10=(Input 3)\n"
        "  WARN! Input 3.3v max!\n"
        "  LM324 14=LM324 13 (out4\n"
        "  LM324 12=(Input 4)\n"
        "ADC on C3:\n"
        "  WARN! 0 to 2048mV max!\n"
        "  Use a voltage divider!!!\n"
        "---\nauthor: @codeallnight\nhttps://discord.com/invite/NsjCvqwPAd\nhttps://youtube.com/@MrDerekJamison");
    view_set_previous_callback(widget_get_view(app->widget_about), navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, DacViewAbout, widget_get_view(app->widget_about));

    return app;
}

/**
 * @brief      Free the dac application.
 * @details    This function frees the dac application resources.
 * @param      app  The dac application object.
*/
static void dac_app_free(DacApp* app) {
    with_view_model(
        app->view_voltage,
        VoltageModel * model,
        { furi_hal_adc_release(model->adc_handle); },
        false);
    furi_hal_pwm_stop(channel_pwm);

    view_dispatcher_remove_view(app->view_dispatcher, DacViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, DacViewVoltage);
    view_free(app->view_voltage);
    view_dispatcher_remove_view(app->view_dispatcher, DacViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

/**
 * @brief      Attempt to enable +5 Volt GPIO pin #1.
 * @details    This function attempts to enable 5V power. It will return true if not already enabled and it was successful.
 * @return     true if 5V was enabled, false otherwise.
 */
static bool attempt_enable_5v() {
    bool enabled_5v = false;
    if(furi_hal_power_is_otg_enabled() || furi_hal_power_is_charging()) {
        enabled_5v = false;
    } else {
        // Some other apps make multiple attempts, so we retry too.
        uint8_t attempts = 5;
        while(--attempts > 0) {
            if(furi_hal_power_enable_otg()) {
                enabled_5v = true;
                break;
            }
        }
    }
    return enabled_5v;
}

/**
 * @brief      Main function for application.
 * @details    This function is the entry point for the application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t analog_output_app(void* _p) {
    UNUSED(_p);
    DacApp* app = dac_app_alloc();
    bool app_enabled_5v = attempt_enable_5v();
    view_dispatcher_run(app->view_dispatcher);
    dac_app_free(app);
    if(app_enabled_5v) {
        furi_hal_power_disable_otg();
    }
    return 0;
}

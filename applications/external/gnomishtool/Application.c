#include "F0App.h"
#include "gnomishtool_icons.h"
#include "F0BigData.h"
extern F0App* FApp;

char* str_mode[] = {"Counter", "Lighter", "Wire tester", "Voltmeter"};
char* str_lighter_mode[] =
    {"OFF", "EXTERNAL", "SCREEN", "LED: WHITE", "LED: RED", "LED: GREEN", "LED: BLUE"};
App_Global_Data AppGlobal = {
    .selectedTool = 0,
    .mode = 1,
    .lighter_mode = 0,
    .alarming = 0,
    .running = 1,
    .counter = 0,
    .ExtLedOn = 0,
    .pc3Voltage = 0};

int AppInit() {
    AppGlobal.adc_handle = furi_hal_adc_acquire();
    furi_hal_adc_configure(AppGlobal.adc_handle);
    notification_message(FApp->Notificator, &sequence_display_backlight_on);
    return 0;
}

int AppDeinit() {
    furi_hal_gpio_init(&gpio_ext_pc3, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    WiretesterSound(0);
    furi_hal_adc_release(AppGlobal.adc_handle);
    return 0;
}

int AppWork() {
    if(AppGlobal.mode == 1) return 0;
    if(AppGlobal.selectedTool == TOOL_WIRETESTER) {
        bool continuous = !PinRead(gpio_ext_pc3);
        if(continuous && !AppGlobal.alarming)
            WiretesterSound(1);
        else if(!continuous && AppGlobal.alarming)
            WiretesterSound(0);
        AppGlobal.alarming = continuous;
    }
    if(AppGlobal.selectedTool == TOOL_VOLTMETER) {
        AppGlobal.pc3Voltage = furi_hal_adc_convert_to_voltage(
            AppGlobal.adc_handle, furi_hal_adc_read(AppGlobal.adc_handle, gpio_pins[5].channel));
        UpdateView();
    }
    return 0;
}

void Draw(Canvas* c, void* ctx) {
    UNUSED(ctx);
    canvas_draw_icon(c, 0, 0, &I_Matik);
    elements_slightly_rounded_box(c, 11, 40 + (AppGlobal.selectedTool * 6), 9, 3);
    char strmode[40];
    if(AppGlobal.mode == 1) {
        canvas_set_font(c, FontSecondary);
        snprintf(strmode, sizeof(strmode), "%s", str_mode[AppGlobal.selectedTool]);
        canvas_draw_str_aligned(c, CAPTION_POSX, CAPTION_POSY + 3, AlignRight, AlignTop, strmode);
        return;
    }

    if(AppGlobal.selectedTool == TOOL_COUNTER) {
        char scount[8];
        snprintf(scount, sizeof(scount), "%d", AppGlobal.counter);
        canvas_set_custom_u8g2_font(c, TechnoDigits15);
        canvas_draw_str_aligned(c, CAPTION_POSX, CAPTION_POSY, AlignRight, AlignTop, scount);
    }

    if(AppGlobal.selectedTool == TOOL_LIGHTER) {
        if(AppGlobal.lighter_mode < 4)
            elements_slightly_rounded_box(c, 86 + (AppGlobal.lighter_mode * 8), 27, 5, 2);
        else
            elements_slightly_rounded_box(c, 124, 21 + ((AppGlobal.lighter_mode - 4) * 8), 2, 5);

        if(AppGlobal.mode == 0) {
            canvas_set_font(c, FontSecondary);
            canvas_draw_str_aligned(
                c,
                CAPTION_POSX,
                CAPTION_POSY + 3,
                AlignRight,
                AlignTop,
                str_lighter_mode[AppGlobal.lighter_mode]);
        }
    }

    if(AppGlobal.selectedTool == TOOL_WIRETESTER) {
        if(AppGlobal.mode == 0) {
            canvas_set_font(c, FontSecondary);
            canvas_draw_str_aligned(
                c, CAPTION_POSX, CAPTION_POSY + 3, AlignRight, AlignTop, "Testing...");
        }
    }

    if(AppGlobal.selectedTool == TOOL_VOLTMETER) {
        if(AppGlobal.mode == 0) {
            char pc3voltstr[64];
            snprintf(pc3voltstr, sizeof(pc3voltstr), "%4.0f", (double)AppGlobal.pc3Voltage);
            canvas_set_custom_u8g2_font(c, TechnoDigits15);
            canvas_draw_str_aligned(
                c, CAPTION_POSX, CAPTION_POSY, AlignRight, AlignTop, pc3voltstr);
        }
    }
}
int KeyProc(int type, int key) {
    if(type == InputTypeMAX) {
        if(key == 255) {
            if(AppGlobal.selectedTool == TOOL_LIGHTER) {
                if(AppGlobal.lighter_mode == 2) SetScreenBacklightBrightness(100);
            } else
                SetScreenBacklightBrightness(FApp->SystemScreenBrightness * 100);
        }
        return 0;
    }

    if(AppGlobal.mode) {
        if(type == InputTypeLong) {
            if(key == InputKeyBack && AppGlobal.mode) return 255;
            return 0;
        }

        if(type == InputTypePress) {
            if(key == InputKeyOk) {
                if(AppGlobal.mode) {
                    AppGlobal.mode = 0;
                    if(AppGlobal.selectedTool == TOOL_LIGHTER)
                        furi_hal_gpio_init(
                            &gpio_ext_pc3, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
                    if(AppGlobal.selectedTool == TOOL_WIRETESTER) {
                        furi_hal_gpio_init(&gpio_ext_pc3, GpioModeInput, GpioPullUp, GpioSpeedLow);
                        AppGlobal.alarming = 0;
                    }
                    if(AppGlobal.selectedTool == TOOL_VOLTMETER)
                        furi_hal_gpio_init(
                            &gpio_ext_pc3, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
                    return 0;
                }
                return 0;
            }
            if(key == InputKeyUp) {
                if(AppGlobal.mode) {
                    if(AppGlobal.selectedTool)
                        AppGlobal.selectedTool--;
                    else
                        AppGlobal.selectedTool = TOOL_VOLTMETER;
                }
                return 0;
            }
            if(key == InputKeyDown) {
                if(AppGlobal.mode) {
                    if(AppGlobal.selectedTool == TOOL_VOLTMETER)
                        AppGlobal.selectedTool = TOOL_COUNTER;
                    else
                        AppGlobal.selectedTool++;
                }
                return 0;
            }
        }

    } else {
        if(type == InputTypePress || type == InputTypeLong) {
            if(AppGlobal.selectedTool == TOOL_COUNTER) {
                return ToolDefaultKey(type, key);
            }
            if(AppGlobal.selectedTool == TOOL_LIGHTER) {
                return ToolLighterKey(type, key);
            }
            if(AppGlobal.selectedTool == TOOL_WIRETESTER) {
                if(key == InputKeyBack) WiretesterSound(0);
                return 0;
            }
        }
        if(key == InputKeyBack) {
            AppGlobal.mode = 1;
            return 0;
        }
    }
    return 0;
}

void WiretesterSound(bool state) {
    if(state) {
        SetLED(0, 0, 255, 1.0);
        if(furi_hal_speaker_acquire(1000)) furi_hal_speaker_start(2400.0f, 0.8f);
        return;
    }
    ResetLED();
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

int ToolDefaultKey(int type, int key) {
    if(key == InputKeyOk) {
        if(type == InputTypeLong) AppGlobal.counter = 0;
        return 0;
    }

    if(key == InputKeyUp) {
        if(type != InputTypePress) return 0;
        AppGlobal.counter++;
        if(AppGlobal.counter > 9999) AppGlobal.counter = 9999;
        return 0;
    }

    if(key == InputKeyDown) {
        if(type != InputTypePress) return 0;
        if(AppGlobal.counter) AppGlobal.counter--;
        return 0;
    }
    if(key == InputKeyLeft) {
        if(type != InputTypePress) return 0;
        if(AppGlobal.counter > 5)
            AppGlobal.counter -= 5;
        else
            AppGlobal.counter = 0;
        return 0;
    }
    if(key == InputKeyRight) {
        if(type != InputTypePress) return 0;
        AppGlobal.counter += 5;
        if(AppGlobal.counter > 9999) AppGlobal.counter = 9999;
        return 0;
    }
    return 0;
}

int ToolLighterKey(int type, int key) {
    if(type != InputTypePress) return 0;
    if(key == InputKeyBack) {
        ResetLED();
        SetScreenBacklightBrightness(FApp->SystemScreenBrightness * 100);
        ExtLedToggle(0);
        AppGlobal.lighter_mode = 0;
        return 0;
    }
    if(key == InputKeyLeft) {
        if(AppGlobal.lighter_mode)
            AppGlobal.lighter_mode--;
        else
            AppGlobal.lighter_mode = 6;
    }
    if(key == InputKeyRight) {
        if(AppGlobal.lighter_mode > 5)
            AppGlobal.lighter_mode = 0;
        else
            AppGlobal.lighter_mode++;
    }
    if(key == InputKeyLeft || key == InputKeyRight) {
        if(AppGlobal.lighter_mode == 0) { //OFF
            ResetLED();
            SetScreenBacklightBrightness(FApp->SystemScreenBrightness * 100);
            ExtLedToggle(0);
        }
        if(AppGlobal.lighter_mode == 1) { //EXT LED
            ResetLED();
            SetScreenBacklightBrightness(FApp->SystemScreenBrightness * 100);
            ExtLedToggle(2);
        }
        if(AppGlobal.lighter_mode == 2) { //SCREEN
            SetScreenBacklightBrightness(100);
            ExtLedToggle(0);
        }
        if(AppGlobal.lighter_mode == 3) { //W
            SetLED(255, 255, 255, 1.0);
            SetScreenBacklightBrightness(0);
        }
        if(AppGlobal.lighter_mode == 4) { //R
            SetLED(255, 0, 0, 1.0);
        }
        if(AppGlobal.lighter_mode == 5) { //G
            SetLED(0, 255, 0, 1.0);
        }
        if(AppGlobal.lighter_mode == 6) { //B
            SetLED(0, 0, 255, 1.0);
            SetScreenBacklightBrightness(0);
        }
    }
    return 0;
}

void ExtLedToggle(int action) {
    if(action == 2) //toggle
        AppGlobal.ExtLedOn = !AppGlobal.ExtLedOn;
    else
        AppGlobal.ExtLedOn = action;
    furi_hal_gpio_write(&gpio_ext_pc3, AppGlobal.ExtLedOn);
}

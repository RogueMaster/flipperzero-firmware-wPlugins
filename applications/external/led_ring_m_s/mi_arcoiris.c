#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#define LED_MIN_COUNT 1
#define LED_MAX_COUNT 64

typedef enum {
    StateWelcome,    // Screen 1: GPIO Wiring Notice
    StateConfig,     // Screen 2: Geometry Selection Menu
    StateAnimation   // Screen 3: Live Light Effects
} AppState;

typedef enum {
    TypeRing,
    TypeMatrix
} LedType;

typedef enum {
    ModoArcoiris,
    ModoPulso,
    ModoKitt,
    ModoFuego,
    ModoMax
} ModoAnimacion;

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    AppState estado_actual;
    LedType tipo_led;
    uint8_t total_leds;
    uint8_t config_index; 
    ModoAnimacion modo_actual;
    uint8_t brillo_porcentaje;
    uint8_t velocidad_factor;
    uint8_t color_rueda_base;
    bool ejecutando;
} AppContext;

// Render system for the user interface
static void render_callback(Canvas* canvas, void* ctx) {
    AppContext* app = ctx;
    canvas_clear(canvas);
    
    // --- STATE 1: WELCOME SCREEN (WIRING NOTICE) ---
    if(app->estado_actual == StateWelcome) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 11, "LED Studio: GPIO Setup");
        
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 24, "Connect your WS2812B / SK6812:");
        canvas_draw_str(canvas, 10, 35, "-> DI (Data)  to  Pin 14 (A7)");
        canvas_draw_str(canvas, 10, 44, "-> VCC (+5V)  to  Pin 5  (5V)");
        canvas_draw_str(canvas, 10, 53, "-> GND       to  Pin 8  (GND)");
        
        canvas_draw_str(canvas, 2, 63, "[OK] Connected -> Setup Layout");
    }
    // --- STATE 2: CONFIGURATION MENU ---
    else if(app->estado_actual == StateConfig) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 11, "Hardware Configuration");
        
        canvas_set_font(canvas, FontSecondary);
        
        if(app->config_index == 0) canvas_draw_str(canvas, 2, 26, "> Layout: ");
        else canvas_draw_str(canvas, 10, 26, "Layout: ");
        canvas_draw_str(canvas, 60, 26, (app->tipo_led == TypeRing) ? "< Circular Ring >" : "< Square Matrix >");
        
        if(app->config_index == 1) canvas_draw_str(canvas, 2, 39, "> LED Count: ");
        else canvas_draw_str(canvas, 10, 39, "LED Count: ");
        char buf_count[16];
        snprintf(buf_count, sizeof(buf_count), "< %d LEDs >", app->total_leds);
        canvas_draw_str(canvas, 75, 39, buf_count);
        
        canvas_draw_str(canvas, 2, 53, "[^/v] Navigate  |  [<-/->] Change");
        canvas_draw_str(canvas, 2, 63, "[OK] Save & Start Animation Engine");
    }
    // --- STATE 3: LIVE EFFECTS GENERATOR ---
    else if(app->estado_actual == StateAnimation) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 11, "LED Studio: Active");
        
        canvas_set_font(canvas, FontSecondary);
        switch(app->modo_actual) {
            case ModoArcoiris: canvas_draw_str(canvas, 2, 23, "Mode: Rainbow Spectrum"); break;
            case ModoPulso:    canvas_draw_str(canvas, 2, 23, "Mode: Color Breath"); break;
            case ModoKitt:     canvas_draw_str(canvas, 2, 23, "Mode: Knight Rider KITT"); break;
            case ModoFuego:    canvas_draw_str(canvas, 2, 23, "Mode: Organic Fire"); break;
            default: break;
        }
        
            // Line 2: Parameters Status (Size expanded to 64 to avoid truncation errors)
    char buffer_status[64];
    snprintf(buffer_status, sizeof(buffer_status), "Pwr: %d%% | Spd: %dx | LEDs: %d", 
             app->brillo_porcentaje, app->velocidad_factor, app->total_leds);
    canvas_draw_str(canvas, 2, 34, buffer_status);
        
        canvas_draw_str(canvas, 2, 45, "[OK] Next Mode | [^/v] Brightness");
        canvas_draw_str(canvas, 2, 54, "[<-/->] Change Animation Speed");
        canvas_draw_str(canvas, 2, 63, "[BACK] Return to Setup Menu");
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* input_queue = ctx;
    furi_message_queue_put(input_queue, input_event, FuriWaitForever);
}

static void conseguir_color_rueda(uint8_t pos, uint8_t* r, uint8_t* g, uint8_t* b) {
    if(pos < 85) {
        *r = pos * 3; *g = 255 - pos * 3; *b = 0;
    } else if(pos < 170) {
        pos -= 85;
        *r = 255 - pos * 3; *g = 0; *b = pos * 3;
    } else {
        pos -= 170;
        *r = 0; *g = pos * 3; *b = 255 - pos * 3;
    }
}

/**
 * LOW-LEVEL HARDWARE DATA TRANSMISSION (PA7 Registers)
 */
static void enviar_buffer_neopixel(uint8_t* buffer, uint16_t num_bytes) {
    volatile uint32_t* bsrr = &GPIOA->BSRR;
    uint32_t pin_set = (1UL << 7);
    uint32_t pin_reset = (1UL << 7) << 16;

    FURI_CRITICAL_ENTER();
    for(uint16_t i = 0; i < num_bytes; i++) {
        uint8_t byte = buffer[i];
        for(int8_t bit = 7; bit >= 0; bit--) {
            if((byte >> bit) & 0x01) {
                *bsrr = pin_set;
                __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
                *bsrr = pin_reset;
                __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
            } else {
                *bsrr = pin_set;
                __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
                *bsrr = pin_reset;
                __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
            }
        }
    }
    FURI_CRITICAL_EXIT();
}
int32_t arcoiris_main(void* p) {
    UNUSED(p);
    
    AppContext* app = malloc(sizeof(AppContext));
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->estado_actual = StateWelcome;
    app->tipo_led = TypeRing;
    app->total_leds = 16;
    app->config_index = 0;
    
    app->modo_actual = ModoArcoiris;
    app->brillo_porcentaje = 15;
    app->velocidad_factor = 2;
    app->color_rueda_base = 0;
    app->ejecutando = true;

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, render_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app->input_queue);
    
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_power_enable_otg();

    uint8_t buffer_leds[64 * 3];
    uint32_t tick_animacion = 0;

    InputEvent evento;
    while(app->ejecutando) {
        uint32_t timeout = (app->estado_actual == StateAnimation) ? 15 : 100;
        
        if(furi_message_queue_get(app->input_queue, &evento, timeout) == FuriStatusOk) {
            if(evento.type == InputTypeShort || evento.type == InputTypeRepeat) {
                
                // BUTTONS LOGIC: WELCOME SCREEN
                if(app->estado_actual == StateWelcome) {
                    if(evento.key == InputKeyBack) {
                        app->ejecutando = false;
                    } else if(evento.key == InputKeyOk) {
                        app->estado_actual = StateConfig;
                    }
                }
                // BUTTONS LOGIC: CONFIGURATION MENU
                else if(app->estado_actual == StateConfig) {
                    if(evento.key == InputKeyBack) {
                        app->estado_actual = StateWelcome;
                    } else if(evento.key == InputKeyUp || evento.key == InputKeyDown) {
                        app->config_index = (app->config_index == 0) ? 1 : 0;
                    } else if(evento.key == InputKeyOk) {
                        app->estado_actual = StateAnimation;
                    } else if(app->config_index == 0) {
                        if(evento.key == InputKeyLeft || evento.key == InputKeyRight) {
                            app->tipo_led = (app->tipo_led == TypeRing) ? TypeMatrix : TypeRing;
                        }
                    } else if(app->config_index == 1) {
                        if(evento.key == InputKeyRight && app->total_leds < LED_MAX_COUNT) {
                            app->total_leds = (app->total_leds == 16) ? 24 : (app->total_leds == 24) ? 32 : (app->total_leds == 32) ? 64 : app->total_leds + 1;
                        } else if(evento.key == InputKeyLeft && app->total_leds > LED_MIN_COUNT) {
                            app->total_leds = (app->total_leds == 64) ? 32 : (app->total_leds == 32) ? 24 : (app->total_leds == 24) ? 16 : app->total_leds - 1;
                        }
                    }
                }
                // BUTTONS LOGIC: LIVE ANIMATIONS
                else if(app->estado_actual == StateAnimation) {
                    if(evento.key == InputKeyBack) {
                        app->estado_actual = StateConfig;
                        memset(buffer_leds, 0, sizeof(buffer_leds));
                        furi_delay_us(60);
                        enviar_buffer_neopixel(buffer_leds, app->total_leds * 3);
                    } else if(evento.key == InputKeyOk && evento.type == InputTypeShort) {
                        app->modo_actual = (app->modo_actual + 1) % ModoMax;
                        app->color_rueda_base += 40;
                    } else if(evento.key == InputKeyUp) {
                        if(app->brillo_porcentaje < 50) app->brillo_porcentaje += 5;
                    } else if(evento.key == InputKeyDown) {
                        if(app->brillo_porcentaje > 5) app->brillo_porcentaje -= 5;
                    } else if(evento.key == InputKeyRight) {
                        if(app->velocidad_factor < 5) app->velocidad_factor++;
                    } else if(evento.key == InputKeyLeft) {
                        if(app->velocidad_factor > 1) app->velocidad_factor--;
                    }
                }
            }
        }

        if(app->estado_actual != StateAnimation) {
            view_port_update(app->view_port);
            continue;
        }

        tick_animacion += app->velocidad_factor;
        memset(buffer_leds, 0, sizeof(buffer_leds));

        // ---------------- GEOMETRIC GRAPHICS ENGINE ----------------
        if(app->modo_actual == ModoArcoiris) {
            for(int i = 0; i < app->total_leds; i++) {
                uint8_t pos_espectro = (i * 256 / app->total_leds) + (tick_animacion * 2);
                conseguir_color_rueda(pos_espectro, &buffer_leds[i*3+1], &buffer_leds[i*3], &buffer_leds[i*3+2]);
            }
        } 
        else if(app->modo_actual == ModoPulso) {
            uint8_t factor_breathe = (tick_animacion % 64);
            if(factor_breathe > 32) factor_breathe = 64 - factor_breathe;
            
            uint8_t r_b, g_b, b_b;
            conseguir_color_rueda(app->color_rueda_base + (tick_animacion / 2), &r_b, &g_b, &b_b);

            for(int i = 0; i < app->total_leds; i++) {
                buffer_leds[i*3+1] = (r_b * factor_breathe) / 32;
                buffer_leds[i*3]   = (g_b * factor_breathe) / 32;
                buffer_leds[i*3+2] = (b_b * factor_breathe) / 32;
            }
        }
        else if(app->modo_actual == ModoKitt) {
            int posicion_activa = (tick_animacion / 4) % (app->total_leds * 2);
            if(posicion_activa >= app->total_leds) posicion_activa = (app->total_leds * 2 - 1) - posicion_activa;

            uint8_t r_k, g_k, b_k;
            conseguir_color_rueda(app->color_rueda_base, &r_k, &g_k, &b_k);

            for(int i = 0; i < app->total_leds; i++) {
                if(i == posicion_activa) {
                    buffer_leds[i*3+1] = r_k; buffer_leds[i*3] = g_k; buffer_leds[i*3+2] = b_k;
                } else if(i == posicion_activa - 1 || i == posicion_activa + 1) {
                    buffer_leds[i*3+1] = r_k / 4; buffer_leds[i*3] = g_k / 4; buffer_leds[i*3+2] = b_k / 4;
                }
            }
        }
                else if(app->modo_actual == ModoFuego) {
            for(int i = 0; i < app->total_leds; i++) {
                uint32_t rnd = furi_hal_random_get();
                uint8_t compensacion_matriz = (app->tipo_led == TypeMatrix && i < app->total_leds / 4) ? 40 : 0;
                uint8_t base_fuego = 50 + (rnd % 130) + compensacion_matriz;
                
                buffer_leds[i*3]   = base_fuego;
                buffer_leds[i*3+1] = base_fuego / 3;
                buffer_leds[i*3+2] = 0;
            }
        }


        // ---------------- INTENSITY COMPENSATOR ----------------
        for(int i = 0; i < app->total_leds * 3; i++) {
            uint32_t color_escalado = ((uint32_t)buffer_leds[i] * app->brillo_porcentaje) / 100;
            buffer_leds[i] = (uint8_t)color_escalado;
        }

        furi_delay_us(60); 
        enviar_buffer_neopixel(buffer_leds, app->total_leds * 3);

        view_port_update(app->view_port);
    }

    // SYSTEM RESET AND SAFE SHUTDOWN
    memset(buffer_leds, 0, sizeof(buffer_leds));
    furi_delay_us(60);
    enviar_buffer_neopixel(buffer_leds, app->total_leds * 3);

    furi_hal_power_disable_otg();
    furi_hal_gpio_init(&gpio_ext_pa7, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->input_queue);
    free(app);

    return 0;
}

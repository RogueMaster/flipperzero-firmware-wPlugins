#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>

// Hardware Definitions
#define NRF24_CE_PIN &gpio_ext_pa6
#define NRF24_CS_PIN &gpio_ext_pc3
#define NRF24_HANDLE &furi_hal_spi_bus_handle_external

// Register Definitions
#define NRF24_REG_SETUP_AW 0x03
#define NRF24_CMD_READ_REG 0x00

typedef struct {
    bool initialized;
    uint8_t reg_value;
    FuriMutex* mutex;
    bool running;
} Nrf24TestApp;

uint8_t nrf24_read_register(uint8_t reg) {
    uint8_t tx_data[2] = {NRF24_CMD_READ_REG | reg, 0};
    uint8_t rx_data[2] = {0};

    furi_hal_gpio_write(NRF24_CS_PIN, false);
    furi_hal_spi_bus_trx(NRF24_HANDLE, tx_data, rx_data, 2, 100);
    furi_hal_gpio_write(NRF24_CS_PIN, true);

    return rx_data[1];
}

bool nrf24_check_connection() {
    return (nrf24_read_register(NRF24_REG_SETUP_AW) == 0x03);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    if(input_event->type == InputTypeShort && input_event->key == InputKeyBack) {
        furi_message_queue_put(event_queue, input_event, FuriWaitForever);
    }
}

static void draw_callback(Canvas* canvas, void* ctx) {
    Nrf24TestApp* app = ctx;

    canvas_clear(canvas);

    // Draw header
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, "NRF24L01+ Monitor");
    canvas_draw_line(canvas, 0, 16, 128, 16);

    // Get thread-safe data
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool initialized = app->initialized;
    uint8_t reg_value = app->reg_value;
    furi_mutex_release(app->mutex);

    // Draw status indicator
    canvas_set_font(canvas, FontSecondary);
    if(initialized) {
        canvas_draw_circle(canvas, 15, 30, 5);
        canvas_draw_line(canvas, 13, 28, 17, 32);
        canvas_draw_line(canvas, 17, 28, 13, 32);
        canvas_draw_str(canvas, 25, 35, "CONNECTED");
    } else {
        canvas_draw_circle(canvas, 15, 30, 5);
        canvas_draw_line(canvas, 10, 30, 20, 30);
        canvas_draw_str(canvas, 25, 35, "NO SIGNAL");
    }

    // Register info box
    canvas_draw_rframe(canvas, 10, 45, 108, 20, 3); // Rounded frame
    canvas_draw_str(canvas, 15, 60, "SETUP_AW:");

    char reg_text[8];
    snprintf(reg_text, sizeof(reg_text), "0x%02X", reg_value);
    canvas_draw_str_aligned(canvas, 100, 60, AlignRight, AlignBottom, reg_text);

    // Footer
    canvas_draw_line(canvas, 0, 70, 128, 70);
    canvas_draw_str_aligned(canvas, 64, 80, AlignCenter, AlignBottom, "[ OK to Exit ]");
}

static int32_t nrf24_status_check_thread(void* ctx) {
    Nrf24TestApp* app = ctx;

    // Initialize hardware
    furi_hal_gpio_init_simple(NRF24_CS_PIN, GpioModeOutputPushPull);
    furi_hal_gpio_write(NRF24_CS_PIN, true);
    furi_hal_gpio_init(NRF24_CE_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_write(NRF24_CE_PIN, false);
    furi_hal_spi_bus_handle_init(NRF24_HANDLE);

    while(1) {
        // Check if we should stop
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool should_run = app->running;
        furi_mutex_release(app->mutex);

        if(!should_run) break;

        // Check status
        furi_hal_spi_acquire(NRF24_HANDLE);
        bool connected = nrf24_check_connection();
        uint8_t reg_value = nrf24_read_register(NRF24_REG_SETUP_AW);
        furi_hal_spi_release(NRF24_HANDLE);

        // Update status
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        app->initialized = connected;
        app->reg_value = reg_value;
        furi_mutex_release(app->mutex);

        furi_delay_ms(100);
    }

    // Cleanup
    furi_hal_spi_bus_handle_deinit(NRF24_HANDLE);
    furi_hal_gpio_write(NRF24_CE_PIN, false);

    return 0;
}

int32_t nrf24_test_app(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();

    Nrf24TestApp* app = malloc(sizeof(Nrf24TestApp));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->initialized = false;
    app->reg_value = 0;
    app->running = true;

    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FuriThread* thread = furi_thread_alloc();
    furi_thread_set_name(thread, "NRF24StatusThread");
    furi_thread_set_stack_size(thread, 1024);
    furi_thread_set_context(thread, app);
    furi_thread_set_callback(thread, nrf24_status_check_thread);
    furi_thread_start(thread);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.key == InputKeyBack) {
                running = false;
            }
        }
        view_port_update(view_port);
    }

    // Signal thread to stop
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->running = false;
    furi_mutex_release(app->mutex);

    furi_thread_join(thread);
    furi_thread_free(thread);

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(app->mutex);
    free(app);
    furi_record_close(RECORD_GUI);

    return 0;
}

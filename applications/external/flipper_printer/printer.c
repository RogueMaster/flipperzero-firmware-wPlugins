#include "flipper_printer.h"
#include <gui/elements.h>

// Global UART handle
static FuriHalSerialHandle* serial_handle = NULL;

// Initialize UART for thermal printer communication
// TWO METHODS AVAILABLE - Choose one by commenting/uncommenting:
// METHOD 1: USART with C0/C1 pins (currently active) - Physical connection: RX/TX pins
// METHOD 2: LPUART with direct RX/TX pins - Physical connection: C0/C1 pins
void printer_init(void) {
    // ===== METHOD 1: USART C0/C1 (CURRENT ACTIVE METHOD) =====
    // Physical connection: Connect wires to RX/TX pins (13/14) on Flipper Zero
    serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!serial_handle) {
        FURI_LOG_E(TAG, "Failed to acquire serial handle");
        return;
    }

    // Configure C0/C1 for USART (maps to physical RX/TX pins)
    furi_hal_gpio_init_ex(
        &gpio_ext_pc0,
        GpioModeAltFunctionPushPull,
        GpioPullNo,
        GpioSpeedVeryHigh,
        GpioAltFn7USART1);

    furi_hal_gpio_init_ex(
        &gpio_ext_pc1,
        GpioModeAltFunctionPushPull,
        GpioPullUp,
        GpioSpeedVeryHigh,
        GpioAltFn7USART1);

    furi_hal_serial_init(serial_handle, 9600);

    /* ===== METHOD 2: LPUART DIRECT (ALTERNATIVE METHOD) =====
     * To use this method: Comment out METHOD 1 above and uncomment below
     * Physical connection: Connect wires to C0/C1 pins (15/16) on Flipper Zero
     * 
     * serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdLpuart);
     * if(!serial_handle) {
     *     FURI_LOG_E(TAG, "Failed to acquire serial handle");
     *     return;
     * }
     * 
     * // Configure PA7 for LPUART (maps to physical C0/C1 pins)
     * furi_hal_gpio_init_ex(&gpio_ext_pa7, GpioModeAltFunctionPushPull, 
     *                       GpioPullNo, GpioSpeedVeryHigh, GpioAltFn8LPUART1);
     * 
     * furi_hal_serial_init(serial_handle, 9600);
     */
}

// Cleanup UART resources
void printer_deinit(void) {
    if(serial_handle) {
        furi_hal_serial_deinit(serial_handle);
        furi_hal_serial_control_release(serial_handle);
        serial_handle = NULL;
    }
}

// Send data to thermal printer with proper timing
static void printer_send_data(const uint8_t* data, size_t length) {
    if(!serial_handle || !data) return;

    furi_hal_serial_tx(serial_handle, data, length);
    furi_delay_ms(50); // Give printer time to process
}

// Reset thermal printer to default state
static void printer_reset(void) {
    const uint8_t cmd_reset[] = {0x1b, 0x40}; // ESC @
    printer_send_data(cmd_reset, sizeof(cmd_reset));
    furi_delay_ms(100);
}

// Feed paper and cut
static void printer_feed_and_cut(void) {
    const uint8_t cmd_feed[] = {0x1b, 0x64, 0x03}; // ESC d 3 (feed 3 lines)
    const uint8_t cmd_cut[] = {0x1d, 0x56, 0x01}; // GS V 1 (partial cut)

    printer_send_data(cmd_feed, sizeof(cmd_feed));
    printer_send_data(cmd_cut, sizeof(cmd_cut));
    furi_delay_ms(100);
}

// Thermal printer formatting commands
static void printer_set_font_size(uint8_t size) {
    // ESC ! n - Select character font and styles
    // Bit 0-1: Font (0=A, 1=B)
    // Bit 4: Double height
    // Bit 5: Double width
    uint8_t cmd[] = {0x1b, 0x21, size};
    printer_send_data(cmd, sizeof(cmd));
}

static void printer_set_alignment(uint8_t align) {
    // ESC a n - Select justification
    // 0: Left, 1: Center, 2: Right
    uint8_t cmd[] = {0x1b, 0x61, align};
    printer_send_data(cmd, sizeof(cmd));
}

static void printer_set_bold(bool enabled) {
    // ESC E n - Turn emphasized mode on/off
    uint8_t cmd[] = {0x1b, 0x45, enabled ? 1 : 0};
    printer_send_data(cmd, sizeof(cmd));
}

static void printer_print_separator(char symbol) {
    printer_set_alignment(1); // Center
    char separator[33]; // 32 chars + null terminator
    for(int i = 0; i < 32; i++) {
        separator[i] = symbol;
    }
    separator[32] = '\0';
    printer_send_data((const uint8_t*)separator, strlen(separator));
    printer_send_data((const uint8_t*)"\n", 1);
    printer_set_alignment(0); // Reset to left
}

static void printer_print_header(const char* title) {
    printer_print_separator('=');

    printer_set_alignment(1); // Center
    printer_set_bold(true);
    printer_set_font_size(0x30); // Double height and width

    printer_send_data((const uint8_t*)title, strlen(title));
    printer_send_data((const uint8_t*)"\n", 1);

    // Reset formatting
    printer_set_font_size(0x00);
    printer_set_bold(false);
    printer_set_alignment(0);

    printer_print_separator('=');
}

// Print text with custom formatting
void printer_print_text(const char* text) {
    if(!text || strlen(text) == 0) return;

    printer_reset();

    // Print header
    printer_print_header("FLIPPER PRINTER");

    // Add spacing
    printer_send_data((const uint8_t*)"\n", 1);

    // Print main content
    printer_set_alignment(0); // Left align
    printer_set_font_size(0x00); // Normal size
    printer_send_data((const uint8_t*)text, strlen(text));
    printer_send_data((const uint8_t*)"\n\n", 2);

    // Print footer separator
    printer_print_separator('-');

    // Print timestamp placeholder
    printer_set_alignment(1); // Center
    printer_send_data(
        (const uint8_t*)"Printed by Flipper Zero", strlen("Printed by Flipper Zero"));
    printer_send_data((const uint8_t*)"\n", 1);
    printer_set_alignment(0); // Reset

    printer_feed_and_cut();
}

// Print coin flip result with custom formatting
void printer_print_coin_flip(int32_t flip_number, bool is_heads) {
    printer_reset();

    // Print header
    char header[32];
    snprintf(header, sizeof(header), "COIN FLIP #%ld", flip_number);
    printer_print_header(header);

    // Add spacing
    printer_send_data((const uint8_t*)"\n", 1);

    // Print result with large font
    printer_set_alignment(1); // Center
    printer_set_bold(true);
    printer_set_font_size(0x30); // Double height and width

    const char* result_text = is_heads ? "HEADS" : "TAILS";
    printer_send_data((const uint8_t*)result_text, strlen(result_text));
    printer_send_data((const uint8_t*)"\n\n", 2);

    // Reset formatting and print decorative element
    printer_set_font_size(0x00);
    printer_set_bold(false);

    // Print coin symbol
    const char* coin_symbol = is_heads ? " (H) " : " (T) ";
    printer_send_data((const uint8_t*)coin_symbol, strlen(coin_symbol));
    printer_send_data((const uint8_t*)"\n", 1);

    printer_set_alignment(0); // Reset to left

    // Print footer separator
    printer_print_separator('~');

    // Print footer
    printer_set_alignment(1); // Center
    printer_send_data(
        (const uint8_t*)"Flipped by Flipper Zero", strlen("Flipped by Flipper Zero"));
    printer_send_data((const uint8_t*)"\n", 1);
    printer_set_alignment(0); // Reset

    printer_feed_and_cut();
}

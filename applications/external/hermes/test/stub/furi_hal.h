#pragma once
#include "furi.h"

typedef struct {
    void* port;
    uint16_t pin;
} GpioPin;

typedef enum {
    GpioModeInput,
    GpioModeOutputPushPull,
    GpioModeAnalog,
    GpioModeInterruptRiseFall,
} GpioMode;

typedef enum { GpioPullNo, GpioPullUp, GpioPullDown } GpioPull;
typedef enum { GpioSpeedLow, GpioSpeedMedium, GpioSpeedHigh, GpioSpeedVeryHigh } GpioSpeed;

typedef void (*GpioExtiCallback)(void* ctx);

extern const GpioPin gpio_usart_rx;
extern const GpioPin gpio_usart_tx;
extern const GpioPin gpio_ext_pc0;
extern const GpioPin gpio_ext_pc1;

/* The harness drives these: the fake pin level, and the interrupt handler that
 * autobaud.c registered, so the test exercises the real ISR. */
extern bool hermes_test_level;
extern GpioExtiCallback hermes_test_isr;
extern void* hermes_test_isr_ctx;

static inline bool furi_hal_gpio_read(const GpioPin* p) {
    (void)p;
    return hermes_test_level;
}

void furi_hal_gpio_init(const GpioPin* p, GpioMode m, GpioPull pu, GpioSpeed s);
void furi_hal_gpio_add_int_callback(const GpioPin* p, GpioExtiCallback cb, void* ctx);
void furi_hal_gpio_remove_int_callback(const GpioPin* p);

uint32_t furi_hal_cortex_instructions_per_microsecond(void);

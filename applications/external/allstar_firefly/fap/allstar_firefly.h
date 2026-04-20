#pragma once

/**
 * allstar_firefly.h
 * Allstar Firefly 318ALD31K Gate Remote — SubGHz FAP
 *
 * Protocol summary:
 *   Frequency  : 318 MHz
 *   Modulation : OOK, FuriHalSubGhzPresetOok650Async
 *   Bits       : 9 trinary (DIP switches), encoded as 18 symbols per frame
 *   Repeats    : ~20 frames per keypress, ~30 440 µs inter-frame gap
 *
 *   Symbol encoding (each bit = 2 consecutive pulse+gap pairs):
 *     '+'  ON    : H H  = [4045µs HIGH, 607µs LOW] x2
 *     '-'  OFF   : L L  = [530µs HIGH, 4139µs LOW] x2
 *     '0'  FLOAT : H L  = [4045µs HIGH, 607µs LOW, 530µs HIGH, 4139µs LOW]
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/canvas.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <lib/subghz/devices/devices.h>
#include <lib/subghz/devices/cc1101_int/cc1101_int_interconnect.h>

/* ─── Protocol constants ──────────────────────────────────────────────────── */

/** Carrier frequency in Hz */
#define AF_FREQ             318000000UL

/** Number of trinary DIP bits per frame */
#define AF_BIT_COUNT        9

/** Number of OOK symbols per frame (2 per bit) */
#define AF_SYM_COUNT        (AF_BIT_COUNT * 2)

/** Long (HIGH) symbol pulse width in µs — measured average across 37 frames */
#define AF_LONG_PULSE_US    4045u

/** Short (LOW) symbol pulse width in µs */
#define AF_SHORT_PULSE_US   530u

/** Gap following a long (H) symbol in µs */
#define AF_SHORT_GAP_US     607u

/** Gap following a short (L) symbol in µs */
#define AF_LONG_GAP_US      4139u

/** Inter-frame silence in µs (separates repeated frames) */
#define AF_INTERFRAME_US    30440u

/**
 * Threshold in µs to classify a received pulse as long (H) vs short (L).
 * pulse >= AF_SYM_THRESH_US  =>  H (1)
 * pulse <  AF_SYM_THRESH_US  =>  L (0)
 */
#define AF_SYM_THRESH_US    2000u

/**
 * Valid duration range for a SHORT (L) pulse.
 * Measured legitimate range: 465–607 µs.
 * Any pulse outside [AF_SHORT_PULSE_MIN, AF_SHORT_PULSE_MAX] is noise
 * and causes the current frame to be discarded immediately.
 */
#define AF_SHORT_PULSE_MIN  300u
#define AF_SHORT_PULSE_MAX  1200u

/**
 * Valid duration range for a LONG (H) pulse.
 * Measured legitimate range: 3935–4101 µs.
 * Any pulse outside [AF_LONG_PULSE_MIN, AF_LONG_PULSE_MAX] is noise
 * and causes the current frame to be discarded immediately.
 */
#define AF_LONG_PULSE_MIN   2500u
#define AF_LONG_PULSE_MAX   5500u

/**
 * A received gap this long or longer signals the end of a frame.
 * Actual inter-frame gaps measured at 26 500–30 500 µs.
 * The largest noise gap in observed preambles is ~16 900 µs, so 20 000 µs
 * cleanly separates real frame boundaries from preamble noise.
 */
#define AF_FRAME_THRESH_US  20000u

/** Number of frame repetitions sent per TX keypress */
#define AF_TX_REPEAT        20

/**
 * TX buffer capacity.
 * Each bit produces 4 durations (p0, g0, p1, g1); small pad for safety.
 */
#define TX_BUF_SIZE         (AF_TX_REPEAT * AF_SYM_COUNT * 2 + 8)

/* ─── Enumerations ────────────────────────────────────────────────────────── */

/** Top-level UI state */
typedef enum {
    AppMode_Listen,    /**< Receiving and decoding frames                    */
    AppMode_Edit,      /**< Editing DIP code; ready to transmit              */
    AppMode_Transmit,  /**< Actively transmitting                            */
} AppMode;

/** RX state machine */
typedef enum {
    RxState_WaitGap,   /**< Haven't seen an inter-frame gap yet; not synced  */
    RxState_Receiving, /**< Collecting symbols for the current frame         */
} RxState;

/* ─── Main application struct ─────────────────────────────────────────────── */

typedef struct {
    /* ── GUI ─────────────────────────────────────────────────────────── */
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* event_queue;
    NotificationApp*  notifications;

    /* ── Radio ───────────────────────────────────────────────────────── */
    const SubGhzDevice* radio;   /**< Internal CC1101 device handle          */
    const GpioPin*      gdo0;    /**< CC1101 GDO0 data pin (from devices API)*/
    uint32_t            cpu_mhz; /**< CPU MHz, used for cycle→µs conversion  */

    /* ── App control ─────────────────────────────────────────────────── */
    AppMode mode;
    bool    running;

    /* ── RX state (volatile: written by IRQ, read by main thread) ────── */
    volatile RxState rx_state;
    volatile uint8_t rx_syms[AF_SYM_COUNT]; /**< 0=short(L), 1=long(H)     */
    volatile uint8_t rx_count;              /**< symbols collected so far   */
    volatile bool    rx_ready;              /**< true = frame ready for main */

    /* ── Decoded / editable DIP string ──────────────────────────────── */
    char dip[AF_BIT_COUNT + 1]; /**< '+', '-', or '0', null-terminated      */
    bool has_decode;            /**< at least one valid frame received       */
    int  cursor;                /**< edit cursor position, 0..AF_BIT_COUNT-1 */

    /* ── TX ──────────────────────────────────────────────────────────── */
    uint32_t          tx_buf[TX_BUF_SIZE]; /**< flat pulse/gap duration array */
    uint32_t          tx_size;             /**< valid entries in tx_buf       */
    volatile uint32_t tx_pos;             /**< current playback index        */
    volatile bool     tx_done;            /**< set by TX IRQ when complete   */

    /* ── Stats / diagnostics ────────────────────────────────────────── */
    uint32_t          frame_count; /**< valid frames decoded this session    */
    float             rssi_dbm;    /**< last RSSI reading, updated ~20 Hz    */
    volatile uint32_t rx_last_cyc; /**< DWT->CYCCNT at last GDO0 edge        */

} AllstarApp;

/* ─── Function declarations ───────────────────────────────────────────────── */

/**
 * App entry point — registered in application.fam.
 * @param p  unused (required by FAP ABI)
 * @return   0 on clean exit, -1 on allocation failure
 */
int32_t allstar_firefly_app(void* p);

/**
 * Decode 18 raw symbols into a 9-character DIP string.
 *
 * Symbol pair rules:
 *   H H -> '+'   H L -> '0'   L L -> '-'   L H -> invalid
 *
 * @param syms  18-element array of 0/1 symbol values (volatile, from IRQ)
 * @param dip   output buffer, must be at least AF_BIT_COUNT+1 bytes
 * @return      true on success; false if any invalid (L,H) pair is found
 */
bool af_decode_symbols(const volatile uint8_t* syms, char* dip);

/**
 * Build the flat TX pulse/gap buffer from the current DIP string.
 * Writes AF_TX_REPEAT copies of the 9-bit frame into app->tx_buf
 * and sets app->tx_size.
 *
 * @param app  application context (reads app->dip, writes app->tx_buf / tx_size)
 */
void af_build_tx_buf(AllstarApp* app);

/**
 * Configure CC1101 for OOK RX at AF_FREQ and register a raw GPIO EXTI
 * interrupt on GDO0 for edge timing.  Resets RX state machine first.
 *
 * Uses the GPIO interrupt path instead of subghz_devices_start_async_rx
 * to avoid HAL-state precondition issues in FAP context.
 *
 * @param app  application context (must have gdo0 and cpu_mhz initialised)
 */
void af_radio_start_rx(AllstarApp* app);

/**
 * Remove GDO0 interrupt and put CC1101 into idle.
 *
 * @param app  application context
 */
void af_radio_stop_rx(AllstarApp* app);

/**
 * Configure CC1101 for OOK TX at AF_FREQ and start async TX callback.
 * Caller must have called af_build_tx_buf() first.
 *
 * @param app  application context
 */
void af_radio_start_tx(AllstarApp* app);

/**
 * Stop async TX and put CC1101 into idle.
 *
 * @param app  application context
 */
void af_radio_stop_tx(AllstarApp* app);
#include "mf_ardf_hal.h"

#include "../common/mf_radio_tx_session.h"
#include "../radio/mf_radio_hal.h"

#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#endif

typedef struct {
    MfRadioTxSession tx;
#ifdef MORSE_FLIPPER_FAP
    NotificationApp* notifications;
#endif
} MfArdfHalContext;

static MfArdfHalContext hal;

static bool frequency_allowed(void* context, uint32_t frequency_hz, MfArdfModulation modulation) {
    (void)modulation;
    MfArdfHalContext* h = context;
    if(h->tx.hardware == NULL) mf_radio_tx_session_init(&h->tx, mf_radio_hal_ops());
    return mf_radio_tx_session_frequency_allowed(&h->tx, frequency_hz);
}

static bool prepare(void* context, uint32_t frequency_hz, MfArdfModulation modulation) {
    MfArdfHalContext* h = context;
    if(h->tx.hardware == NULL) mf_radio_tx_session_init(&h->tx, mf_radio_hal_ops());
    return mf_radio_tx_session_start(
        &h->tx,
        frequency_hz,
        modulation == MfArdfModulationCwfm ? MfRadioTxModeCwfm : MfRadioTxModeOok);
}

static bool set_mark(void* context, bool mark) {
    return mf_radio_tx_session_set_mark(&((MfArdfHalContext*)context)->tx, mark);
}

static void stop(void* context) {
    mf_radio_tx_session_stop(&((MfArdfHalContext*)context)->tx);
}

static void set_p15(void* context, bool high) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    furi_hal_gpio_write(&gpio_ext_pc1, high);
#else
    (void)high;
#endif
}

static void set_p16(void* context, bool high) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    furi_hal_gpio_write(&gpio_ext_pc0, high);
#else
    (void)high;
#endif
}

static void set_led(void* context, bool high) {
    MfArdfHalContext* h = context;
#ifdef MORSE_FLIPPER_FAP
    if(h->notifications != NULL)
        notification_message(
            h->notifications, high ? &sequence_set_only_blue_255 : &sequence_reset_rgb);
#else
    (void)h;
    (void)high;
#endif
}

static bool set_clock(void* context, MfArdfClockTime time) {
    (void)context;
#ifdef MORSE_FLIPPER_FAP
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);
    datetime.hour = time.hour;
    datetime.minute = time.minute;
    datetime.second = time.second;
    furi_hal_rtc_set_datetime(&datetime);
#else
    (void)time;
#endif
    return true;
}

static const MfArdfHardwareOps ops = {
    .frequency_allowed = frequency_allowed,
    .prepare = prepare,
    .set_mark = set_mark,
    .stop = stop,
    .set_p15 = set_p15,
    .set_p16 = set_p16,
    .set_led = set_led,
    .set_clock = set_clock,
    .context = &hal,
};

const MfArdfHardwareOps* mf_ardf_hal_ops(void) {
    return &ops;
}

void mf_ardf_hal_init(void) {
    mf_radio_tx_session_init(&hal.tx, mf_radio_hal_ops());
#ifdef MORSE_FLIPPER_FAP
    if(hal.notifications == NULL) hal.notifications = furi_record_open(RECORD_NOTIFICATION);
#endif
}

void mf_ardf_hal_rtc_sample(MfArdfState* state) {
#ifdef MORSE_FLIPPER_FAP
    DateTime datetime;
    uint32_t before = furi_get_tick();
    furi_hal_rtc_get_datetime(&datetime);
    uint32_t after = furi_get_tick();
    mf_ardf_core_rtc_sample(
        state, (MfArdfClockTime){datetime.hour, datetime.minute, datetime.second}, before, after);
#else
    (void)state;
#endif
}

void mf_ardf_hal_deinit(void) {
    mf_radio_tx_session_stop(&hal.tx);
#ifdef MORSE_FLIPPER_FAP
    if(hal.notifications != NULL) {
        notification_message(hal.notifications, &sequence_reset_rgb);
        furi_record_close(RECORD_NOTIFICATION);
    }
#endif
    memset(&hal, 0, sizeof(hal));
}

#include "air_check.h"

#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>
#include <ble_glue.h>
#include <stdlib.h>
#include <string.h>

/* Test-mode RF index is (MHz - 2402) / 2. The three BLE ADVERTISING channels
 * are the only ones a beaconing tracker ever uses. */
static const uint8_t adv_rf_index[AIR_ADV_CHANNELS] = {0, 12, 39};
const uint16_t air_adv_mhz[AIR_ADV_CHANNELS] = {2402, 2426, 2480};
const char* const air_adv_label[AIR_ADV_CHANNELS] = {"37", "38", "39"};

#define AIR_DATARATE_1M 1u
#define AIR_DWELL_MS    40u
#define AIR_SETTLE_MS   2u
#define AIR_SWEEP_MS    1500u

/* Histogram spans the whole range the radio can report, one bin per dB.
 *
 * An earlier version clamped to -100..-40, which was a bad bug rather than a
 * cosmetic one: the BLE stack returns exactly 0 dBm when an RSSI read FAILS,
 * and clamping that to the -40 ceiling filed every failed read as the loudest
 * sample of the sweep. The result was all three channels pinned at 100% and a
 * permanent verdict of BUSY, everywhere, including a quiet room - which is the
 * exact opposite of what somebody sweeping an empty garage needs to be told. */
#define HIST_MIN_DBM (-127)
#define HIST_MAX_DBM (-1)
#define HIST_BINS    128

/* A sample this far above the band's own floor counts as occupancy rather
 * than noise. Eight dB is deliberately conservative: it keeps thermal wobble
 * out of the bars, which matters because an inflated "busy" reading in a
 * genuinely quiet garage is exactly the wrong answer to give somebody. */
#define BUSY_MARGIN_DB 8

/* Below this many samples a sweep is not evidence of anything. */
#define MIN_SAMPLES 200u

struct AirCheck {
    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;

    /* Worker-owned. On the heap, not the worker's stack: three histograms is
     * 366 bytes and a 1 KB thread stack has better uses. */
    uint16_t hist[AIR_ADV_CHANNELS][HIST_BINS];
    uint32_t counted[AIR_ADV_CHANNELS];
    int8_t peak[AIR_ADV_CHANNELS];
    uint32_t started_tick;

    /* Shared. Guarded by mutex. */
    AirSnapshot snap;

    volatile bool c2_ok;
    volatile AirRfMode rf_mode;
    uint32_t sweeps_done;
    Bt* bt;
};

/* Bin index is simply -dbm, so bin 0 is unused and bins 1..127 are -1..-127. */
static int bin_of(int dbm) {
    if(dbm > HIST_MAX_DBM) dbm = HIST_MAX_DBM;
    if(dbm < HIST_MIN_DBM) dbm = HIST_MIN_DBM;
    return -dbm;
}

static void accum_reset(AirCheck* air) {
    memset(air->hist, 0, sizeof(air->hist));
    memset(air->counted, 0, sizeof(air->counted));
    for(size_t c = 0; c < AIR_ADV_CHANNELS; c++)
        air->peak[c] = HIST_MIN_DBM;
}

/** The quiet end of the distribution: the 20th percentile across all channels.
 *
 * Walked from the WEAKEST bin downwards, because bin index is -dbm - so the
 * quiet end of the band is the high end of the array. */
static int8_t band_floor(AirCheck* air) {
    uint32_t total = 0;
    for(size_t c = 0; c < AIR_ADV_CHANNELS; c++)
        total += air->counted[c];
    if(total == 0) return HIST_MIN_DBM;

    uint32_t want = total / 5; /* 20% */
    uint32_t seen = 0;
    for(int b = HIST_BINS - 1; b >= 1; b--) {
        for(size_t c = 0; c < AIR_ADV_CHANNELS; c++)
            seen += air->hist[c][b];
        if(seen >= want) return (int8_t)(-b);
    }
    return HIST_MAX_DBM;
}

static void publish(AirCheck* air) {
    int8_t floor = band_floor(air);
    int busy_from = floor + BUSY_MARGIN_DB;

    AirSnapshot s;
    memset(&s, 0, sizeof(s));
    s.floor_dbm = floor;
    s.elapsed_s = (furi_get_tick() - air->started_tick) / furi_kernel_get_tick_frequency();

    uint32_t total = 0;
    for(size_t c = 0; c < AIR_ADV_CHANNELS; c++) {
        /* "Above the floor" means a STRONGER signal, i.e. a SMALLER bin index. */
        uint32_t above = 0;
        int from = bin_of(busy_from);
        for(int b = 1; b <= from; b++)
            above += air->hist[c][b];
        uint32_t n = air->counted[c];
        s.busy_pct[c] = n ? (uint8_t)((above * 100u) / n) : 0;
        s.peak_dbm[c] = air->peak[c];
        total += n;
    }
    s.samples = total;
    s.valid = total >= MIN_SAMPLES;
    /* Distinguish "still filling up" from "this radio is handing us nothing".
     * Both drew the same word before, so a genuinely dead read looked exactly
     * like a slow start, forever. */
    s.dead = (total == 0) && (s.elapsed_s >= 8);
    s.radio_ready = air->c2_ok;
    s.rf_mode = air->rf_mode;

    furi_mutex_acquire(air->mutex, FuriWaitForever);
    air->snap = s;
    furi_mutex_release(air->mutex);
}

/*
 * There are two ways to put this radio into receive, and the SDK documents
 * neither well enough to pick one from the header.
 *
 *   furi_hal_bt_start_rx(channel)            - "set up the RF to listen"
 *   furi_hal_bt_start_packet_rx(channel, dr) - the BLE receiver test
 *
 * On official firmware (API 87) the first one leaves furi_hal_bt_get_rssi()
 * returning exactly 0 - a failed read - on every single sample, which is not
 * something the header hints at anywhere. So the worker tries the plain
 * listener first and falls back to the packet receiver if the first sweep
 * produced nothing usable, rather than shipping a guess. Whichever one yields
 * data is reported on screen, because "which RF path worked" is a fact about
 * the firmware in front of you, not a constant.
 */
static void dwell(AirCheck* air, size_t chan) {
    if(air->rf_mode == AirRfPacket) {
        furi_hal_bt_start_packet_rx(adv_rf_index[chan], AIR_DATARATE_1M);
    } else {
        furi_hal_bt_start_rx(adv_rf_index[chan]);
    }
    furi_delay_ms(AIR_SETTLE_MS);

    uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(AIR_DWELL_MS - AIR_SETTLE_MS);
    uint32_t n = 0;
    while(air->running && furi_get_tick() < deadline) {
        /* Each read is a round trip to the radio co-processor, which blocks
         * this thread and yields the CPU, so this is not normally a spin. The
         * periodic yield below keeps it from starving the GUI even if a read
         * ever returns immediately. */
        int dbm = (int)furi_hal_bt_get_rssi();
        /* The stack returns exactly 0 when the read FAILED. A genuine 0 dBm
         * would mean a transmitter touching the antenna, so dropping it costs
         * nothing and stops a failed read being filed as the loudest sample of
         * the sweep - which is precisely what pinned every channel at 100%. */
        if(dbm >= 0) {
            if((++n & 0x0Fu) == 0) furi_delay_tick(1);
            continue;
        }
        if(dbm < HIST_MIN_DBM) dbm = HIST_MIN_DBM;
        air->hist[chan][bin_of(dbm)]++;
        air->counted[chan]++;
        if(dbm > air->peak[chan]) air->peak[chan] = (int8_t)dbm;
        if((++n & 0x0Fu) == 0) furi_delay_tick(1);
    }

    if(air->rf_mode == AirRfPacket) {
        furi_hal_bt_stop_packet_test();
    } else {
        furi_hal_bt_stop_rx();
    }
}

static int32_t air_check_worker(void* context) {
    AirCheck* air = context;

    /* Hand the radio over: stop being a Bluetooth device so it can be a plain
     * receiver.
     *
     * The order here is not decoration. RF test mode lives on the WB55's
     * SECOND core, and asking for RSSI while that core is not running the
     * radio stack returns 0 for every single read - which is exactly what the
     * first hardware test of this mode produced: n=0, forever. Disconnecting
     * and stopping advertising frees the radio; ensure_c2_mode confirms the
     * core that actually answers the question is up. */
    air->bt = furi_record_open(RECORD_BT);
    bt_disconnect(air->bt);
    /* The second core needs a moment to flush its key storage. */
    furi_delay_ms(200);

    air->c2_ok = furi_hal_bt_ensure_c2_mode(BleGlueC2ModeStack);
    furi_hal_bt_stop_advertising();
    furi_delay_ms(100);

    air->started_tick = furi_get_tick();
    air->rf_mode = AirRfListen;
    air->sweeps_done = 0;
    accum_reset(air);

    uint32_t sweep_started = furi_get_tick();
    while(air->running) {
        for(size_t c = 0; c < AIR_ADV_CHANNELS && air->running; c++)
            dwell(air, c);

        /* Nothing at all from the plain listener: try the packet receiver once
         * before concluding the radio has nothing to say. */
        air->sweeps_done++;
        if(air->rf_mode == AirRfListen && air->sweeps_done >= 3) {
            uint32_t got = 0;
            for(size_t c = 0; c < AIR_ADV_CHANNELS; c++)
                got += air->counted[c];
            if(got == 0) {
                air->rf_mode = AirRfPacket;
                accum_reset(air);
                air->started_tick = furi_get_tick();
            }
        }

        publish(air);

        /* Roll the window rather than accumulating forever, so walking from a
         * busy room into a quiet one actually changes the reading instead of
         * being averaged away by the last ten minutes. */
        if((furi_get_tick() - sweep_started) > furi_ms_to_ticks(AIR_SWEEP_MS * 8)) {
            accum_reset(air);
            sweep_started = furi_get_tick();
        }
    }

    furi_hal_bt_stop_rx();
    /* Put the radio back the way it was found, or the Flipper's own Bluetooth
     * stays dead until the device is rebooted. */
    furi_hal_bt_reinit();
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(air->bt);
    bt_profile_restore_default(air->bt);
    furi_record_close(RECORD_BT);
    air->bt = NULL;

    return 0;
}

AirCheck* air_check_alloc(void) {
    AirCheck* air = malloc(sizeof(AirCheck));
    memset(air, 0, sizeof(AirCheck));
    air->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return air;
}

void air_check_free(AirCheck* air) {
    furi_assert(air);
    air_check_stop(air);
    furi_mutex_free(air->mutex);
    free(air);
}

void air_check_start(AirCheck* air) {
    furi_assert(air);
    if(air->running) return;

    furi_mutex_acquire(air->mutex, FuriWaitForever);
    memset(&air->snap, 0, sizeof(air->snap));
    furi_mutex_release(air->mutex);

    air->running = true;
    air->thread = furi_thread_alloc_ex("GhostTagAir", 1536, air_check_worker, air);
    /* Below the UI: a radio loop must never make a button feel dead. */
    furi_thread_set_priority(air->thread, FuriThreadPriorityLow);
    furi_thread_start(air->thread);
}

void air_check_stop(AirCheck* air) {
    furi_assert(air);
    if(!air->running) return;
    air->running = false;
    if(air->thread) {
        furi_thread_join(air->thread);
        furi_thread_free(air->thread);
        air->thread = NULL;
    }
}

bool air_check_is_running(AirCheck* air) {
    furi_assert(air);
    return air->running;
}

void air_check_snapshot(AirCheck* air, AirSnapshot* out) {
    furi_assert(air);
    furi_assert(out);
    furi_mutex_acquire(air->mutex, FuriWaitForever);
    *out = air->snap;
    furi_mutex_release(air->mutex);
}

AirBand air_check_band(const AirSnapshot* snap) {
    if(snap->dead) return AirBandNoReading;
    if(!snap->valid) return AirBandUnknown;
    /* The busiest advertising channel decides it: a tracker beacons on all
     * three, but plenty of other traffic favours one, and the question being
     * asked is "how hard will it be to pick something out of this". */
    uint8_t worst = 0;
    for(size_t c = 0; c < AIR_ADV_CHANNELS; c++) {
        if(snap->busy_pct[c] > worst) worst = snap->busy_pct[c];
    }
    if(worst >= 30) return AirBandBusy;
    if(worst >= 8) return AirBandModerate;
    return AirBandQuiet;
}

const char* air_band_label(AirBand band) {
    switch(band) {
    case AirBandQuiet:
        return "QUIET";
    case AirBandModerate:
        return "MODERATE";
    case AirBandBusy:
        return "BUSY";
    case AirBandNoReading:
        return "NO READING";
    default:
        return "SAMPLING";
    }
}

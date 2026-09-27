#include "demo_source.h"

#include <string.h>
#include <stdlib.h>

#define DEMO_TICK_MS   250
#define DEMO_LOOP_MS   75000UL /* the scenario restarts after this */
#define DEMO_NEVER     UINT32_MAX

struct DemoSource {
    FuriThread* thread;
    volatile bool running;
    uint32_t started_tick;

    DemoSourceRxCallback rx_cb;
    void* cb_context;
};

/**
 * One simulated broadcaster.
 *
 * RSSI walks linearly from @c rssi_from to @c rssi_to across the device's life,
 * which is what an approaching or receding transmitter actually looks like to a
 * scanner. @c jitter is the plus/minus wobble added on top - without it the
 * radar blips sit perfectly still and the whole thing reads as a cartoon.
 */
typedef struct {
    uint8_t mac[6];
    TrackerType type;
    const char* name;
    uint32_t appear_ms;
    uint32_t depart_ms; /* DEMO_NEVER = stays for the whole scenario */
    int8_t rssi_from;
    int8_t rssi_to;
    uint8_t jitter;
} DemoDevice;

/*
 * The scenario. It is written to teach the three things the app exists to
 * show, in the order somebody meets them:
 *
 *   1. Most of what a scanner hears is harmless. The phone and the earbuds are
 *      nearby Apple kit in owner-present mode - GhostTag sees them, lists them,
 *      and deliberately does NOT call them threats.
 *   2. A tracker being present is not a tracker following you. The Tile walks
 *      past, peaks, and is gone inside twenty seconds. It never trips.
 *   3. What following actually looks like: the AirTag arrives early, stays, and
 *      slowly gets stronger. At ~25 s it clears the dwell window and fires the
 *      alert. The SmartTag joins late so the list has a second follower in it.
 */
static const DemoDevice demo_cast[] = {
    /* ambient, harmless - owner-present Apple kit */
    {{0xD4, 0x9A, 0x20, 0x11, 0x4C, 0x8E},
     TrackerTypeAirTagPaired,
     "iPhone",
     0,
     DEMO_NEVER,
     -68,
     -61,
     4},
    {{0xA0, 0x99, 0x9B, 0x3C, 0x77, 0x12},
     TrackerTypeAirTagPaired,
     "AirPods Pro",
     1500,
     DEMO_NEVER,
     -77,
     -72,
     5},

    /* the stalker: arrives early, never leaves, closes in */
    {{0x6C, 0x4B, 0x90, 0x2D, 0xE1, 0x07},
     TrackerTypeAppleFindMy,
     "",
     2000,
     DEMO_NEVER,
     -79,
     -56,
     3},

    /* a passer-by: strong, then gone. Present, but never following. */
    {{0xE8, 0x1C, 0xFD, 0x55, 0x30, 0xB4},
     TrackerTypeTile,
     "Tile Mate",
     5000,
     23000,
     -84,
     -63,
     5},

    /* an unknown beacon - listed, never graded a threat */
    {{0x52, 0x11, 0x7A, 0x0C, 0x44, 0x99},
     TrackerTypeUnknown,
     "",
     8000,
     46000,
     -90,
     -86,
     6},

    /* a second follower, so the list has more than one row that matters */
    {{0x34, 0x2E, 0xB7, 0x68, 0x0A, 0x21},
     TrackerTypeSamsungSmartTag,
     "SmartTag2",
     30000,
     DEMO_NEVER,
     -81,
     -67,
     4},
};

#define DEMO_CAST_COUNT (sizeof(demo_cast) / sizeof(demo_cast[0]))

/* Deterministic wobble. A real PRNG would make every recording of the demo
 * different, and the screenshots in this repo are supposed to be reproducible. */
static int demo_jitter(uint32_t seed, uint8_t amplitude) {
    if(!amplitude) return 0;
    seed = seed * 1664525u + 1013904223u;
    return (int)((seed >> 16) % (amplitude * 2u + 1u)) - (int)amplitude;
}

static int8_t demo_rssi(const DemoDevice* d, uint32_t t_ms, uint32_t step) {
    uint32_t life_end = (d->depart_ms == DEMO_NEVER) ? DEMO_LOOP_MS : d->depart_ms;
    uint32_t span = (life_end > d->appear_ms) ? (life_end - d->appear_ms) : 1;
    uint32_t into = t_ms - d->appear_ms;
    if(into > span) into = span;

    int32_t from = d->rssi_from;
    int32_t to = d->rssi_to;
    int32_t level = from + ((to - from) * (int32_t)into) / (int32_t)span;
    level += demo_jitter(step * 31u + d->mac[5], d->jitter);

    if(level > -30) level = -30;
    if(level < -99) level = -99;
    return (int8_t)level;
}

static int32_t demo_source_worker(void* context) {
    DemoSource* demo = context;
    const uint32_t tick = furi_ms_to_ticks(DEMO_TICK_MS);
    uint32_t step = 0;

    while(demo->running) {
        uint32_t t_ms = (step * DEMO_TICK_MS) % DEMO_LOOP_MS;

        for(size_t i = 0; i < DEMO_CAST_COUNT && demo->running; i++) {
            const DemoDevice* d = &demo_cast[i];
            if(t_ms < d->appear_ms) continue;
            if(d->depart_ms != DEMO_NEVER && t_ms >= d->depart_ms) continue;

            /* A scanner does not hear every advert from every device on every
             * pass. Skipping some of them is what gives the sighting counts
             * their uneven, real-looking spread. */
            if(demo_jitter(step * 7u + (uint32_t)i, 4) < -2) continue;

            if(demo->rx_cb) {
                demo->rx_cb(
                    demo->cb_context, d->mac, d->type, demo_rssi(d, t_ms, step), d->name);
            }
        }

        step++;
        /* furi_delay_tick yields. furi_delay_us is a non-yielding busy-wait and
         * would starve the GUI service until the buttons stopped responding. */
        furi_delay_tick(tick);
    }
    return 0;
}

DemoSource* demo_source_alloc(void) {
    DemoSource* demo = malloc(sizeof(DemoSource));
    memset(demo, 0, sizeof(DemoSource));
    return demo;
}

void demo_source_free(DemoSource* demo) {
    furi_assert(demo);
    demo_source_stop(demo);
    free(demo);
}

void demo_source_set_callback(DemoSource* demo, DemoSourceRxCallback cb, void* context) {
    furi_assert(demo);
    demo->rx_cb = cb;
    demo->cb_context = context;
}

void demo_source_start(DemoSource* demo) {
    furi_assert(demo);
    if(demo->running) return;

    demo->running = true;
    demo->started_tick = furi_get_tick();
    demo->thread = furi_thread_alloc_ex("GhostTagDemo", 1024, demo_source_worker, demo);
    /* Below the UI, so a busy scenario can never make a button feel dead. */
    furi_thread_set_priority(demo->thread, FuriThreadPriorityLow);
    furi_thread_start(demo->thread);
}

void demo_source_stop(DemoSource* demo) {
    furi_assert(demo);
    if(!demo->running) return;
    demo->running = false;
    if(demo->thread) {
        furi_thread_join(demo->thread);
        furi_thread_free(demo->thread);
        demo->thread = NULL;
    }
}

bool demo_source_is_running(DemoSource* demo) {
    furi_assert(demo);
    return demo->running;
}

uint32_t demo_source_elapsed_ms(DemoSource* demo) {
    furi_assert(demo);
    if(!demo->running) return 0;
    return furi_get_tick() - demo->started_tick;
}

#include "../faraday_i.h"

/* Ticks (100 ms each) spent listening on each band before moving on.
 *
 * A fob does not transmit continuously - it sends short bursts with gaps
 * between them - so the question is not "is the carrier there" but "did we
 * happen to be listening on the right band while a burst was going out".
 * Measured across one session at 400 ms, the SAME fob on the SAME band read
 * -90, -67, -52, -84, -60 and -31 dBm on successive passes: the sweep was
 * regularly landing entirely inside a gap, and the scan then reported no fob.
 * 500 ms is long enough to cover a typical burst interval. */
#define FDY_BAND_DWELL_TICKS 5

/* Maximum sweeps. Each one is another chance to catch a burst. */
#define FDY_BAND_PASSES 5

/* ...but stop as soon as the answer is not in doubt, so a fob held properly
 * against the Flipper is done in about two seconds rather than ten. Only the
 * awkward cases pay for the extra passes. */
#define FDY_BAND_SURE_DBM (-60)
#define FDY_BAND_SURE_LEAD_DB 20

/* A band is only called when it is BOTH loud in absolute terms AND clearly
 * ahead of the runner-up. Both conditions are measured, not guessed, and
 * neither works alone:
 *
 *   - A LEAD test alone is fragile wherever a band carries real ambient
 *     traffic. Measured in one room: with no fob pressed, 315 MHz read -76 dBm
 *     against -90 on the other three, a 14 dB lead from tyre sensors and
 *     garage doors. Raising the lead threshold past that then broke the real
 *     case, because the same room's 315 ambient is the runner-up a genuine
 *     433.92 fob has to beat - a fob at -60 led by only 18 dB.
 *
 *   - An ABSOLUTE test alone would call a band on any nearby transmitter,
 *     which is exactly what 315 MHz ambient is.
 *
 * Together they separate the two cleanly: ambient fails the absolute bar, and
 * a fob held against the Flipper (-60 dBm and up) clears both comfortably.
 * Saying "no fob heard" is always available; naming the wrong band is not
 * recoverable, because every later measurement is then taken against noise. */
#define FDY_BAND_MIN_DBM (-70)
#define FDY_BAND_LEAD_DB 10

static void faraday_band_ok_cb(void* context) {
    FaradayApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, FaradayCustomEventOk);
}

/** Hand the view a complete frame. Used on entry as well as on every tick. */
static void faraday_band_publish(FaradayApp* app) {
    const FdyBandScan* s = &app->scan;
    BandData d;
    memset(&d, 0, sizeof(d));
    for(uint8_t i = 0; i < FDY_BAND_COUNT; i++) {
        d.label[i] = fdy_bands[i].label;
        d.peak[i] = s->peak[i];
        d.norm[i] = fdy_subghz_normalize(s->peak[i]);
    }
    d.active = s->active;
    d.pass = s->pass;
    d.passes = FDY_BAND_PASSES;
    d.done = s->done;
    d.found = s->found;
    d.weak = s->weak;
    d.winner = s->winner;
    d.margin = s->margin;
    band_view_update(app->band_view, &d);
}

static void faraday_band_reset(FaradayApp* app) {
    FdyBandScan* s = &app->scan;
    memset(s, 0, sizeof(*s));
    for(uint8_t i = 0; i < FDY_BAND_COUNT; i++) {
        s->peak[i] = FDY_RSSI_FLOOR_DBM;
    }
    fdy_subghz_set_freq(app->subghz, fdy_bands[0].frequency);
    fdy_subghz_reset_peak(app->subghz);
}

void faraday_scene_band_on_enter(void* context) {
    FaradayApp* app = context;

    faraday_band_reset(app);
    band_view_set_ok_callback(app->band_view, faraday_band_ok_cb, app);

    fdy_subghz_start(app->subghz);

    /* Push a populated frame BEFORE switching, or the first thing drawn comes
     * from a zeroed model - blank band labels and "Scanning 1/0", because the
     * total pass count had not been sent yet. */
    faraday_band_publish(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, FaradayViewBand);
}

/** Already beyond argument? Loud in absolute terms and far ahead of the rest. */
static bool faraday_band_certain(const FdyBandScan* s) {
    uint8_t best = 0;
    for(uint8_t i = 1; i < FDY_BAND_COUNT; i++) {
        if(s->peak[i] > s->peak[best]) best = i;
    }
    int16_t runner = FDY_RSSI_FLOOR_DBM;
    for(uint8_t i = 0; i < FDY_BAND_COUNT; i++) {
        if(i != best && s->peak[i] > runner) runner = s->peak[i];
    }
    return s->peak[best] >= FDY_BAND_SURE_DBM &&
           (int16_t)(s->peak[best] - runner) >= FDY_BAND_SURE_LEAD_DB;
}

/** Decide, once every pass is in. */
static void faraday_band_decide(FaradayApp* app) {
    FdyBandScan* s = &app->scan;

    uint8_t best = 0;
    for(uint8_t i = 1; i < FDY_BAND_COUNT; i++) {
        if(s->peak[i] > s->peak[best]) best = i;
    }
    /* Lead over the strongest OTHER band, not over the average: averaging
     * would let one very quiet band flatter a winner that is really just
     * joint-loudest with its neighbour. */
    int16_t runner = FDY_RSSI_FLOOR_DBM;
    for(uint8_t i = 0; i < FDY_BAND_COUNT; i++) {
        if(i != best && s->peak[i] > runner) runner = s->peak[i];
    }

    s->winner = best;
    s->margin = (int16_t)(s->peak[best] - runner);
    s->found = (s->peak[best] >= FDY_BAND_MIN_DBM) && (s->margin >= FDY_BAND_LEAD_DB);
    /* Led its neighbours but was not loud enough to be a fob in the hand:
     * that is a distance problem, and saying so is worth more than a flat
     * refusal the user cannot act on. */
    s->weak = !s->found && (s->margin >= FDY_BAND_LEAD_DB);
    s->done = true;

    if(s->found) {
        faraday_notify_lock(app);
    } else {
        faraday_notify_reject(app);
    }
}

bool faraday_scene_band_on_event(void* context, SceneManagerEvent event) {
    FaradayApp* app = context;
    FdyBandScan* s = &app->scan;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == FaradayCustomEventOk) {
            if(!s->done) {
                consumed = true; // still sweeping; let it finish
            } else if(s->found) {
                /* Adopt it. This is the entire point of the screen: the user
                 * should never have to know what band their car is on. */
                app->settings.band_index = s->winner;
                fdy_store_settings_save(&app->settings);
                faraday_notify_lock(app);
                scene_manager_previous_scene(app->scene_manager);
                consumed = true;
            } else {
                faraday_band_reset(app); // nothing found: sweep again
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(!s->done) {
            FdySubGhzSnapshot sn;
            fdy_subghz_get(app->subghz, &sn);

            /* Record the loudest thing heard on this band. The peak-hold is
             * reset on every retune, so this is only ever this band's own. */
            if(sn.peak > s->peak[s->active]) s->peak[s->active] = sn.peak;

            if(++s->dwell >= FDY_BAND_DWELL_TICKS) {
                s->dwell = 0;
                s->active = (uint8_t)((s->active + 1) % FDY_BAND_COUNT);
                if(s->active == 0) {
                    s->pass++;
                    /* Decide early when a band is already beyond argument, or
                     * when the passes have run out. */
                    if(faraday_band_certain(s) || s->pass >= FDY_BAND_PASSES) {
                        faraday_band_decide(app);
                    }
                }
                if(!s->done) {
                    fdy_subghz_set_freq(app->subghz, fdy_bands[s->active].frequency);
                    fdy_subghz_reset_peak(app->subghz);
                }
            }
        }

        faraday_band_publish(app);
        consumed = true;
    }
    return consumed;
}

void faraday_scene_band_on_exit(void* context) {
    FaradayApp* app = context;
    fdy_subghz_stop(app->subghz);
    /* Leave the radio tuned to whatever the user is actually going to test on,
     * so the next scene does not have to undo this screen's last hop. */
    uint8_t bi = app->settings.band_index;
    if(bi >= FDY_BAND_COUNT) bi = 1;
    fdy_subghz_set_freq(app->subghz, fdy_bands[bi].frequency);
}

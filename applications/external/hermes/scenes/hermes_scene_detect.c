#include "../hermes_i.h"
#include <string.h>

/* Re-fitting costs a couple of milliseconds, so it runs at 5 Hz rather than on
 * every frame - fast enough for the number on screen to feel live. */
#define DETECT_FIT_INTERVAL_MS (200u)

typedef enum {
    DetectStateListening,
    DetectStateVerifying,
} DetectState;

/* ------------------------------------------------------------- results ---- */

/** Copy the phase-A runner-ups in, best score first, skipping the winner. */
static void hermes_detect_add_runners_up(HermesApp* app, uint32_t winner_baud) {
    const VerifierReport* r = verifier_report(app->verifier);
    bool used[HERMES_BAUD_COUNT] = {0};

    for(uint8_t slot = 1; slot < RESULT_MAX_ENTRIES; slot++) {
        int best = -1;
        for(uint8_t i = 0; i < r->entry_count; i++) {
            if(used[i]) continue;
            if(r->entries[i].baud == winner_baud) continue;
            if(r->entries[i].bytes == 0) continue; // silence is not a candidate
            if(best < 0 || r->entries[i].score > r->entries[best].score) best = i;
        }
        if(best < 0) break;
        used[best] = true;

        const VerifierEntry* v = &r->entries[best];
        const ResultEntry e = {
            .baud = v->baud,
            .framing = v->framing,
            .confidence = v->score,
            .verified = true,
            .bytes = v->bytes,
            .printable_pct = v->printable_pct,
            .note = hermes_baud_table[hermes_baud_nearest_index(v->baud)].note,
        };
        result_view_add(app->result_view, &e);
    }
}

/** Turn whatever the two stages learned into the result screen.
 *
 * Bytes read on the real UART outrank a timing fit, and when nothing was heard
 * at all the screen says so rather than dressing up a guess as a measurement.
 */
static bool hermes_detect_build_results(HermesApp* app) {
    const VerifierReport* r = verifier_report(app->verifier);
    result_view_reset(app->result_view);

    if(r->saw_traffic && r->best.bytes > 0) {
        const ResultEntry e = {
            .baud = r->best.baud,
            .framing = r->best.framing,
            .confidence = r->best.score,
            .verified = true,
            .bytes = r->best.bytes,
            .printable_pct = r->best.printable_pct,
            .note = hermes_baud_table[hermes_baud_nearest_index(r->best.baud)].note,
        };
        result_view_add(app->result_view, &e);
        hermes_detect_add_runners_up(app, r->best.baud);
        result_view_set_unverified(app->result_view, false);
        return true;
    }

    /* The line went quiet between the fit and the sweep. The timing still says
     * something, so offer it - clearly labelled as unverified. */
    const AutobaudResult* d = &app->detect_result;
    if(d->verdict != AutobaudVerdictOk || d->candidate_count == 0) return false;

    for(uint8_t i = 0; i < d->candidate_count && i < RESULT_MAX_ENTRIES; i++) {
        const AutobaudCandidate* c = &d->candidates[i];
        const ResultEntry e = {
            .baud = c->baud,
            .framing = HermesFraming8N1, // no traffic means no framing evidence
            .confidence = c->confidence,
            .verified = false,
            .bytes = 0,
            .printable_pct = 0,
            .note = hermes_baud_table[hermes_baud_nearest_index(c->baud)].note,
        };
        result_view_add(app->result_view, &e);
    }

    char detail[28];
    snprintf(
        detail, sizeof(detail), "%u edges - %u%% fit", (unsigned)d->edges_used, d->fit_percent);
    result_view_set_detail(app->result_view, detail);
    result_view_set_unverified(app->result_view, true);
    return true;
}

/* ------------------------------------------------------------ verifier ---- */

static void hermes_detect_verifier_callback(void* context) {
    HermesApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        verifier_is_running(app->verifier) ? HermesCustomEventVerifyTick :
                                             HermesCustomEventVerifyDone);
}

/** Stop listening and hand the shortlist to the UART for a second opinion. */
static void hermes_detect_begin_verify(HermesApp* app) {
    autobaud_analyze(app->autobaud, &app->detect_result);
    autobaud_stop(app->autobaud);

    uint32_t bauds[HERMES_BAUD_COUNT];
    size_t count = 0;

    if(app->detect_result.verdict == AutobaudVerdictOk) {
        for(uint8_t i = 0; i < app->detect_result.candidate_count; i++) {
            bauds[count++] = app->detect_result.candidates[i].baud;
        }
    } else {
        /* No usable timing - above ~230400 the interrupt cannot keep up with the
         * edges. The UART itself has no such limit, so sweep the whole table. */
        for(size_t i = 0; i < HERMES_BAUD_COUNT; i++) {
            bauds[count++] = hermes_baud_table[i].baud;
        }
    }

    scene_manager_set_scene_state(app->scene_manager, HermesSceneDetect, DetectStateVerifying);
    detect_view_set_phase(app->detect_view, DetectPhaseVerifying);
    detect_view_set_verify(app->detect_view, 0, bauds[0]);

    verifier_set_callback(app->verifier, hermes_detect_verifier_callback, app);
    if(!verifier_start(app->verifier, app->port, bauds, count)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventVerifyDone);
    }
}

/* --------------------------------------------------------------- listen --- */

static void hermes_detect_tick_listening(HermesApp* app) {
    AutobaudSegment segs[AUTOBAUD_TRACE_EDGES];
    const size_t n = autobaud_trace(app->autobaud, segs, AUTOBAUD_TRACE_EDGES);
    detect_view_set_trace(app->detect_view, segs, n, app->detect_rt.bit_time);

    const uint32_t edges = autobaud_edge_count(app->autobaud);
    detect_view_set_listen(app->detect_view, edges, AUTOBAUD_MAX_EDGES, app->detect_rt.live_baud);
    detect_view_set_idle(app->detect_view, autobaud_line_idle_high(app->autobaud));

    const uint32_t now = furi_get_tick();
    if(now - app->detect_rt.last_fit_tick >= furi_ms_to_ticks(DETECT_FIT_INTERVAL_MS)) {
        app->detect_rt.last_fit_tick = now;

        AutobaudResult r;
        autobaud_analyze(app->autobaud, &r);
        if(r.verdict == AutobaudVerdictOk && r.candidate_count > 0) {
            app->detect_rt.live_baud = r.candidates[0].baud;
            app->detect_rt.bit_time = r.bit_time_cycles;
        }
    }

    const bool full = autobaud_is_full(app->autobaud);
    const bool timed_out = (now - app->detect_rt.started_tick) >=
                           furi_ms_to_ticks(HERMES_LISTEN_TIMEOUT_MS);

    if(full || timed_out) hermes_detect_begin_verify(app);
}

/* ---------------------------------------------------------------- scene --- */

void hermes_scene_detect_on_enter(void* context) {
    HermesApp* app = context;

    memset(&app->detect_rt, 0, sizeof(HermesDetectRuntime));
    memset(&app->detect_result, 0, sizeof(AutobaudResult));
    app->detect_rt.started_tick = furi_get_tick();
    app->detect_rt.last_fit_tick = furi_get_tick();

    detect_view_reset(app->detect_view);
    detect_view_set_port(
        app->detect_view, hermes_port_name(app->port), hermes_port_pins(app->port));
    detect_view_set_phase(app->detect_view, DetectPhaseListening);

    scene_manager_set_scene_state(app->scene_manager, HermesSceneDetect, DetectStateListening);
    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewDetect);

    if(!autobaud_start(app->autobaud, app->port)) {
        /* Someone else holds the port. Bail out via the queue rather than
         * navigating from inside on_enter. */
        hermes_notify_found(app, false);
        view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventPortBusy);
    }
}

bool hermes_scene_detect_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        const uint32_t state =
            scene_manager_get_scene_state(app->scene_manager, HermesSceneDetect);
        if(state == DetectStateListening) hermes_detect_tick_listening(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case HermesCustomEventPortBusy:
            scene_manager_previous_scene(app->scene_manager);
            return true;

        case HermesCustomEventVerifyTick:
            detect_view_set_verify(
                app->detect_view,
                verifier_progress(app->verifier),
                verifier_current_baud(app->verifier));
            return true;

        case HermesCustomEventVerifyDone: {
            const bool found = hermes_detect_build_results(app);
            hermes_notify_found(app, found);
            scene_manager_next_scene(app->scene_manager, HermesSceneResult);
            return true;
        }

        default:
            break;
        }
    }

    return false;
}

void hermes_scene_detect_on_exit(void* context) {
    HermesApp* app = context;
    /* Both are safe when idle, and both must release the port before the next
     * scene tries to claim it. */
    autobaud_stop(app->autobaud);
    verifier_stop(app->verifier);
}

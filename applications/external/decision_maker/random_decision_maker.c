/**
 * @file random_decision_maker.c
 * @brief Random Decision Maker — type your own choices and let the roulette decide.
 *
 * Controls — Manage screen:
 *   UP / DOWN   Navigate the list
 *   OK          Add a new decision (cursor on "Añadir") / select to spin (cursor on a decision)
 *   LEFT        Delete the highlighted decision
 *   RIGHT       Spin the roulette (requires ≥ 2 decisions)
 *   BACK        Exit the app
 *
 * Controls — Keyboard screen:
 *   Type your decision, press OK to confirm, BACK to cancel.
 *
 * Controls — Result screen:
 *   OK    Spin again
 *   BACK  Return to the manage screen
 *
 * Architecture:
 *   ViewDispatcher manages three screens:
 *     ViewIdManage    — scrollable list of decisions + "Añadir" button
 *     ViewIdTextInput — built-in TextInput keyboard module
 *     ViewIdSpinning  — roulette animation
 *     ViewIdResult    — final winner display
 *
 *   A FuriTimer fires periodically during the spin; its callback posts a
 *   custom event to ViewDispatcher, which processes it safely on the GUI
 *   thread via app_custom_event_callback().
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <input/input.h>
#include <furi_hal_random.h>
#include <string.h>
#include <stdlib.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * 1.  Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum number of decisions the user can enter. */
#define MAX_DECISIONS    20u

/**
 * @brief Buffer size per decision, including the null terminator.
 * Effective maximum length shown on screen is MAX_DECISION_LEN - 1 = 20 chars.
 * FontPrimary (~6 px/char) × 20 = 120 px ≤ 124 px frame width.
 */
#define MAX_DECISION_LEN 21u

/** Number of rows visible in the manage-screen list at once. */
#define MENU_VISIBLE     4u

/** Minimum full rotations through the list before the wheel decelerates. */
#define SPIN_BASE_CYCLES 3u

/** Fast animation frame interval (ms). */
#define SPIN_FAST_MS     60u

/** Slow / deceleration animation frame interval (ms). */
#define SPIN_SLOW_MS     220u

/* ═══════════════════════════════════════════════════════════════════════════
 * 2.  View IDs and custom events
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Identifiers for each view managed by the ViewDispatcher. */
typedef enum {
    ViewIdManage    = 0,
    ViewIdTextInput = 1,
    ViewIdSpinning  = 2,
    ViewIdResult    = 3,
} ViewId;

/** Custom events sent from the spin timer to the ViewDispatcher. */
typedef enum {
    CustomEventSpinTick = 0,
} CustomEvent;

/* ═══════════════════════════════════════════════════════════════════════════
 * 3.  View models
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Model for the manage-decisions screen.
 *
 * The list has (count + 1) entries: the decisions followed by an "Añadir"
 * button (unless count == MAX_DECISIONS, in which case it is hidden).
 */
typedef struct {
    char    items[MAX_DECISIONS][MAX_DECISION_LEN]; /**< Decision strings.   */
    uint8_t count;  /**< Number of decisions entered so far.                 */
    uint8_t cursor; /**< Highlighted row index (count = "Añadir" row).       */
    uint8_t scroll; /**< Index of the topmost visible row.                   */
} ManageModel;

/** @brief Model for the spinning animation screen. */
typedef struct {
    char    options[MAX_DECISIONS][MAX_DECISION_LEN]; /**< Snapshot of decisions. */
    uint8_t count;       /**< Number of options in this spin run.            */
    uint8_t current;     /**< Index of the currently displayed option.       */
    uint8_t steps;       /**< Animation frames elapsed.                      */
    uint8_t total;       /**< Total frames for this spin run.                */
    uint8_t fast_steps;  /**< Frames executed at full speed.                 */
} SpinModel;

/** @brief Model for the result screen. */
typedef struct {
    char winner[MAX_DECISION_LEN]; /**< The chosen decision string. */
} ResultModel;

/* ═══════════════════════════════════════════════════════════════════════════
 * 4.  Application context
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    ViewDispatcher* view_dispatcher;

    View*       view_manage;   /**< Custom manage-decisions view.   */
    TextInput*  text_input;    /**< Built-in keyboard module.       */
    View*       view_spinning; /**< Custom roulette animation view. */
    View*       view_result;   /**< Custom result display view.     */

    /** Buffer written by TextInput; read in the done callback. */
    char input_buffer[MAX_DECISION_LEN];

    /** Periodic timer that drives the roulette animation. */
    FuriTimer* spin_timer;
} App;

/* ═══════════════════════════════════════════════════════════════════════════
 * 5.  Forward declarations
 * ═══════════════════════════════════════════════════════════════════════════ */

static void start_spin(App* app);
static void text_input_done_callback(void* context);

/* ═══════════════════════════════════════════════════════════════════════════
 * 6.  Spin timer callback  (runs in the FuriTimer thread)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Posts a CustomEventSpinTick to the ViewDispatcher.
 *
 * This function runs in the timer thread; it must not touch any shared data
 * directly.  view_dispatcher_send_custom_event() is thread-safe.
 */
static void spin_timer_callback(void* context) {
    App* app = (App*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)CustomEventSpinTick);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7.  Manage screen
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Draws the list of decisions and the "Añadir" button.
 *
 * Layout (128 × 64 px):
 *   [0–11]   Title bar + separator line
 *   [12–51]  Four 10-px option rows
 *   [55–63]  Context-sensitive hint bar
 */
static void manage_draw_callback(Canvas* canvas, void* model_ptr) {
    ManageModel* m         = (ManageModel*)model_ptr;
    bool         has_add   = (m->count < MAX_DECISIONS);
    uint8_t      total     = (uint8_t)(m->count + (has_add ? 1u : 0u));

    canvas_clear(canvas);

    /* ── Title bar ── */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Mis Decisiones");
    canvas_draw_line(canvas, 0, 11, 127, 11);

    /* ── Option rows ── */
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < MENU_VISIBLE; i++) {
        uint8_t idx   = (uint8_t)(m->scroll + i);
        if(idx >= total) break;

        uint8_t row_y    = (uint8_t)(12 + i * 10);
        bool    selected = (idx == m->cursor);

        if(selected) {
            canvas_draw_box(canvas, 0, row_y, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }

        if(idx < m->count) {
            canvas_draw_str(canvas, 3, (uint8_t)(row_y + 8), m->items[idx]);
        } else {
            /* "Add" button */
            canvas_draw_str(canvas, 3, (uint8_t)(row_y + 8), "+ Anadir decision");
        }

        if(selected) canvas_set_color(canvas, ColorBlack);
    }

    /* ── Scroll arrows ── */
    if(m->scroll > 0)
        canvas_draw_str(canvas, 122, 19, "^");
    if((uint8_t)(m->scroll + MENU_VISIBLE) < total)
        canvas_draw_str(canvas, 122, 50, "v");

    /* ── Context-sensitive hint ── */
    bool on_decision = (m->cursor < m->count);
    bool can_spin    = (m->count >= 2u);

    if(on_decision && can_spin) {
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "[<]Borrar [>]Girar");
    } else if(on_decision) {
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "[<]Borrar (min.2 decisiones)");
    } else if(can_spin) {
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "[OK]Anadir [>]Girar");
    } else {
        canvas_draw_str_aligned(
            canvas, 64, 63, AlignCenter, AlignBottom, "[OK]Anadir (min.2 para girar)");
    }
}

/**
 * @brief Handles button presses on the manage screen.
 *
 * UP/DOWN support held-down repeat; all other keys respond to short press only.
 */
static bool manage_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;

    /* Accept short presses and held-key repeats (for scrolling). */
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    /* LEFT / RIGHT / OK only on short press to prevent accidental actions. */
    bool is_repeat = (event->type == InputTypeRepeat);
    if(is_repeat &&
       (event->key == InputKeyLeft || event->key == InputKeyRight ||
        event->key == InputKeyOk   || event->key == InputKeyBack)) {
        return false;
    }

    bool open_keyboard = false;
    bool do_spin       = false;
    bool do_exit       = false;

    with_view_model(
        app->view_manage,
        ManageModel* m,
        {
            uint8_t has_add    = (m->count < MAX_DECISIONS) ? 1u : 0u;
            uint8_t total      = (uint8_t)(m->count + has_add);
            bool    on_decision = (m->cursor < m->count);

            switch(event->key) {
            case InputKeyUp:
                if(m->cursor > 0) {
                    m->cursor--;
                    if(m->cursor < m->scroll) m->scroll = m->cursor;
                }
                break;

            case InputKeyDown:
                if(m->cursor < (uint8_t)(total - 1)) {
                    m->cursor++;
                    if(m->cursor >= (uint8_t)(m->scroll + MENU_VISIBLE))
                        m->scroll = (uint8_t)(m->cursor - MENU_VISIBLE + 1);
                }
                break;

            case InputKeyOk:
                if(!on_decision && m->count < MAX_DECISIONS) {
                    /* Cursor is on "Añadir" — open the keyboard. */
                    open_keyboard = true;
                } else if(on_decision && m->count >= 2u) {
                    /* Pressing OK on a decision also spins (shortcut). */
                    do_spin = true;
                }
                break;

            case InputKeyLeft:
                /* Delete the highlighted decision. */
                if(on_decision) {
                    for(uint8_t i = m->cursor; i < (uint8_t)(m->count - 1); i++) {
                        memcpy(m->items[i], m->items[i + 1], MAX_DECISION_LEN);
                    }
                    m->count--;
                    /* Clamp cursor: if it now points past the list, put it on "Añadir". */
                    uint8_t new_total = (uint8_t)(m->count + 1);
                    if(m->cursor >= new_total) m->cursor = m->count;
                    /* Adjust scroll so cursor stays visible. */
                    if(m->scroll > 0 && m->cursor < m->scroll) m->scroll--;
                }
                break;

            case InputKeyRight:
                if(m->count >= 2u) do_spin = true;
                break;

            case InputKeyBack:
                do_exit = true;
                break;

            default:
                break;
            }
        },
        true /* redraw */);

    /* ── Post-model actions (must happen outside with_view_model) ── */
    if(do_exit) {
        view_dispatcher_stop(app->view_dispatcher);
    } else if(open_keyboard) {
        /* Prepare TextInput and switch to it. */
        app->input_buffer[0] = '\0';
        text_input_reset(app->text_input);
        text_input_set_header_text(app->text_input, "Nueva decision:");
        text_input_set_result_callback(
            app->text_input,
            text_input_done_callback,
            app,
            app->input_buffer,
            MAX_DECISION_LEN,
            true /* clear default text */);
        text_input_set_minimum_length(app->text_input, 1u);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdTextInput);
    } else if(do_spin) {
        start_spin(app);
    }

    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8.  TextInput done callback
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Called by TextInput when the user confirms their text with OK.
 *
 * Appends the entered string to the decision list (if non-empty) and returns
 * to the manage screen.
 */
static void text_input_done_callback(void* context) {
    App* app = (App*)context;

    if(app->input_buffer[0] != '\0') {
        with_view_model(
            app->view_manage,
            ManageModel* m,
            {
                if(m->count < MAX_DECISIONS) {
                    /* Copy the new decision into the list. */
                    strncpy(m->items[m->count], app->input_buffer, MAX_DECISION_LEN - 1);
                    m->items[m->count][MAX_DECISION_LEN - 1] = '\0';

                    /* Move cursor to the newly added item. */
                    m->cursor = m->count;
                    m->count++;

                    /* Scroll so the new item is visible. */
                    if(m->cursor >= (uint8_t)(m->scroll + MENU_VISIBLE))
                        m->scroll = (uint8_t)(m->cursor - MENU_VISIBLE + 1);
                }
            },
            true /* redraw */);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdManage);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9.  Spin helper
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Copies the current decisions into the spin model, calculates timing,
 *        starts the timer, and switches to the spinning view.
 *
 * The total step count is engineered so the wheel lands exactly on the
 * randomly chosen target: total = (BASE_CYCLES × count) + delta, where delta
 * is the forward distance from start to target (1 … count).
 */
static void start_spin(App* app) {
    /* ── Snapshot decisions from manage model ── */
    char    decisions[MAX_DECISIONS][MAX_DECISION_LEN];
    uint8_t count = 0;

    with_view_model(
        app->view_manage,
        ManageModel* m,
        {
            count = m->count;
            for(uint8_t i = 0; i < count; i++) {
                strncpy(decisions[i], m->items[i], MAX_DECISION_LEN - 1);
                decisions[i][MAX_DECISION_LEN - 1] = '\0';
            }
        },
        false /* no redraw needed */);

    if(count < 2u) return;

    /* ── Choose a random start position and a random winner ── */
    uint8_t start  = (uint8_t)(furi_hal_random_get() % count);
    uint8_t target = (uint8_t)(furi_hal_random_get() % count);
    uint8_t delta  = (uint8_t)((target - start + count) % count);
    if(delta == 0) delta = count; /* At least one full pass. */

    uint8_t fast_steps = (uint8_t)(SPIN_BASE_CYCLES * count);
    uint8_t total      = (uint8_t)(fast_steps + delta);

    /* ── Populate spin model ── */
    with_view_model(
        app->view_spinning,
        SpinModel* sm,
        {
            sm->count      = count;
            sm->current    = start;
            sm->steps      = 0;
            sm->total      = total;
            sm->fast_steps = fast_steps;
            for(uint8_t i = 0; i < count; i++) {
                strncpy(sm->options[i], decisions[i], MAX_DECISION_LEN - 1);
                sm->options[i][MAX_DECISION_LEN - 1] = '\0';
            }
        },
        false);

    /* ── Start timer and switch view ── */
    furi_timer_start(app->spin_timer, SPIN_FAST_MS);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdSpinning);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10.  Spinning screen
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Draws the roulette frame and progress bar.
 *
 * Layout:
 *   [0–12]   "Girando..." header
 *   [14–44]  Rounded frame with the current option centred inside
 *   [50–56]  Progress bar (fills left → right)
 *   [57–63]  "Esperando..." hint
 */
static void spinning_draw_callback(Canvas* canvas, void* model_ptr) {
    SpinModel* m = (SpinModel*)model_ptr;

    canvas_clear(canvas);

    /* Header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "Girando...");

    /* Roulette frame */
    canvas_draw_rframe(canvas, 2, 14, 124, 30, 4);
    canvas_draw_str_aligned(canvas, 64, 29, AlignCenter, AlignCenter, m->options[m->current]);

    /* Progress bar */
    uint8_t fill = (m->total > 0) ? (uint8_t)((m->steps * 116u) / m->total) : 0u;
    canvas_draw_frame(canvas, 6, 50, 116, 7);
    if(fill > 0) canvas_draw_box(canvas, 6, 50, fill, 7);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Esperando...");
}

/** Consumes all input while the animation is running. */
static bool spinning_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11.  Result screen
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Draws the winner inside a rounded frame.
 *
 * Layout:
 *   [0–13]   "La decision es:" header + separator
 *   [16–48]  Rounded frame with the winner centred inside
 *   [55–63]  Hint bar
 */
static void result_draw_callback(Canvas* canvas, void* model_ptr) {
    ResultModel* m = (ResultModel*)model_ptr;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 1, AlignCenter, AlignTop, "La decision es:");
    canvas_draw_line(canvas, 0, 13, 127, 13);

    canvas_draw_rframe(canvas, 2, 16, 124, 32, 4);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, m->winner);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas, 64, 63, AlignCenter, AlignBottom, "[OK]Otra vez [BACK]Menu");
}

/** Handles OK (spin again) and BACK (return to manage) on the result screen. */
static bool result_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        start_spin(app);
        return true;
    }
    if(event->key == InputKeyBack) {
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdManage);
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 12.  ViewDispatcher callbacks
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Processes custom events sent by the spin timer.
 *
 * Called on the GUI/ViewDispatcher thread — safe to update view models here.
 *
 * Each tick:
 *   1. Advances spin_current by one step.
 *   2. At fast_steps, restarts the timer at the slower interval.
 *   3. At total steps, stops the timer and transitions to the result screen.
 */
static bool app_custom_event_callback(void* context, uint32_t event) {
    App* app = (App*)context;

    if((CustomEvent)event != CustomEventSpinTick) return false;

    bool   finished = false;
    char   winner[MAX_DECISION_LEN];
    winner[0] = '\0';

    with_view_model(
        app->view_spinning,
        SpinModel* m,
        {
            m->steps++;
            m->current = (uint8_t)((m->current + 1u) % m->count);

            if(m->steps >= m->total) {
                /* Animation complete. */
                furi_timer_stop(app->spin_timer);
                strncpy(winner, m->options[m->current], MAX_DECISION_LEN - 1);
                winner[MAX_DECISION_LEN - 1] = '\0';
                finished = true;

            } else if(m->steps == m->fast_steps) {
                /* Transition to the slow / deceleration phase. */
                furi_timer_stop(app->spin_timer);
                furi_timer_start(app->spin_timer, SPIN_SLOW_MS);
            }
        },
        !finished /* redraw spinning view only while still spinning */);

    if(finished) {
        /* Populate the result model and switch to the result screen. */
        with_view_model(
            app->view_result,
            ResultModel* rm,
            {
                strncpy(rm->winner, winner, MAX_DECISION_LEN - 1);
                rm->winner[MAX_DECISION_LEN - 1] = '\0';
            },
            true);
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdResult);
    }

    return true;
}

/**
 * @brief Called when BACK is pressed and the current view does not consume it.
 *
 * This handles the BACK key while inside the TextInput keyboard (when the
 * text buffer is empty, TextInput passes BACK up to the dispatcher).
 */
static bool app_navigation_event_callback(void* context) {
    App* app = (App*)context;
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdManage);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 13.  Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Allocates and fully initialises every application resource. */
static App* app_alloc(void) {
    App* app = malloc(sizeof(App));

    /* ── ViewDispatcher ── */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, app_navigation_event_callback);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    /* ── Manage view ── */
    app->view_manage = view_alloc();
    view_allocate_model(app->view_manage, ViewModelTypeLockFree, sizeof(ManageModel));
    view_set_draw_callback(app->view_manage, manage_draw_callback);
    view_set_input_callback(app->view_manage, manage_input_callback);
    view_set_context(app->view_manage, app);
    /* Initialise model fields to zero/empty. */
    with_view_model(app->view_manage, ManageModel* m, { memset(m, 0, sizeof(ManageModel)); }, false);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdManage, app->view_manage);

    /* ── TextInput ── */
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, ViewIdTextInput, text_input_get_view(app->text_input));

    /* ── Spinning view ── */
    app->view_spinning = view_alloc();
    view_allocate_model(app->view_spinning, ViewModelTypeLockFree, sizeof(SpinModel));
    view_set_draw_callback(app->view_spinning, spinning_draw_callback);
    view_set_input_callback(app->view_spinning, spinning_input_callback);
    view_set_context(app->view_spinning, app);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdSpinning, app->view_spinning);

    /* ── Result view ── */
    app->view_result = view_alloc();
    view_allocate_model(app->view_result, ViewModelTypeLockFree, sizeof(ResultModel));
    view_set_draw_callback(app->view_result, result_draw_callback);
    view_set_input_callback(app->view_result, result_input_callback);
    view_set_context(app->view_result, app);
    view_dispatcher_add_view(app->view_dispatcher, ViewIdResult, app->view_result);

    /* ── Spin timer (created stopped) ── */
    app->spin_timer = furi_timer_alloc(spin_timer_callback, FuriTimerTypePeriodic, app);

    /* ── Start on the manage screen ── */
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewIdManage);

    return app;
}

/**
 * @brief Frees all resources in the correct tear-down order.
 *
 * Order requirements:
 *   1. Stop timer before freeing it.
 *   2. Remove views from dispatcher before freeing the views.
 *   3. Free TextInput before its view is freed by view_dispatcher_remove_view.
 *   4. Close the GUI record after detaching.
 */
static void app_free(App* app) {
    /* Stop timer first so no more custom events are sent. */
    furi_timer_stop(app->spin_timer);
    furi_timer_free(app->spin_timer);

    /* Remove views from dispatcher (does not free them). */
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdManage);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdSpinning);
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdResult);

    /* Free each view. */
    view_free(app->view_manage);
    text_input_free(app->text_input); /* also frees its internal View */
    view_free(app->view_spinning);
    view_free(app->view_result);

    view_dispatcher_free(app->view_dispatcher);

    /* Close GUI record (opened in app_alloc). */
    furi_record_close(RECORD_GUI);

    free(app);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 14.  Entry point
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Application entry point — referenced in application.fam.
 *
 * Allocates resources, runs the ViewDispatcher event loop (blocking until the
 * user exits), then cleans up and returns 0.
 */
int32_t random_decision_maker_app(void* p) {
    UNUSED(p);

    App* app = app_alloc();

    /* Blocks here until view_dispatcher_stop() is called (BACK on manage screen). */
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}

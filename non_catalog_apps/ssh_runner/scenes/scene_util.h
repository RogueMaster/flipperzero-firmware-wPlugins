/* Helpers shared by the scenes. SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include "../ssh_runner_i.h"
#include "../core/fontw.h"

/* Submenu / VariableItemList / Widget callbacks that forward a custom event. */
#define EVT_LONG 0x10000u /* OR-ed into a submenu index on long-press OK */

void util_submenu_cb(void* context, uint32_t index);
void util_submenu_ex_cb(void* context, InputType input_type, uint32_t index);
void util_var_list_enter_cb(void* context, uint32_t index);
void util_widget_button_cb(GuiButtonType result, InputType type, void* context);
/* TextView buttons: Left/Right/OK -> EVT_DIALOG_LEFT/RIGHT/CENTER */
void util_text_view_cb(void* context, InputKey key);
void util_popup_done_cb(void* context);

/* Start the keyboard scene for `target`; `initial` prefills the field (may be NULL). */
void util_start_input(SshRunnerApp* app, InputTarget target, const char* initial);
/* One scene's texts while it is on screen (list rows, a card, a status body), shared by the
 * scenes that show one: each fills it in on_enter (and on its own events) and resets its view
 * on exit, and only one scene is on screen at a time. 320 B instead of ~1.3 KB of static
 * buffers, which the release build needs at its start under RPC. */
#define UTIL_SCRATCH_SIZE 320
extern char util_scratch[UTIL_SCRATCH_SIZE];
/* The keyboard limit of each field (scene_input.c). */
size_t input_max_length(InputTarget t);

/* "alias" or "user@host" for headers */
void util_profile_label(const HostProfile* p, char* buf, size_t size);
/* The board's firmware version for people: "fes 0.1.0" -> "ESP32 fw 0.1.0" */
void util_board_version(const char* version, char* buf, size_t size);
/* Truncate in place to `max` characters, appending ".." */
void util_truncate(char* s, size_t max);
/* List values: Unleashed's stock VariableItemList clips a value to 37 px and scrolls the
 * selected row, so the value goes in whole. OFW 1.5.1 has no scrolling and draws a long
 * value over the arrows: there, keep as much of the tail as fits ("...8.1.10"). */
#define VALUE_PX 37
void util_fit_value(char* s, size_t size);
/* A list row for a long value (host, address): "Host   192.168.1.10" as the label with no
 * value, which the stock list draws whole (a value column is 37 px and would marquee), the
 * value padded flush right to LABEL_VALUE_RIGHT_PX (the stock '>' ends at x 118; labels start
 * at x 6). Falls back to label + value column when the pair does not fit LABEL_VALUE_PX or the
 * value is empty. */
#define LABEL_VALUE_PX       116
#define LABEL_VALUE_RIGHT_PX 112
VariableItem* util_label_value(
    VariableItemList* vl,
    char* buf,
    size_t size,
    const char* label,
    const char* value,
    void* ctx);
/* The label text of such a row: false (buf holds "label value" unpadded) when it does not fit.
 * `size` 64 holds the padding for any label and address. */
bool util_label_flush(char* buf, size_t size, const char* label, const char* value);

/* Submenu items with labels pre-fitted to the stock 111 px, so the module never cuts them
 * itself (it would leave "restart ..."). A longer label scrolls while its row is selected:
 * util_submenu_tick() on every app tick, util_submenu_reset() in place of submenu_reset(). */
#define SUBMENU_LABEL_PX 111
/* A submenu row "label   value" with the value flush right (a status, an address); at least
 * SUBMENU_GAP_PX between the two. */
#define SUBMENU_GAP_PX   8
void util_submenu_flush(char* buf, size_t size, const char* label, const char* value);
void util_submenu_add(
    Submenu* sm,
    const char* label,
    uint32_t index,
    SubmenuItemCallback cb,
    void* ctx);
void util_submenu_reset(Submenu* sm);
void util_submenu_tick(void);
void util_submenu_add_ex(
    Submenu* sm,
    const char* label,
    uint32_t index,
    SubmenuItemCallbackEx cb,
    void* ctx);

/* One layout for every confirm, result and error screen: a title (with the warning sign for
 * errors), left-aligned body text in FontSecondary and/or a FontKeyboard block for commands,
 * paths, addresses and keys, and stock buttons (events EVT_DIALOG_LEFT/CENTER/RIGHT).
 * Either text may be NULL; mono_first puts the FontKeyboard block above the body. */
void util_text_screen(
    SshRunnerApp* app,
    const char* header,
    bool warn,
    const char* body,
    const char* mono,
    bool mono_first,
    const char* left,
    const char* center,
    const char* right);

/* One layout for every progress screen: title (what; in FontKeyboard when title_mono or
 * when it is technical text, e.g. the target being connected to), an icon centred at y 18, the status
 * with the seconds since start, and a hint line ("Back cancels", may be NULL). The scene
 * gets every tick while it is up; it calls util_progress_tick() on EvtChanged and
 * util_progress_stop() on exit. util_progress_set() changes icon and/or status (NULL keeps). */
void util_progress_start(
    SshRunnerApp* app,
    const char* title,
    bool title_mono,
    const Icon* icon,
    const char* status,
    const char* hint);
void util_progress_set(SshRunnerApp* app, const Icon* icon, const char* status);
/* The busy spinner: pass UTIL_SPINNER as the icon and util_progress_tick() turns it, one step
 * per UTIL_SPINNER_MS (a turn a second). */
#define UTIL_SPINNER_FRAMES 8
#define UTIL_SPINNER_MS     125
extern const Icon* const util_spinner[UTIL_SPINNER_FRAMES];
#define UTIL_SPINNER     (util_spinner[0])
/* The status-line form (a running command's footer): six braille-like dots, as terminal
 * spinners, one step per UTIL_BUSY_MS. */
#define UTIL_BUSY_FRAMES 6
#define UTIL_BUSY_MS     100
extern const Icon* const util_busy[UTIL_BUSY_FRAMES];
void util_progress_tick(SshRunnerApp* app);
void util_progress_stop(SshRunnerApp* app);

/* Dialog result events */
#define EVT_DIALOG_LEFT   (EvtBase + 100)
#define EVT_DIALOG_CENTER (EvtBase + 101)
#define EVT_DIALOG_RIGHT  (EvtBase + 102)

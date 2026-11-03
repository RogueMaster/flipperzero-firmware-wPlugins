/* SSH Runner — application state shared by all scenes. SPDX-License-Identifier: GPL-3.0-only */
#pragma once

/* snprintf truncation into fixed UI buffers is intentional throughout the app. */
#pragma GCC diagnostic ignored "-Wformat-truncation"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include "views/textview.h"
#include <gui/modules/widget.h>
#include <gui/modules/loading.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

#include "core/backend.h"
#include "core/keydir.h"
#include "core/profile.h"
#include "core/scanlist.h"
#include "core/session.h"
#include "core/textproc.h"
#include "views/home_view.h"
#include "views/kbd.h"
#include "views/progress_view.h"
#include "views/wifi_list_view.h"
#include "scenes/ssh_runner_scene.h"

#define SSH_RUNNER_VERSION "0.3"
/* as fap_author and fap_weburl in application.fam */
#define SSH_RUNNER_AUTHOR  "SeanChangX"
#define SSH_RUNNER_WEB     "scx.tw/links"
/* The release build has no mock backend (application.fam); the demo build and the host
 * tests have it. */
#ifdef SSH_RUNNER_NO_MOCK
#define APP_HAS_MOCK 0
#else
#define APP_HAS_MOCK 1
#endif
#define TAG "SshRunner"

#define DATA_DIR          EXT_PATH("apps_data/ssh_runner")
#define HOSTS_DIR         DATA_DIR "/hosts"
#define SCRIPTS_DIR       DATA_DIR "/scripts"
#define LOGS_DIR          DATA_DIR "/logs"
#define RECENT_FILE       DATA_DIR "/recent.conf"
#define COMMANDS_FILE     DATA_DIR "/commands.conf"
#define SETTINGS_FILE     DATA_DIR "/settings.conf"
#define KEYS_DIR          DATA_DIR "/keys" /* SSH keys as files, like ~/.ssh */
/* A public key line lives in the tail of file_buf, behind a key file's head (the scan
 * reads both at once): up to RSA 4096 with a comment. */
#define KEY_PUB_LINE_MAX  1023
#define KEY_LINE_BUF(app) ((app)->file_buf + KEYFMT_SNIFF_BYTES)

#define MAX_HOSTS        32
#define INPUT_MAX        160
#define FILE_BUF_SIZE    3072
#define TICK_MS          20
#define TICK_RUN_MS      5 /* while a command runs: the link's turnaround (ssh_runner.c) */
#define OUTPUT_REDRAW_MS 150
/* FontKeyboard characters a TextView title shows whole (124 px / 6 px) */
#define TITLE_MONO_CHARS 20

typedef enum {
    ViewHome,
    ViewSubmenu,
    ViewVarList,
    ViewDialog,
    ViewProgress,
    ViewText,
    ViewWidget,
    ViewLoading,
    ViewKbd,
    ViewWifiList,
} AppView;

/* Custom events. Scene-local events stay below EvtBase. */
typedef enum {
    EvtBase = 1000,
    EvtChanged, /* app->chg has new bits */
    EvtInputDone, /* keyboard confirmed */
    EvtPopupDone, /* popup timeout */
} AppEvent;

/* Host editor item of the Auth row (scene state of HostEdit) */
#define HOST_EDIT_AUTH_ROW 4
#define HOST_EDIT_CONNECT  11 /* the host page opened on Connect (scene_hosts.c EditConnect) */

/* Change bits beyond the SESSION_CHG_* ones */
#define APP_CHG_SCAN      (1u << 16)
#define APP_CHG_PING      (1u << 17)
#define PING_VIA_ALL      0xFF
#define PING_DEFAULT_HOST "8.8.8.8" /* public DNS: the usual reachability check */
#define APP_CHG_KEY       (1u << 18)
#define APP_CHG_HOSTKEYS  (1u << 19)
#define APP_CHG_OPDONE    (1u << 20)
#define APP_CHG_ERROR     (1u << 21)
#define APP_CHG_SEC       (1u << 22) /* SEC_STATUS or SEC_STATE_EVT */

typedef enum {
    InputQuickConnect,
    InputPassword,
    InputCommand,
    InputAlias,
    InputUser,
    InputHost,
    InputPort,
    InputHostCmd,
    InputSaveAlias,
    InputPingHost,
    InputWifiPassword,
    InputIp,
    InputMask,
    InputGateway,
    InputDns,
    InputPassphrase, /* the staged key's passphrase at connect */
    InputKeyName, /* Generate key: file name */
    InputKeyPass1, /* Generate key: passphrase, then again */
    InputKeyPass2,
    InputKeyRename,
    InputSecWord, /* Protect secrets: type "confirm" */
} InputTarget;

typedef enum {
    ConfirmDeleteHost,
    ConfirmDeleteCmd,
    ConfirmDeleteHostkey,
    ConfirmReplaceKeys,
    ConfirmDeleteKey,
    ConfirmRememberPassphrase, /* at connect: Once / Remember (default) */
    ConfirmRemoveRecent,
    ConfirmDiscardEdit,
    ConfirmRunScript,
    ConfirmProtectSecrets, /* Protect secrets warning, Cancel left */
} ConfirmAction;

typedef struct {
    bool mock;
    char default_key[KEY_NAME_MAX + 1]; /* DefaultKey: in settings.conf, "" = none */
    bool remember_notice_seen; /* the one-time "stored unprotected" note was shown */
    bool examples_seeded; /* the built-in examples were offered once (first run) */
} AppSettings;

/* What an incoming key file / public key belongs to. */
typedef enum {
    KeyJobNone,
    KeyJobGenerate, /* Keys -> Generate key */
    KeyJobConnectPub, /* a key without .pub: its public key after the first parse */
} KeyJob;

typedef struct SshRunnerApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Storage* storage;

    HomeView* home;
    Submenu* submenu;
    VariableItemList* var_list;
    ProgressView* progress;
    TextView* text_view;
    Widget* widget;
    Loading* loading;
    SshKbd* kbd;
    WifiListView* wifi_list; /* allocated on the first WiFi list, kept until the app exits */

    FuriTimer* tick_timer;
    volatile bool tick_pending;
    bool tick_fast; /* TICK_RUN_MS while a command runs */
    Backend backend;
    Session* session;
    uint32_t chg; /* change bits accumulated since the last tick dispatch */
    uint32_t cur_chg; /* bits being dispatched to the active scene right now */
    bool blinking;
    bool want_ticks; /* the scene gets EvtChanged on every tick, not only on changes */
    size_t heap_at_start; /* system free heap when the app started (About) */

    AppSettings settings;
    RecentList recent;
    CommandList* commands; /* 2 KB: loaded by the run menu, freed when the output opens */

    /* Current host: being connected to, or being edited. */
    HostProfile profile;
    bool profile_saved; /* exists on SD under profile.alias */
    char profile_orig_alias[PROFILE_ALIAS_MAX + 1];
    bool profile_dirty;
    bool quick_connect; /* unsaved target from Quick Connect / Recent */
    uint32_t return_scene; /* where to go when the session ends */
    bool offered_save; /* "Save as alias?" already offered this session */

    /* Hosts list */
    uint8_t host_count;
    char host_aliases[MAX_HOSTS][PROFILE_ALIAS_MAX + 1];

    /* Keyboard */
    InputTarget input_target;
    char input[INPUT_MAX];

    /* Confirm dialog */
    ConfirmAction confirm_action;
    uint32_t confirm_index;

    /* Command detail (long-press a command): which one, and from where */
    uint8_t cmd_index;
    bool cmd_global; /* commands.conf: run only */
    bool cmd_in_session; /* from the run menu: Run, and edits are saved at once */
    int16_t cmd_edit_index; /* InputHostCmd: -1 adds a command, else replaces this one */

    /* Run / output */
    char run_title[INPUT_MAX];
    bool run_is_script;
    bool have_output;
    FuriString* output_text;
    uint32_t output_rendered_at;
    bool output_dirty;
    char output_footer[48]; /* the footer shown, for the spinner's steps */
    const Icon* output_icon;
    /* The Output screen shows the session's OutBuf in place (no copy): these are what its
     * TextView blocks point at besides it, valid while output_bound */
    bool output_bound;
    char output_title[INPUT_MAX + 4];
    OutbufParts output_parts;
    File* script_file;
    char script_name[72];
    uint32_t script_size;
    char script_interp[16];

    /* Network / keys */
    uint8_t net_iface; /* ProtoIface being edited */
    uint8_t mock_scenario; /* MockScenario, Developer menu (mock only) */
    ProtoNetCfg net_cfg;
    ScanList* scan; /* the last WiFi scan; allocated on the first one (~600 B), freed at exit */
    char wifi_ssid[PROTO_STR_SSID + 1];
    ProtoWifiJoinResult
        join; /* how the last join ended, when join_known (a board that sends WIFI_JOIN_RESULT) */
    bool join_known;
    ProtoPingResp ping;
    bool ping_done;
    uint8_t ping_via; /* PING_VIA_ALL, or the one ProtoIface to ping through */
    uint8_t ping_todo; /* interfaces still to ping (bit = ProtoIface) */
    uint8_t ping_have; /* results in ping_res (bit = ProtoIface) */
    ProtoPingResp ping_res[2];
    ProtoHostkeyList hostkeys;
    bool hostkeys_loaded;
    BackendError last_error;

    /* Keys */
    /* keys/ as last scanned (~1.1 KB, keys_load/keys_unload around the screens that
     * need it: RAM is tight once the FAP is loaded) */
    KeyList* keys;
    char key_name[KEY_NAME_MAX + 1]; /* the key being shown, generated or acted on */
    KeyJob key_job;
    uint8_t gen_type; /* ProtoKeyGenType */
    char gen_pass[PROTO_STR_PASSPHRASE + 1]; /* wiped once sent */
    bool key_blob_ok; /* the job's private key file is on the card, verified */
    uint32_t key_blob_crc;
    bool key_job_failed;
    char key_job_msg[PROTO_STR_MSG + 1];
    char key_fp[PROTO_STR_FP + 1]; /* fingerprint of the job's key */
    /* key file / public key line from the ESP32, copied in the backend callback and
     * written to the card after the tick (shallow stack, no SD work inside the link) */
    uint8_t* key_in;
    uint32_t key_in_len;
    char* key_in_line;
    BackendKeyPub key_in_pub; /* .line points at key_in_line */
    bool key_in_pub_due;
    File* key_file; /* streamed to the ESP32 at connect */
    bool key_failed; /* the connect failed on the key file itself (missing, unusable) */

    /* Popup message */
    char msg_header[32];
    char msg_text[96];
    char msg_mono[72]; /* FontKeyboard part: a path, name or address (may be empty) */
    bool msg_warn; /* an error: the title carries the warning sign */
    uint32_t msg_next_scene; /* 0 = previous scene */

    char* file_buf; /* FILE_BUF_SIZE bytes while acquired (file_buf_acquire), else NULL */
    uint8_t file_buf_depth;

    /* Settings > Security */
    ProtoSecStatus sec;
    bool sec_known; /* `sec` answered since the screen asked */
    ProtoSecState sec_state; /* last SEC_STATE_EVT */
    bool sec_relinked; /* the link came back during Protect secrets */
    uint32_t sec_restart_at; /* tick of the restart phase, 0 before */
} SshRunnerApp;

/* ssh_runner.c */
void app_backend_restart(SshRunnerApp* app);
void app_notify_success(SshRunnerApp* app);
void app_notify_error(SshRunnerApp* app);
void app_blink_start(SshRunnerApp* app);
void app_blink_stop(SshRunnerApp* app);
/* Change bits for the EvtChanged currently being dispatched (0 otherwise). */
uint32_t app_take_changes(SshRunnerApp* app);
/* End the SSH session (if any) and return to app->return_scene. */
void app_end_session(SshRunnerApp* app);
void app_fill_home_status(SshRunnerApp* app, HomeStatus* st);
/* Popup helper: show a message, then go back (or to `next_scene` if nonzero). */
void app_show_message(SshRunnerApp* app, const char* header, const char* text, uint32_t next_scene);
/* The same with a FontKeyboard part under the text (path, name, address). */
void app_show_message_mono(
    SshRunnerApp* app,
    const char* header,
    const char* text,
    const char* mono,
    uint32_t next_scene);
/* An error: the title carries the warning sign. */
void app_show_error(SshRunnerApp* app, const char* header, const char* text, uint32_t next_scene);
void app_output_footer(SshRunnerApp* app, char* footer, size_t footer_size);
/* Points the Output TextView at the current output; with the TextView locked. */
void output_bind(SshRunnerApp* app);

/* store.c — SD card */
/* file_buf exists only while in use: 3 KB off the heap the rest of the time. Nested users
 * share it; the outermost release wipes it (key file heads pass through) and frees it. */
void file_buf_acquire(SshRunnerApp* app);
void file_buf_release(SshRunnerApp* app);
/* The built-in examples (files/examples/ in the FAP): each copied only where the user has no
 * file yet. first_run: commands.conf only if missing; else (Settings > Restore Examples) the
 * example commands it lacks are appended to it. */
typedef struct {
    bool available; /* the FAP's examples are installed */
    unsigned commands; /* added */
    unsigned scripts;
    unsigned no_room; /* commands not added: commands.conf is full */
} ExamplesResult;
ExamplesResult store_examples(SshRunnerApp* app, bool first_run);
/* scene_run.c: the global saved commands, on demand */
void commands_load(SshRunnerApp* app);
void commands_unload(SshRunnerApp* app);
/* scene_security.c: SEC_ENABLE with the typed word in app->input, then the progress screen */
void sec_enable_start(SshRunnerApp* app);
void store_init(SshRunnerApp* app);
bool store_load_settings(SshRunnerApp* app);
bool store_save_settings(SshRunnerApp* app);
void store_load_hosts(SshRunnerApp* app);
bool store_load_profile(SshRunnerApp* app, const char* alias, HostProfile* out);
bool store_save_profile(SshRunnerApp* app, const HostProfile* p);
bool store_delete_profile(SshRunnerApp* app, const char* alias);
bool store_profile_exists(SshRunnerApp* app, const char* alias);
void store_load_recent(SshRunnerApp* app);
bool store_save_recent(SshRunnerApp* app);
void store_load_commands(SshRunnerApp* app);
bool store_save_output(SshRunnerApp* app, char* path_out, size_t path_size);
bool store_keys_scan(SshRunnerApp* app, KeyList* out);
/* Scan keys/ into app->keys, allocating it if needed. */
void keys_load(SshRunnerApp* app);
void keys_unload(SshRunnerApp* app);
bool store_key_exists(SshRunnerApp* app, const char* name);
File* store_key_open(SshRunnerApp* app, const char* name, uint32_t* size);
/* Writes keys/<name> (never over an existing file) and checks it reads back with `crc32`. */
bool store_key_write(
    SshRunnerApp* app,
    const char* name,
    const uint8_t* data,
    uint32_t len,
    uint32_t crc32);
bool store_key_write_pub(SshRunnerApp* app, const char* name, const char* line);
size_t store_key_read_pub(SshRunnerApp* app, const char* name, char* out, size_t size);
size_t store_key_derive_pub(SshRunnerApp* app, const char* name, char* out, size_t size);
bool store_key_file_id(SshRunnerApp* app, const char* name, char* out, size_t size);
unsigned store_key_users(
    SshRunnerApp* app,
    const char* name,
    char (*aliases)[PROFILE_ALIAS_MAX + 1],
    unsigned max);
bool store_key_rename(SshRunnerApp* app, const char* old_name, const char* new_name);
bool store_key_delete(SshRunnerApp* app, const char* name, unsigned* switched, uint8_t* to);

/* scene_keys.c — key jobs: the backend callback only copies (keys_on_*), the work
 * happens in keys_process() after the backend tick */
void keys_on_blob(SshRunnerApp* app, const BackendKeyBlob* b);
void keys_on_pub(SshRunnerApp* app, const BackendKeyPub* k);
/* Returns APP_CHG_KEY when a job step finished. */
uint32_t keys_process(SshRunnerApp* app);
/* The key entry a host connects with: scans keys/; NULL with `*missing` set if absent. */
const KeyEntry* keys_for_profile(SshRunnerApp* app, const HostProfile* p, bool* missing);

/* backend_uart.c */
Backend uart_backend_create(BackendEventCallback cb, void* ctx);

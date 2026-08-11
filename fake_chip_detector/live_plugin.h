#pragma once

#include "live_test.h"

// Live tests loaded from the SD card.
//
// A test compiled as a .fal and dropped into the tests folder behaves exactly
// like one built into the app. That is the whole point of the LiveTestEnv
// contract: the plugin never links against a symbol of ours, so there is no
// second version of a test to keep in step with the first.
//
// Memory is the constraint that shapes this file. Reading a plugin's name
// means loading its ELF, and a user may have a folder full of them, so nothing
// here ever holds more than one plugin mapped at a time. Listing copies the
// metadata out and unloads immediately; running loads again and keeps that one
// alive only while the test is on screen.

// Where a user drops tests: /ext/apps_data/fake_chip_detector/tests. Created
// on first run so the folder is there to be found, rather than the user being
// told to make a directory whose name they have to guess.
#define LIVE_PLUGIN_DIR_NAME "tests"
#define LIVE_PLUGIN_EXT ".fal"

// A folder with more than this in it is a folder nobody is scrolling through.
// The count of what was skipped is reported rather than silently dropped.
#define LIVE_PLUGIN_MAX 24

#define LIVE_PLUGIN_FILE_LEN 32
#define LIVE_PLUGIN_CHIP_LEN 20
#define LIVE_PLUGIN_TITLE_LEN 24
#define LIVE_PLUGIN_PROBLEM_LEN 28

// Why a file in the tests folder cannot be run. Kept as a code rather than a
// message so the screen can phrase it, and so "it is broken" is never confused
// with "it is fine but for a different chip".
typedef enum {
    LivePluginOk,
    LivePluginNotAPlugin, // a .fal, but not built as a plugin
    LivePluginWrongApp, // someone else's plugin, sharing the folder
    LivePluginWrongVersion, // ours, but built against a different contract
    LivePluginBadFile, // will not load: corrupt, or built for another target
    LivePluginNoAddrs, // loads, but declares no address to probe
    LivePluginBadStrings, // its name or offer never ends inside the room for it
} LivePluginStatus;

// Everything known about one file in the tests folder. All strings are copies:
// the originals live inside an ELF that has already been unmapped by the time
// anything reads this.
typedef struct {
    char file[LIVE_PLUGIN_FILE_LEN]; // as it appears in the folder
    char chip[LIVE_PLUGIN_CHIP_LEN];
    char title[LIVE_PLUGIN_TITLE_LEN];
    char offer[LIVE_TEST_LINE_LEN];
    uint8_t addrs[LIVE_TEST_MAX_ADDRS];
    LivePluginStatus status;
} LivePluginInfo;

typedef struct {
    LivePluginInfo items[LIVE_PLUGIN_MAX];
    uint8_t count;
    uint8_t skipped; // files past LIVE_PLUGIN_MAX
    bool folder_ready; // the folder exists (it is created if it did not)
} LivePluginList;

// Scans the tests folder, loading and unloading each plugin in turn to read
// its descriptor. Takes a moment per file, so call it when a screen opens, not
// on every draw. Never partially fills `out`: it is cleared first.
void live_plugin_list(LivePluginList* out);

// One line of plain English for a status, for the browser to show.
const char* live_plugin_status_text(LivePluginStatus status);

// A plugin held open. The LiveTest inside points into mapped memory, so the
// handle has to outlive every use of it — including the worker thread.
typedef struct LivePluginHandle LivePluginHandle;

// Loads one file from the tests folder by name. NULL if it will not load; the
// reason lands in *status when status is non-NULL.
LivePluginHandle* live_plugin_open(const char* file_name, LivePluginStatus* status);

// Valid only until live_plugin_close. Never NULL for a handle that opened.
const LiveTest* live_plugin_test(LivePluginHandle* handle);

// Unmaps the plugin. Every pointer obtained from it dies here, so the test
// thread must be joined first.
void live_plugin_close(LivePluginHandle* handle);

#include "live_plugin.h"

#include <furi.h>
#include <storage/storage.h>
#include <flipper_application/flipper_application.h>
#include <loader/firmware_api/firmware_api.h>

#include <string.h>

struct LivePluginHandle {
    Storage* storage;
    FlipperApplication* app;
    const LiveTest* test;
};

// Resolves /data/tests to its real place on the card and makes sure both it
// and the app folder above it exist. Creating it matters: an empty folder that
// is present can be found, photographed and dropped into; a folder that does
// not exist is a path the user has to type correctly from memory.
static void live_plugin_dir(Storage* storage, FuriString* out) {
    furi_string_set(out, STORAGE_APP_DATA_PATH_PREFIX "/" LIVE_PLUGIN_DIR_NAME);
    storage_common_resolve_path_and_ensure_app_directory(storage, out);
    storage_simply_mkdir(storage, furi_string_get_cstr(out));
}

static bool live_plugin_has_ext(const char* name) {
    size_t len = strlen(name);
    size_t ext = strlen(LIVE_PLUGIN_EXT);
    if(len <= ext) return false;
    // Case-insensitively, because the card is FAT and a file copied from a
    // desktop may well arrive shouting.
    for(size_t i = 0; i < ext; i++) {
        char a = name[len - ext + i];
        char b = LIVE_PLUGIN_EXT[i];
        if(a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if(a != b) return false;
    }
    return true;
}

// True if `s` reaches a terminator within `max` bytes, so a bounded copy of it
// is safe. The strings in a descriptor belong to somebody else's file, and the
// only thing standing between a missing terminator and a walk off the end of
// the mapped image is a length nobody has checked — a listing screen that
// hard-faults every time it opens leaves no way to delete the offending file
// from the app. This bounds the read; it cannot rescue a pointer that was never
// valid, which would fault on its first byte and needs image bounds the loader
// does not expose.
static bool live_plugin_str_ok(const char* s, size_t max) {
    for(size_t i = 0; i < max; i++) {
        if(!s[i]) return true;
    }
    return false;
}

// Loads one plugin far enough to have its descriptor in hand. On success the
// caller owns *app and must free it; on failure nothing is left mapped.
static LivePluginStatus
    live_plugin_map(Storage* storage, const char* path, FlipperApplication** out_app, const LiveTest** out_test) {
    *out_app = NULL;
    *out_test = NULL;

    FlipperApplication* app = flipper_application_alloc(storage, firmware_api_interface);
    LivePluginStatus status = LivePluginBadFile;

    do {
        if(flipper_application_preload(app, path) != FlipperApplicationPreloadStatusSuccess) break;
        if(flipper_application_map_to_memory(app) != FlipperApplicationLoadStatusSuccess) break;

        if(!flipper_application_is_plugin(app)) {
            status = LivePluginNotAPlugin;
            break;
        }

        const FlipperAppPluginDescriptor* descriptor =
            flipper_application_plugin_get_descriptor(app);
        if(!descriptor || !descriptor->appid || !descriptor->entry_point) break;

        // Someone else's plugin may perfectly reasonably be sitting in the
        // same folder. Not an error worth shouting about, but definitely not
        // something to call and hand a bus to.
        if(strcmp(descriptor->appid, LIVE_TEST_PLUGIN_APPID) != 0) {
            status = LivePluginWrongApp;
            break;
        }

        // The gate that stops a test compiled against an older LiveTestState
        // from writing past the end of the struct this app allocates — and,
        // worse, from reading rubbish out of it and publishing it as a
        // measurement.
        if(descriptor->ep_api_version != LIVE_TEST_PLUGIN_API_VERSION) {
            status = LivePluginWrongVersion;
            break;
        }

        const LiveTest* test = descriptor->entry_point;
        if(!test->chip || !test->title || !test->offer || !test->run) break;
        if(!live_plugin_str_ok(test->chip, LIVE_PLUGIN_CHIP_LEN) ||
           !live_plugin_str_ok(test->title, LIVE_PLUGIN_TITLE_LEN) ||
           !live_plugin_str_ok(test->offer, LIVE_TEST_LINE_LEN)) {
            status = LivePluginBadStrings;
            break;
        }
        if(test->addrs[0] == LIVE_TEST_ADDR_NONE) {
            // Without an address there is nothing to probe, and guessing one
            // would mean writing configuration registers to whatever answers.
            status = LivePluginNoAddrs;
            break;
        }

        *out_app = app;
        *out_test = test;
        return LivePluginOk;
    } while(false);

    flipper_application_free(app);
    return status;
}

const char* live_plugin_status_text(LivePluginStatus status) {
    switch(status) {
    case LivePluginOk:
        return "Ready";
    case LivePluginNotAPlugin:
        return "Not built as a plugin";
    case LivePluginWrongApp:
        return "For a different app";
    case LivePluginWrongVersion:
        return "Built for another version";
    case LivePluginBadFile:
        return "Will not load";
    case LivePluginNoAddrs:
        return "Declares no address";
    case LivePluginBadStrings:
        return "Its own name is damaged";
    default:
        return "Unknown";
    }
}

void live_plugin_list(LivePluginList* out) {
    memset(out, 0, sizeof(*out));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* dir = furi_string_alloc();
    live_plugin_dir(storage, dir);

    File* handle = storage_file_alloc(storage);
    if(storage_dir_open(handle, furi_string_get_cstr(dir))) {
        out->folder_ready = true;

        FileInfo info;
        char name[LIVE_PLUGIN_FILE_LEN];
        FuriString* path = furi_string_alloc();

        while(storage_dir_read(handle, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!live_plugin_has_ext(name)) continue;

            if(out->count >= LIVE_PLUGIN_MAX) {
                out->skipped++;
                continue;
            }

            LivePluginInfo* item = &out->items[out->count];
            strlcpy(item->file, name, sizeof(item->file));

            furi_string_printf(path, "%s/%s", furi_string_get_cstr(dir), name);

            // One at a time, mapped and unmapped before the next is touched.
            // A folder of twenty tests must cost no more memory than a folder
            // of one, because the user with twenty is exactly the user whose
            // Flipper is already short of heap.
            FlipperApplication* loaded = NULL;
            const LiveTest* test = NULL;
            item->status =
                live_plugin_map(storage, furi_string_get_cstr(path), &loaded, &test);

            if(item->status == LivePluginOk) {
                strlcpy(item->chip, test->chip, sizeof(item->chip));
                strlcpy(item->title, test->title, sizeof(item->title));
                strlcpy(item->offer, test->offer, sizeof(item->offer));
                memcpy(item->addrs, test->addrs, sizeof(item->addrs));
                flipper_application_free(loaded);
            }

            out->count++;
        }

        furi_string_free(path);
    }
    storage_dir_close(handle);
    storage_file_free(handle);

    furi_string_free(dir);
    furi_record_close(RECORD_STORAGE);
}

LivePluginHandle* live_plugin_open(const char* file_name, LivePluginStatus* status) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* path = furi_string_alloc();
    live_plugin_dir(storage, path);
    furi_string_cat_printf(path, "/%s", file_name);

    FlipperApplication* app = NULL;
    const LiveTest* test = NULL;
    LivePluginStatus result = live_plugin_map(storage, furi_string_get_cstr(path), &app, &test);
    furi_string_free(path);

    if(status) *status = result;

    if(result != LivePluginOk) {
        furi_record_close(RECORD_STORAGE);
        return NULL;
    }

    // The storage record stays open for as long as the plugin is mapped: the
    // loader holds on to it, and closing it out from under a running test is
    // not something to find out about at a pickup counter.
    LivePluginHandle* handle = malloc(sizeof(LivePluginHandle));
    handle->storage = storage;
    handle->app = app;
    handle->test = test;
    return handle;
}

const LiveTest* live_plugin_test(LivePluginHandle* handle) {
    return handle ? handle->test : NULL;
}

void live_plugin_close(LivePluginHandle* handle) {
    if(!handle) return;
    flipper_application_free(handle->app);
    furi_record_close(RECORD_STORAGE);
    free(handle);
}

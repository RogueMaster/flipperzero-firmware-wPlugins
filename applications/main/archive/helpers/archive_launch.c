#include "archive_launch.h"
#include "archive_helpers_ext.h"
#include <loader/loader.h>

#ifndef APP_ARCHIVE_BROWSER
#include <desktop/desktop_i.h>
#endif

static const char* archive_get_flipper_app_name(ArchiveFileTypeEnum file_type) {
    switch(file_type) {
    case ArchiveFileTypeIButton:
        return "iButton";
    case ArchiveFileTypeNFC:
        return "NFC";
    case ArchiveFileTypeSubGhz:
        return "Sub-GHz";
    case ArchiveFileTypeLFRFID:
        return "125 kHz RFID";
    case ArchiveFileTypeInfrared:
        return "Infrared";
    case ArchiveFileTypeSubghzPlaylist:
        return EXT_PATH("apps/Sub-GHz/subghz_playlist.fap");
    case ArchiveFileTypeSubghzRemote:
        return EXT_PATH("apps/Sub-GHz/subghz_remote_refactored.fap");
    case ArchiveFileTypeProtoPirate:
        return EXT_PATH("apps/Sub-GHz/proto_pirate.fap");
    case ArchiveFileTypeInfraredRemote:
        return EXT_PATH("apps/Infrared/ir_remote.fap");
    case ArchiveFileTypeBadUsb:
        return "Bad KB";
    case ArchiveFileTypeMP3:
        return EXT_PATH("apps/Media/mp3_player.fap");
    case ArchiveFileTypeWAV:
        return EXT_PATH("apps/Media/wav_player.fap");
    case ArchiveFileTypeMag:
        return EXT_PATH("apps/GPIO/magspoof.fap");
    case ArchiveFileTypeCrossRemote:
        return EXT_PATH("apps/Infrared/cross_remote.fap");
    case ArchiveFileTypePicopass:
        return EXT_PATH("apps/NFC/picopass.fap");
    case ArchiveFileTypeU2f:
        return "U2F";
    case ArchiveFileTypeUpdateManifest:
        return "UpdaterApp";
    case ArchiveFileTypeDiskImage:
        return EXT_PATH("apps/USB/mass_storage.fap");
    case ArchiveFileTypeJS:
#ifdef JS_RUNNER_FAP
        return EXT_PATH("apps/Main/js_app.fap");
#else
        return "JS Runner";
#endif
    default:
        return NULL;
    }
}

static void archive_start(Loader* loader, const char* app, const char* args) {
#ifdef APP_ARCHIVE_BROWSER
    /* The loader copies both strings and waits for this FAP to exit. */
    loader_enqueue_launch(loader, app, args, LoaderDeferredLaunchFlagGui);
#else
    loader_start_detached_with_gui_error(loader, app, args);
#endif
}

static void archive_show_file(Loader* loader, const char* path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool text = true;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[1000];
        size_t read = storage_file_read(file, buf, sizeof(buf));
        for(size_t i = 0; i < read; i++) {
            const char c = buf[i];
            if((c < ' ' || c > '~') && c != '\r' && c != '\n') {
                text = false;
                break;
            }
        }
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    archive_start(
        loader,
        text ? EXT_PATH("apps/Tools/text_viewer.fap") : EXT_PATH("apps/Tools/hex_viewer.fap"),
        path);
}

bool archive_launch_selected(const ArchiveFile_t* selected, bool favorites) {
    furi_assert(selected);
    /* Search is a browser action, and folders are navigated in the browser. */
    if(selected->type == ArchiveFileTypeSearch || selected->type == ArchiveFileTypeFolder) {
        return false;
    }

    Loader* loader = furi_record_open(RECORD_LOADER);
    const char* path = furi_string_get_cstr(selected->path);
    const char* app_name = archive_get_flipper_app_name(selected->type);

    if(selected->type == ArchiveFileTypeSetting) {
        const char* app = strchr(path + (path[0] == '/'), '/');
        if(!app || !app[1]) {
            furi_record_close(RECORD_LOADER);
            return false;
        }
        FuriString* name = furi_string_alloc_set_str(app + 1);
        size_t slash = furi_string_search_char(name, '/');
        FuriString* args = NULL;
        if(slash != FURI_STRING_FAILURE) {
            /* Copy the argument before truncating the application's name. */
            args = furi_string_alloc_set_str(furi_string_get_cstr(name) + slash + 1);
            furi_string_left(name, slash);
        }
        archive_start(
            loader, furi_string_get_cstr(name), args ? furi_string_get_cstr(args) : NULL);
        if(args) furi_string_free(args);
        furi_string_free(name);
    } else if(app_name) {
        if(selected->is_app) {
            const char* param = strrchr(path, '/');
            archive_start(loader, app_name, param ? param + 1 : NULL);
        } else if(
            favorites &&
            (selected->type == ArchiveFileTypeIButton || selected->type == ArchiveFileTypeLFRFID ||
             selected->type == ArchiveFileTypeNFC || selected->type == ArchiveFileTypeSubGhz)) {
            FuriString* args = furi_string_alloc_printf("fav%s", path);
            archive_start(loader, app_name, furi_string_get_cstr(args));
            furi_string_free(args);
        } else {
            archive_start(loader, app_name, path);
        }
    } else if(selected->type == ArchiveFileTypeApplication) {
        archive_start(loader, path, NULL);
    } else {
        archive_show_file(loader, path);
    }
    furi_record_close(RECORD_LOADER);
    return true;
}

#ifndef APP_ARCHIVE_BROWSER
/* Keep file/folder desktop keybind routing available without loading Archive. */
void run_with_default_app(const char* path) {
    furi_assert(path);
    FileInfo info;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool is_dir = storage_common_stat(storage, path, &info) == FSE_OK && file_info_is_dir(&info);
    furi_record_close(RECORD_STORAGE);

    if(is_dir) {
        Desktop* desktop = furi_record_open(RECORD_DESKTOP);
        desktop_launch_archive(desktop, path);
        furi_record_close(RECORD_DESKTOP);
    } else {
        ArchiveFile_t item;
        ArchiveFile_t_init(&item);
        furi_string_set(item.path, path);
        archive_set_file_type(&item, path, false, false);
        archive_launch_selected(&item, false);
        ArchiveFile_t_clear(&item);
    }
}
#endif

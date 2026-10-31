#include "flippass_launch_layout.h"
#include "flippass_launch_args.h"
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

static bool layout_equal(const char* text, const char* expected) {
    while(*text && *expected) {
        unsigned char a = (unsigned char)*text++, b = (unsigned char)*expected++;
        if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if(a != b) return false;
    }
    return *text == *expected;
}

FlipPassLaunchLayoutResult
    flippass_launch_layout_resolve(Storage* storage, const char* id, char* path, size_t capacity) {
    if(path == NULL || capacity == 0U) return FlipPassLaunchLayoutInvalidId;
    path[0] = '\0';
    if(!flippass_launch_layout_id_valid(id)) return FlipPassLaunchLayoutInvalidId;
    if(id == NULL || id[0] == '\0' || layout_equal(id, "Default"))
        return FlipPassLaunchLayoutDefault;
    if(layout_equal(id, "Alt+NumPad") || layout_equal(id, "alt-numpad"))
        return FlipPassLaunchLayoutOverride;
    const int bytes = snprintf(path, capacity, "/ext/badusb/assets/layouts/%s.kl", id);
    if(bytes < 0 || (size_t)bytes >= capacity) {
        path[0] = '\0';
        return FlipPassLaunchLayoutInvalidId;
    }
    if(storage == NULL) {
        path[0] = '\0';
        return FlipPassLaunchLayoutUnavailable;
    }
    FileInfo info;
    const FS_Error status = storage_common_stat(storage, path, &info);
    if(status != FSE_OK) {
        path[0] = '\0';
        return status == FSE_NOT_EXIST ? FlipPassLaunchLayoutNotFound :
                                         FlipPassLaunchLayoutUnavailable;
    }
    if((info.flags & FSF_DIRECTORY) || info.size != 256U) {
        path[0] = '\0';
        return FlipPassLaunchLayoutInvalidFile;
    }
    return FlipPassLaunchLayoutOverride;
}

const char* flippass_launch_layout_error(FlipPassLaunchLayoutResult result) {
    switch(result) {
    case FlipPassLaunchLayoutNotFound:
        return "The requested keyboard layout ID was not found.";
    case FlipPassLaunchLayoutInvalidFile:
        return "The keyboard layout must be a 256-byte .kl file.";
    case FlipPassLaunchLayoutUnavailable:
        return "Keyboard layout storage is unavailable.";
    default:
        return "The requested keyboard layout ID is invalid.";
    }
}

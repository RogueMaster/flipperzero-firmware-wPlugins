#include "dnd_handoff.h"

#include <furi.h>
#include <loader/loader.h>

bool dnd_handoff_launch(const char* fap_path, const char* args) {
    if(!fap_path || !fap_path[0]) return false;

    Loader* loader = furi_record_open(RECORD_LOADER);
    if(!loader) return false;

    loader_enqueue_launch(loader, fap_path, args, LoaderDeferredLaunchFlagGui);
    furi_record_close(RECORD_LOADER);
    return true;
}

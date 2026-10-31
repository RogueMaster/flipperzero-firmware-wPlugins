#pragma once
#include <furi.h>
#include <gui/canvas.h>
#include <stdlib.h>
#include <storage/storage.h>
#include "Platform.h"
#include "Config.h"

// Frame capture for the README clips and catalog screenshots, compiled in
// only when Config.h sets HAZKE_CAPTURE. Every `Every`-th frame the screen
// shows, menus included, goes to /ext/apps_data/hazke/frames.bin as a
// 4-byte tick followed by the 1 KB screen buffer. tools/frames.py turns
// the file into PNGs and GIFs.

namespace Capture {

#if HAZKE_CAPTURE

constexpr int Ring = 8;
constexpr uint32_t Every = 3;
constexpr size_t ScreenBytes = 1024;

struct Frame {
    uint32_t tick;
    uint8_t px[ScreenBytes];
};

inline Frame* ring = nullptr;
inline volatile uint32_t head = 0; // advanced by the GUI thread
inline volatile uint32_t tail = 0; // advanced by the game thread
inline uint32_t seen = 0;
inline bool active = false;
inline Storage* storage = nullptr;
inline File* file = nullptr;

inline void begin() {
    storage = (Storage*)furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    ring = (Frame*)malloc(sizeof(Frame) * Ring);
    file = storage_file_alloc(storage);
    if(!ring ||
       !storage_file_open(file, APP_DATA_PATH("frames.bin"), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        free(ring);
        file = nullptr;
        ring = nullptr;
        furi_record_close(RECORD_STORAGE);
        storage = nullptr;
        return;
    }
    active = true;
}

// GUI thread, after the frame is drawn. Drops the frame if the writer is
// behind rather than stall the screen.
inline void onDraw(Canvas* c) {
    if(!active) return;
    if(seen++ % Every) return;
    if(head - tail >= (uint32_t)Ring) return;
    const uint8_t* src = canvas_get_buffer(c);
    if(!src || canvas_get_buffer_size(c) != ScreenBytes) return;
    Frame& f = ring[head % Ring];
    f.tick = furi_get_tick();
    memcpy(f.px, src, ScreenBytes);
    head = head + 1;
}

// Game thread, outside the frame lock.
inline void flush() {
    while(active && tail != head) {
        storage_file_write(file, &ring[tail % Ring], sizeof(Frame));
        tail = tail + 1;
    }
}

inline void end() {
    if(!active) return;
    flush();
    active = false;
    storage_file_close(file);
    storage_file_free(file);
    free(ring);
    file = nullptr;
    ring = nullptr;
    furi_record_close(RECORD_STORAGE);
    storage = nullptr;
}

#else

// Release builds: nothing is recorded and none of the above is compiled.
inline void begin() {
}
inline void onDraw(Canvas*) {
}
inline void flush() {
}
inline void end() {
}

#endif

} // namespace Capture

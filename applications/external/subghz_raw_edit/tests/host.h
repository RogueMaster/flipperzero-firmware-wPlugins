#pragma once

/* What the editing code (load.c, work.c, edit_ops.c, view.c, sel_info.c) needs
 * from the firmware, on the host: files map to the disk under host_root, the
 * heap size is set by the tests, and drawing does nothing. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FW_ORIGIN_Official  // no custom buttons
#define UNUSED(x)           (void)(x)
#define COUNT_OF(x)         (sizeof(x) / sizeof((x)[0]))
#define FuriWaitForever     0xFFFFFFFFU
#define APP_DATA_PATH(path) "/data/" path

typedef struct FuriString FuriString;
typedef struct FuriMutex FuriMutex;
typedef struct FuriMessageQueue FuriMessageQueue;
typedef struct Gui Gui;
typedef struct ViewPort ViewPort;
typedef struct View View;
typedef struct ViewDispatcher ViewDispatcher;
typedef struct DialogsApp DialogsApp;
typedef struct Canvas Canvas;
typedef struct SubGhzEnvironment SubGhzEnvironment;
typedef struct SubGhzProtocol SubGhzProtocol;
typedef struct Storage Storage;
typedef struct File File;
typedef struct {
    int key;
} InputEvent;

typedef enum {
    GuiButtonTypeLeft,
    GuiButtonTypeCenter,
    GuiButtonTypeRight,
} GuiButtonType;

typedef enum {
    InputTypeShort,
    InputTypeLong,
} InputType;

typedef enum {
    FSAM_READ = 1,
    FSAM_WRITE = 2,
    FSAM_READ_WRITE = 3,
} FS_AccessMode;

typedef enum {
    FSOM_OPEN_EXISTING = 1,
    FSOM_CREATE_ALWAYS = 5,
} FS_OpenMode;

typedef enum {
    FSE_OK,
    FSE_INTERNAL,
} FS_Error;

typedef enum {
    ColorWhite,
    ColorBlack,
} Color;

typedef enum {
    FontSecondary,
} Font;

typedef enum {
    AlignLeft,
    AlignRight,
    AlignTop,
    AlignBottom,
    AlignCenter,
} Align;

// host.c
extern const char* host_root; // where "/..." paths live
extern size_t host_heap; // the heap's size; what the app allocates is taken from it

/* The app's allocations count against host_heap, and like the firmware's
 * malloc going past it aborts. */
void* host_malloc(size_t n);
void* host_realloc(void* p, size_t n);
void host_free(void* p);
size_t host_heap_free(void);
#ifndef HOST_C
#define malloc  host_malloc
#define realloc host_realloc
#define free    host_free
#endif

File* storage_file_alloc(Storage* st);
void storage_file_free(File* f);
bool storage_file_open(File* f, const char* path, FS_AccessMode access, FS_OpenMode open);
bool storage_file_close(File* f);
size_t storage_file_read(File* f, void* buf, size_t n);
size_t storage_file_write(File* f, const void* buf, size_t n);
bool storage_file_seek(File* f, uint32_t offset, bool from_start);
bool storage_file_truncate(File* f);
bool storage_file_exists(Storage* st, const char* path);
FS_Error storage_common_rename(Storage* st, const char* from, const char* to);
bool storage_simply_remove(Storage* st, const char* path);
bool storage_simply_remove_recursive(Storage* st, const char* path);
bool storage_simply_mkdir(Storage* st, const char* path);

FuriString* furi_string_alloc(void);
FuriString* furi_string_alloc_set_str(const char* s);
void furi_string_free(FuriString* s);
void furi_string_reset(FuriString* s);
void furi_string_reserve(FuriString* s, size_t n);
void furi_string_set(FuriString* s, const char* v);
void furi_string_cat_str(FuriString* s, const char* v);
void furi_string_push_back(FuriString* s, char c);
void furi_string_printf(FuriString* s, const char* fmt, ...);
const char* furi_string_get_cstr(const FuriString* s);
size_t furi_string_size(const FuriString* s);

size_t host_strlcpy(char* dst, const char* src, size_t n);
#define strlcpy host_strlcpy

static inline size_t memmgr_get_free_heap(void) {
    return host_heap_free();
}

static inline size_t memmgr_heap_get_max_free_block(void) {
    return host_heap_free();
}

static inline uint32_t furi_get_tick(void) {
    return 0;
}

static inline void furi_mutex_acquire(FuriMutex* m, uint32_t t) {
    (void)m, (void)t;
}

static inline void furi_mutex_release(FuriMutex* m) {
    (void)m;
}

static inline void furi_message_queue_put(FuriMessageQueue* q, const void* e, uint32_t t) {
    (void)q, (void)e, (void)t;
}

// Drawing: nothing
static inline void canvas_clear(Canvas* c) {
    (void)c;
}

static inline void canvas_set_color(Canvas* c, Color v) {
    (void)c, (void)v;
}

static inline void canvas_set_font(Canvas* c, Font v) {
    (void)c, (void)v;
}

static inline void canvas_draw_str(Canvas* c, int x, int y, const char* s) {
    (void)c, (void)x, (void)y, (void)s;
}

static inline void
    canvas_draw_str_aligned(Canvas* c, int x, int y, Align h, Align v, const char* s) {
    (void)c, (void)x, (void)y, (void)h, (void)v, (void)s;
}

static inline void canvas_draw_dot(Canvas* c, int x, int y) {
    (void)c, (void)x, (void)y;
}

static inline void canvas_draw_line(Canvas* c, int x0, int y0, int x1, int y1) {
    (void)c, (void)x0, (void)y0, (void)x1, (void)y1;
}

static inline void canvas_draw_box(Canvas* c, int x, int y, int w, int h) {
    (void)c, (void)x, (void)y, (void)w, (void)h;
}

static inline void canvas_draw_frame(Canvas* c, int x, int y, int w, int h) {
    (void)c, (void)x, (void)y, (void)w, (void)h;
}

static inline uint16_t canvas_string_width(Canvas* c, const char* s) {
    (void)c, (void)s;
    return 0;
}

static inline size_t canvas_glyph_width(Canvas* c, uint16_t g) {
    (void)c, (void)g;
    return 0;
}

static inline void elements_scrollable_text_line(
    Canvas* c,
    int x,
    int y,
    size_t w,
    FuriString* s,
    size_t scroll,
    bool ellipsis) {
    (void)c, (void)x, (void)y, (void)w, (void)s, (void)scroll, (void)ellipsis;
}

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <furi.h>

/* ============================================================================
 * Max values
 * ============================================================================ */
#define BIBLE_MAX_BOOKS            66
#define BIBLE_MAX_CHAPTERS         150
#define BIBLE_MAX_FILTER_OPTIONS   22
#define BIBLE_MAX_COLLECTION       100
#define BIBLE_MAX_VERSES           176 /* Psalms 119 = 176 verses */
#define BIBLE_MAX_LINES            800 /* Psalm 119 wraps to about 770 lines */
#define BIBLE_MAX_CHAPTER_SECTIONS 24 /* Psalm 119 has 22 sections */
#define BIBLE_MAX_FILE_SIZE        20000
#define BIBLE_MAX_COLLECTION_SIZE  2048
#define BIBLE_READ_CHUNK           1024

/* ============================================================================
 * Toast system
 * ============================================================================ */
#define BIBLE_TOAST_MAX_LEN 64
#define BIBLE_TOAST_FRAMES  60

typedef struct {
    char message[BIBLE_TOAST_MAX_LEN];
    uint8_t timer;
} BibleToast;

static inline void bible_toast_set(BibleToast* toast, const char* msg) {
    furi_check(toast);
    if(msg) {
        strlcpy(toast->message, msg, BIBLE_TOAST_MAX_LEN);
        toast->timer = BIBLE_TOAST_FRAMES;
    } else {
        toast->message[0] = '\0';
        toast->timer = 0;
    }
}

static inline void bible_toast_tick(BibleToast* toast) {
    if(toast->timer > 0) {
        toast->timer--;
        if(toast->timer == 0) {
            toast->message[0] = '\0';
        }
    }
}

static inline bool bible_toast_active(const BibleToast* toast) {
    return toast->timer > 0;
}

/* ============================================================================
 * Line (rendered wrapped line)
 * ============================================================================ */
typedef struct {
    char text[28]; /* max 20 chars per line + null terminator + alignment */
    uint16_t verse_number; /* verse this line belongs to (0 = unassigned) */
    bool is_verse_number; /* true if first line of a verse (has number prefix) */
} BibleLine;

typedef struct {
    uint16_t start_verse;
    char title[56];
} BibleSectionHeading;

/* ============================================================================
 * Passage (currently loaded chapter metadata — verses live on SD, not RAM)
 * ============================================================================ */
typedef struct {
    uint8_t book_index; /* 0-65 */
    uint16_t chapter; /* 1-150 */
    uint16_t start_verse;
    uint16_t end_verse;
    uint16_t verse_count;
    uint8_t section_count;
    BibleSectionHeading sections[BIBLE_MAX_CHAPTER_SECTIONS];
} BiblePassage;

/* ============================================================================
 * Collection entry
 * ============================================================================ */
typedef struct {
    char scripture_ref[32];
    char scripture_display_ref[32];
    char scripture_translation[8];
    uint8_t book_index;
    uint16_t chapter;
    uint16_t start_verse;
    uint16_t end_verse;
    char captured_at[32];
    char note[64];
} BibleCollectionEntry;

/* ============================================================================
 * App state
 * ============================================================================ */
typedef struct BibleAppState {
    /* Navigation */
    uint8_t current_scene; /* matches BibleScene enum */
    uint8_t selected_book; /* 0-65 */
    char selected_book_name[20];
    uint16_t selected_chapter; /* 1-150 */
    uint16_t selected_start_verse;
    uint16_t selected_end_verse;
    uint8_t verse_select_mode; /* 0=All, 1=Start, 2=End */
    uint8_t book_filter_idx; /* 0-22 */
    bool genre_mode_enabled; /* true = Testament -> Genre -> Book hierarchy */
    uint8_t browse_depth; /* 0 = Testament, 1 = Genre, 2 = Book */
    uint8_t selected_testament; /* 0 = OT, 1 = NT */
    uint8_t selected_genre; /* 0-9 index into BIBLE_GENRES */
    uint16_t scroll_offset;
    uint16_t book_scroll;
    uint16_t collection_scroll;
    uint8_t collection_selected;
    uint8_t action_menu_selection;
    bool action_menu_from_collection;
    uint16_t goto_verse;

    /* Data */
    BiblePassage passage;
    BibleCollectionEntry* collection; /* dynamically allocated, NULL if empty */
    uint8_t collection_count;
    uint8_t collection_capacity;
    bool collection_loaded;
    bool bsb_data_present; /* true when the bundled bsb.pack is installed */
    bool show_data_help; /* true when a chapter file was missing */

    /* Rendering */
    BibleLine* lines; /* dynamically allocated, NULL if no lines */
    uint16_t line_count;
    uint16_t lines_capacity;
    uint16_t current_page;
    bool reader_on_chapter_page; /* true when showing the chapter boundary title page */
    uint32_t header_scroll_ticks; /* tick counter for pause-then-scroll header marquee */
    const void* header_scroll_key; /* tracks active header so scroll resets on change */

    /* Feedback */
    BibleToast toast;

    /* NFC / sharing */
    bool verse_select_for_nfc;
    bool reader_came_from_collection;
    char nfc_url[128];
    bool nfc_emitting;
    bool nfc_is_export;
    uint8_t nfc_pulse_counter;
} BibleAppState;

/* ============================================================================
 * Dynamic memory helpers
 * ============================================================================ */

/** Grow a heap block.
 *
 * Flipper's realloc (SDK 1.4.3) mallocs the new block and then memcpy's into it
 * before testing the pointer, so a failed growth HardFaults inside realloc and
 * the old block is not safe to keep using. This allocates first and leaves `ptr`
 * owned by the caller when malloc fails.
 *
 * On success the caller owns the returned pointer and `ptr` has been freed.
 * `old_size` is the number of bytes to preserve (0 if `ptr` is NULL).
 */
static inline void* bible_heap_grow(void* ptr, size_t old_size, size_t new_size) {
    if(new_size == 0) {
        free(ptr);
        return NULL;
    }

    void* next = malloc(new_size);
    if(!next) {
        return NULL;
    }

    if(ptr) {
        if(old_size > 0) {
            size_t copy = old_size < new_size ? old_size : new_size;
            memcpy(next, ptr, copy);
        }
        free(ptr);
    }
    return next;
}

/** Ensure lines array has at least `capacity` slots. Grows incrementally (max ~1KB per alloc).
 *  Returns false on allocation failure. The previous buffer, if any, is left unchanged. */
static inline bool bible_lines_ensure(BibleAppState* state, uint16_t capacity) {
    furi_check(state);
    if(capacity == 0) return true;
    if(state->lines && state->lines_capacity >= capacity) return true;

    /* Max 15 lines per realloc: 15 * sizeof(BibleLine) <= 1020 bytes */
    const uint16_t LINES_PER_GROW = 15;

    uint16_t new_cap = state->lines_capacity;
    while(new_cap < capacity) {
        new_cap += LINES_PER_GROW;
    }
    if(new_cap > BIBLE_MAX_LINES) new_cap = BIBLE_MAX_LINES;

    size_t old_bytes = (size_t)state->lines_capacity * sizeof(BibleLine);
    size_t new_bytes = (size_t)new_cap * sizeof(BibleLine);
    BibleLine* new_lines = bible_heap_grow(state->lines, old_bytes, new_bytes);
    if(!new_lines) return false;
    /* Zero-initialize newly allocated slots */
    if(new_cap > state->lines_capacity) {
        memset(
            new_lines + state->lines_capacity,
            0,
            (new_cap - state->lines_capacity) * sizeof(BibleLine));
    }
    state->lines = new_lines;
    state->lines_capacity = new_cap;
    return true;
}

/** Free the lines array. */
static inline void bible_lines_free(BibleAppState* state) {
    furi_check(state);
    if(state->lines) {
        free(state->lines);
        state->lines = NULL;
    }
    state->line_count = 0;
    state->lines_capacity = 0;
}

/** Ensure collection array has at least `capacity` slots.
 *  Returns false on allocation failure. The previous buffer, if any, is left unchanged. */
static inline bool bible_collection_ensure(BibleAppState* state, uint16_t capacity) {
    furi_check(state);
    if(capacity == 0) return true;
    if(state->collection && state->collection_capacity >= capacity) return true;

    uint16_t new_cap = state->collection_capacity > 0 ? state->collection_capacity * 2 : 8;
    while(new_cap < capacity)
        new_cap *= 2;
    if(new_cap > BIBLE_MAX_COLLECTION) new_cap = BIBLE_MAX_COLLECTION;

    size_t old_bytes = (size_t)state->collection_capacity * sizeof(BibleCollectionEntry);
    size_t new_bytes = (size_t)new_cap * sizeof(BibleCollectionEntry);
    BibleCollectionEntry* new_col = bible_heap_grow(state->collection, old_bytes, new_bytes);
    if(!new_col) return false;
    state->collection = new_col;
    state->collection_capacity = new_cap;
    return true;
}

/** Free the collection array. */
static inline void bible_collection_free(BibleAppState* state) {
    furi_check(state);
    if(state->collection) {
        free(state->collection);
        state->collection = NULL;
    }
    state->collection_count = 0;
    state->collection_capacity = 0;
}

/* ============================================================================
 * Helpers
 * ============================================================================ */
static inline void bible_app_state_init(BibleAppState* state) {
    furi_check(state);
    memset(state, 0, sizeof(BibleAppState));
    state->current_scene = 0; /* BibleSceneBookList */
    state->selected_book = 0;
    state->selected_chapter = 1;
    state->selected_start_verse = 1;
    state->selected_end_verse = 1;
    state->verse_select_mode = 0;
    state->book_filter_idx = 0;
    state->genre_mode_enabled = false;
    state->browse_depth = 0;
    state->selected_testament = 0;
    state->selected_genre = 0;
    state->action_menu_selection = 0;
    state->action_menu_from_collection = false;
    state->goto_verse = 1;
    state->collection_count = 0;
    state->collection_loaded = false;
    state->line_count = 0;
    state->lines_capacity = 0;
    state->current_page = 0;
    state->reader_on_chapter_page = false;
    state->header_scroll_ticks = 0;
    state->header_scroll_key = NULL;
    state->verse_select_for_nfc = false;
    state->reader_came_from_collection = false;
    state->nfc_emitting = false;
    state->nfc_is_export = false;
}

/** Free all dynamically allocated memory in the state. Call before free(state). */
static inline void bible_app_state_deinit(BibleAppState* state) {
    furi_check(state);
    bible_lines_free(state);
    bible_collection_free(state);
}

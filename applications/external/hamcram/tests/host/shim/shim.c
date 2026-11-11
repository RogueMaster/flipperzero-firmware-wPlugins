#include <furi.h>
#include <furi_hal_random.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <storage/storage.h>

#undef malloc

/* ----------------------------------------------------------------- furi */

void shim_check_failed(const char* file, int line) {
    fprintf(stderr, "furi_check failed at %s:%d\n", file, line);
    abort();
}

size_t shim_strlcpy(char* dest, const char* src, size_t size) {
    size_t length = strlen(src);
    if(size) {
        size_t copy = length < size - 1 ? length : size - 1;
        memcpy(dest, src, copy);
        dest[copy] = '\0';
    }
    return length;
}

static uint32_t shim_random_state = 0x2545F491;

void shim_random_seed(uint32_t seed) {
    shim_random_state = seed ? seed : 1;
}

uint32_t furi_hal_random_get(void) {
    /* xorshift32 */
    uint32_t x = shim_random_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    shim_random_state = x;
    return x;
}

/* -------------------------------------------------------------- storage */

struct File {
    FILE* handle;
};

static int shim_rename_failures = 0;

void shim_storage_fail_renames(int count) {
    shim_rename_failures = count;
}

File* storage_file_alloc(Storage* storage) {
    UNUSED(storage);
    return calloc(1, sizeof(File));
}

void storage_file_free(File* file) {
    if(file->handle) fclose(file->handle);
    free(file);
}

bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode) {
    if(file->handle) fclose(file->handle);
    const char* flags = "rb";
    if(access == FSAM_WRITE && mode == FSOM_CREATE_ALWAYS) {
        flags = "wb";
    } else if(access != FSAM_READ || mode != FSOM_OPEN_EXISTING) {
        fprintf(stderr, "shim: unsupported open mode\n");
        abort();
    }
    file->handle = fopen(path, flags);
    return file->handle != NULL;
}

bool storage_file_close(File* file) {
    if(!file->handle) return false;
    bool ok = fclose(file->handle) == 0;
    file->handle = NULL;
    return ok;
}

size_t storage_file_read(File* file, void* buffer, size_t size) {
    return file->handle ? fread(buffer, 1, size, file->handle) : 0;
}

size_t storage_file_write(File* file, const void* buffer, size_t size) {
    return file->handle ? fwrite(buffer, 1, size, file->handle) : 0;
}

bool storage_file_seek(File* file, uint32_t offset, bool from_start) {
    if(!file->handle) return false;
    return fseek(file->handle, (long)offset, from_start ? SEEK_SET : SEEK_CUR) == 0;
}

uint64_t storage_file_size(File* file) {
    if(!file->handle) return 0;
    long position = ftell(file->handle);
    fseek(file->handle, 0, SEEK_END);
    long size = ftell(file->handle);
    fseek(file->handle, position, SEEK_SET);
    return size < 0 ? 0 : (uint64_t)size;
}

FS_Error storage_common_remove(Storage* storage, const char* path) {
    UNUSED(storage);
    return remove(path) == 0 ? FSE_OK : FSE_NOT_EXIST;
}

FS_Error storage_common_rename(Storage* storage, const char* old_path, const char* new_path) {
    UNUSED(storage);
    if(shim_rename_failures > 0) {
        shim_rename_failures--;
        return FSE_INTERNAL;
    }
    return rename(old_path, new_path) == 0 ? FSE_OK : FSE_INTERNAL;
}

/* ----------------------------------------------------------------- view */

struct View {
    void* model;
    void* context;
    ViewDrawCallback draw;
    ViewInputCallback input;
};

View* view_alloc(void) {
    return calloc(1, sizeof(View));
}

void view_free(View* view) {
    free(view->model);
    free(view);
}

void view_allocate_model(View* view, ViewModelType type, size_t size) {
    UNUSED(type);
    view->model = calloc(1, size);
}

void view_set_context(View* view, void* context) {
    view->context = context;
}

void view_set_draw_callback(View* view, ViewDrawCallback callback) {
    view->draw = callback;
}

void view_set_input_callback(View* view, ViewInputCallback callback) {
    view->input = callback;
}

void* view_get_model(View* view) {
    return view->model;
}

void view_commit_model(View* view, bool update) {
    UNUSED(view);
    UNUSED(update);
}

void shim_view_draw(View* view, Canvas* canvas) {
    memset(canvas, 0, sizeof(Canvas));
    canvas->color = ColorBlack;
    view->draw(canvas, view->model);
}

bool shim_view_input(View* view, InputKey key, InputType type) {
    InputEvent event = {.key = key, .type = type};
    return view->input(&event, view->context);
}

/* --------------------------------------------------------------- canvas */

void canvas_set_font(Canvas* canvas, Font font) {
    UNUSED(canvas);
    UNUSED(font);
}

void canvas_set_color(Canvas* canvas, Color color) {
    canvas->color = color;
}

void canvas_invert_color(Canvas* canvas) {
    canvas->color = canvas->color == ColorBlack ? ColorWhite : ColorBlack;
}

/* A rough proportional font: narrow, regular and wide glyphs, similar in
 * spread to the firmware's secondary font. */
size_t canvas_glyph_width(Canvas* canvas, uint16_t symbol) {
    UNUSED(canvas);
    if(symbol < 32 || symbol > 126) return 0;
    if(strchr("il.,:;!|'", (int)symbol)) return 2;
    if(strchr(" ()[]fjrt1-", (int)symbol)) return 4;
    if(strchr("mwMW@%", (int)symbol)) return 8;
    return 6;
}

uint16_t canvas_string_width(Canvas* canvas, const char* str) {
    size_t width = 0;
    for(; *str; str++)
        width += canvas_glyph_width(canvas, (uint8_t)*str);
    return (uint16_t)width;
}

void canvas_draw_box(Canvas* canvas, int32_t x, int32_t y, size_t width, size_t height) {
    UNUSED(x);
    UNUSED(y);
    UNUSED(width);
    UNUSED(height);
    canvas->box_count++;
}

void canvas_draw_line(Canvas* canvas, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    UNUSED(canvas);
    UNUSED(x1);
    UNUSED(y1);
    UNUSED(x2);
    UNUSED(y2);
}

void canvas_draw_str(Canvas* canvas, int32_t x, int32_t y, const char* str) {
    if(canvas->string_count >= SHIM_MAX_STRINGS) {
        fprintf(stderr, "shim: too many strings drawn\n");
        abort();
    }
    ShimString* string = &canvas->strings[canvas->string_count++];
    string->x = x;
    string->y = y;
    string->color = canvas->color;
    shim_strlcpy(string->text, str, sizeof(string->text));
}

void canvas_draw_str_aligned(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    Align horizontal,
    Align vertical,
    const char* str) {
    UNUSED(vertical);
    int32_t width = canvas_string_width(canvas, str);
    if(horizontal == AlignRight) x -= width;
    if(horizontal == AlignCenter) x -= width / 2;
    canvas_draw_str(canvas, x, y, str);
}

void elements_scrollbar_pos(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    size_t height,
    size_t pos,
    size_t total) {
    UNUSED(x);
    UNUSED(y);
    UNUSED(height);
    furi_check(pos < total);
    canvas->scrollbar = true;
}

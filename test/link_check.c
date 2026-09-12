#include <stdio.h>
/* Not a test: a link. Every source file is compiled on its device path,
   with no BB_HOST_TEST, and linked against do-nothing hardware. It
   proves the firmware has one definition of everything it uses and no
   two of anything, which is the half of a build that ufbt would catch
   and the host tests would not. */
#include "beepback.h"
#include <furi_hal_random.h>

uint32_t furi_get_tick(void) {
    return 0;
}
uint32_t furi_ms_to_ticks(uint32_t ms) {
    return ms;
}
void furi_delay_ms(uint32_t ms) {
    UNUSED(ms);
}
FuriMutex* furi_mutex_alloc(FuriMutexType t) {
    UNUSED(t);
    return NULL;
}
void furi_mutex_free(FuriMutex* m) {
    UNUSED(m);
}
FuriStatus furi_mutex_acquire(FuriMutex* m, uint32_t timeout) {
    UNUSED(m);
    UNUSED(timeout);
    return FuriStatusOk;
}
FuriStatus furi_mutex_release(FuriMutex* m) {
    UNUSED(m);
    return FuriStatusOk;
}
FuriMessageQueue* furi_message_queue_alloc(uint32_t cap, uint32_t size) {
    UNUSED(cap);
    UNUSED(size);
    return NULL;
}
void furi_message_queue_free(FuriMessageQueue* q) {
    UNUSED(q);
}
FuriStatus furi_message_queue_put(FuriMessageQueue* q, const void* msg, uint32_t timeout) {
    UNUSED(q);
    UNUSED(msg);
    UNUSED(timeout);
    return FuriStatusOk;
}
FuriStatus furi_message_queue_get(FuriMessageQueue* q, void* msg, uint32_t timeout) {
    UNUSED(q);
    UNUSED(msg);
    UNUSED(timeout);
    return FuriStatusError;
}
void* furi_record_open(const char* name) {
    UNUSED(name);
    return NULL;
}
void furi_record_close(const char* name) {
    UNUSED(name);
}
uint32_t furi_hal_random_get(void) {
    return 1;
}
void furi_hal_rtc_get_datetime(DateTime* dt) {
    memset(dt, 0, sizeof(*dt));
}
bool furi_hal_speaker_acquire(uint32_t t) {
    UNUSED(t);
    return true;
}
void furi_hal_speaker_release(void) {
}
bool furi_hal_speaker_is_mine(void) {
    return true;
}
void furi_hal_speaker_start(float f, float v) {
    UNUSED(f);
    UNUSED(v);
}
void furi_hal_speaker_stop(void) {
}
ViewPort* view_port_alloc(void) {
    return NULL;
}
void view_port_free(ViewPort* vp) {
    UNUSED(vp);
}
void view_port_enabled_set(ViewPort* vp, bool e) {
    UNUSED(vp);
    UNUSED(e);
}
void view_port_update(ViewPort* vp) {
    UNUSED(vp);
}
void view_port_draw_callback_set(ViewPort* vp, ViewPortDrawCallback cb, void* ctx) {
    UNUSED(vp);
    UNUSED(cb);
    UNUSED(ctx);
}
void view_port_input_callback_set(ViewPort* vp, ViewPortInputCallback cb, void* ctx) {
    UNUSED(vp);
    UNUSED(cb);
    UNUSED(ctx);
}
void gui_add_view_port(Gui* g, ViewPort* vp, GuiLayer l) {
    UNUSED(g);
    UNUSED(vp);
    UNUSED(l);
}
void gui_remove_view_port(Gui* g, ViewPort* vp) {
    UNUSED(g);
    UNUSED(vp);
}
void canvas_clear(Canvas* c) {
    UNUSED(c);
}
void canvas_set_color(Canvas* c, Color x) {
    UNUSED(c);
    UNUSED(x);
}
void canvas_set_font(Canvas* c, Font f) {
    UNUSED(c);
    UNUSED(f);
}
uint16_t canvas_string_width(Canvas* c, const char* s) {
    UNUSED(c);
    UNUSED(s);
    return 10;
}
void canvas_draw_str(Canvas* c, int32_t x, int32_t y, const char* s) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(s);
}
void canvas_draw_str_aligned(
    Canvas* c,
    int32_t x,
    int32_t y,
    Align h,
    Align v,
    const char* s) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(h);
    UNUSED(v);
    UNUSED(s);
}
void canvas_draw_dot(Canvas* c, int32_t x, int32_t y) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
}
void canvas_draw_box(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
}
void canvas_draw_rbox(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
    UNUSED(r);
}
void canvas_draw_frame(Canvas* c, int32_t x, int32_t y, size_t w, size_t h) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
}
void canvas_draw_rframe(Canvas* c, int32_t x, int32_t y, size_t w, size_t h, size_t r) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(w);
    UNUSED(h);
    UNUSED(r);
}
void canvas_draw_line(Canvas* c, int32_t a, int32_t b, int32_t x, int32_t y) {
    UNUSED(c);
    UNUSED(a);
    UNUSED(b);
    UNUSED(x);
    UNUSED(y);
}
void canvas_draw_circle(Canvas* c, int32_t x, int32_t y, size_t r) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(r);
}
void canvas_draw_disc(Canvas* c, int32_t x, int32_t y, size_t r) {
    UNUSED(c);
    UNUSED(x);
    UNUSED(y);
    UNUSED(r);
}
const NotificationMessage message_do_not_reset = {NotificationMessageTypeDoNotReset, {{0}}};
const NotificationMessage message_vibro_on = {NotificationMessageTypeVibro, {{0}}};
const NotificationMessage message_vibro_off = {NotificationMessageTypeVibro, {{0}}};
const NotificationSequence sequence_blink_red_100 = {NULL};
const NotificationSequence sequence_blink_green_100 = {NULL};
const NotificationSequence sequence_display_backlight_enforce_on = {NULL};
const NotificationSequence sequence_display_backlight_enforce_auto = {NULL};
void notification_message(NotificationApp* app, const NotificationSequence* seq) {
    UNUSED(app);
    UNUSED(seq);
}
File* storage_file_alloc(Storage* s) {
    UNUSED(s);
    return NULL;
}
void storage_file_free(File* f) {
    UNUSED(f);
}
bool storage_file_open(File* f, const char* p, FS_AccessMode am, FS_OpenMode om) {
    UNUSED(f);
    UNUSED(p);
    UNUSED(am);
    UNUSED(om);
    return false;
}
bool storage_file_close(File* f) {
    UNUSED(f);
    return true;
}
size_t storage_file_read(File* f, void* b, size_t n) {
    UNUSED(f);
    UNUSED(b);
    UNUSED(n);
    return 0;
}
size_t storage_file_write(File* f, const void* b, size_t n) {
    UNUSED(f);
    UNUSED(b);
    UNUSED(n);
    return n;
}
FS_Error storage_common_mkdir(Storage* s, const char* p) {
    UNUSED(s);
    UNUSED(p);
    return FSE_OK;
}

int32_t beepback_app(void* p);

int main(void) {
    /* never run: the point is that it links */
    if(bb_window_ms(BbModeClassic, 1) == 0) return (int)beepback_app(NULL);
    printf("ok   the whole firmware links as one binary\n");
    return 0;
}

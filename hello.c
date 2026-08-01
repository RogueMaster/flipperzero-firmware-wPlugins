#include <furi.h>
#include <gui/gui.h>
#include "hello_zh.h"

// 应用事件队列里传递的事件类型
#define EVENT_TYPE_KEY 1

typedef struct {
    uint8_t type;
    InputEvent input;
} AppEvent;

// 绘制回调:在屏幕上同时显示中英文
static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // 顶部标题(英文)
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "Hello App");

    // 分隔线
    canvas_draw_line(canvas, 0, 14, 127, 14);

    // 英文问候
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 27, "Hello, World!");

    // 中文问候(内置字体不含 CJK,用预渲染位图显示)
    canvas_draw_xbm(canvas, 27, 33, ZH_W, ZH_H, zh_bitmap);

    // 底部操作提示
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 62, "Press Back to exit");
}

// 输入回调:按键事件压入队列
static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    AppEvent e = {.type = EVENT_TYPE_KEY, .input = *event};
    furi_message_queue_put(queue, &e, FuriWaitForever);
}

// 应用入口
int32_t hello_app(void* p) {
    UNUSED(p);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, NULL);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // 事件循环:按返回键退出
    AppEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == EVENT_TYPE_KEY &&
               event.input.type == InputTypeShort &&
               event.input.key == InputKeyBack) {
                running = false;
            }
        }
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_record_close(RECORD_GUI);
    return 0;
}

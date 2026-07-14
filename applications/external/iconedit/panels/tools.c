#include <gui/canvas.h>
#include <gui/icon.h>
#include <input/input.h>

#include "panels.h"
#include "../icon.h"
#include "../iconedit.h"
#include "utils/draw.h"
#include "../utils/notification.h"

#include <iconedit_icons.h>

#define TOOL_WIDTH  14
#define TOOL_HEIGHT 14

static struct {
    ToolCallback tool_callback;
    EditorTool tool;
} toolModel = {.tool_callback = NULL, .tool = Tool_Draw};

const char* EditorTool_Desc[Tool_COUNT] = {
    "Draw pixel",
    "Draw Line",
    "Draw Circle",
    "Draw Rect",
    "Dupe Frame",
    "Delete Frame",
    "Play", // View
    "Change FPS",
    "Prev Frame",
    "Next Frame"};

// Tool selection allowed movements
const EditorTool EditorTool_UP[Tool_COUNT] = {
    Tool_TABBAR,
    Tool_TABBAR,
    Tool_TABBAR,
    Tool_TABBAR,
    Tool_Draw,
    Tool_Line,
    Tool_Circle,
    Tool_Rect,
    Tool_AddFrame,
    Tool_DeleteFrame};
const EditorTool EditorTool_DOWN[Tool_COUNT] = {
    Tool_AddFrame,
    Tool_DeleteFrame,
    Tool_Play,
    Tool_FPS,
    Tool_PrevFrame,
    Tool_NextFrame,
    Tool_NextFrame,
    Tool_NextFrame,
    Tool_NONE,
    Tool_NONE};
const EditorTool EditorTool_LEFT[Tool_COUNT] = {
    Tool_NONE,
    Tool_Draw,
    Tool_Line,
    Tool_Circle,
    Tool_NONE,
    Tool_AddFrame,
    Tool_DeleteFrame,
    Tool_Play,
    Tool_NONE,
    Tool_PrevFrame};
const EditorTool EditorTool_RIGHT[Tool_COUNT] = {
    Tool_Line,
    Tool_Circle,
    Tool_Rect,
    Tool_NONE,
    Tool_DeleteFrame,
    Tool_Play,
    Tool_FPS,
    Tool_NONE,
    Tool_NextFrame,
    Tool_NONE};

const Icon* EditorTool_Icon[Tool_COUNT] = {
    [Tool_Draw] = &I_iet_Draw,
    [Tool_Line] = &I_iet_Line,
    [Tool_Circle] = &I_iet_Circle,
    [Tool_Rect] = &I_iet_Rect,
    [Tool_AddFrame] = &I_iet_Frame,
    [Tool_DeleteFrame] = &I_iet_Delete,
    [Tool_Play] = &I_iet_Play, // or View
    [Tool_FPS] = &I_iet_FPS,
    [Tool_PrevFrame] = &I_iet_LArrow,
    [Tool_NextFrame] = &I_iet_RArrow,
};

EditorTool tools_get_selected_tool() {
    return toolModel.tool;
}

void tools_draw(Canvas* canvas, void* context) {
    IconEdit* app = context;

    // draw tools 4xn
    int tool_R = 4; // number of tools per row
    int tool_x = 3; // These should be #defines
    int tool_y = 11;

    for(int t = Tool_START; t < Tool_COUNT; t++) {
        if(t == Tool_Play && app->icon->frame_count == 1) {
            canvas_draw_icon(
                canvas,
                tool_x + 2 + (t % tool_R) * TOOL_WIDTH,
                tool_y + 2 + (t / tool_R) * TOOL_HEIGHT,
                &I_iet_View);
        } else {
            canvas_draw_icon(
                canvas,
                tool_x + 2 + (t % tool_R) * TOOL_WIDTH,
                tool_y + 2 + (t / tool_R) * TOOL_HEIGHT,
                EditorTool_Icon[t]);
        }
        if((EditorTool)t == toolModel.tool) {
            if(app->panel == Panel_Tools) {
                if(t == Tool_Play && app->icon->frame_count == 1) {
                    ie_draw_str(canvas, 1, 64 - 7, AlignLeft, AlignTop, Font5x7, "View");
                } else {
                    ie_draw_str(
                        canvas, 1, 64 - 7, AlignLeft, AlignTop, Font5x7, EditorTool_Desc[t]);
                }
                canvas_set_color(canvas, ColorXOR);
                canvas_draw_rbox(
                    canvas,
                    tool_x + (t % tool_R) * TOOL_WIDTH,
                    tool_y + (t / tool_R) * TOOL_HEIGHT,
                    TOOL_WIDTH,
                    TOOL_HEIGHT,
                    0);
                canvas_set_color(canvas, ColorBlack);
            } else if(app->panel == Panel_Canvas) {
                canvas_draw_rframe(
                    canvas,
                    tool_x + (t % tool_R) * TOOL_WIDTH,
                    tool_y + (t / tool_R) * TOOL_HEIGHT,
                    TOOL_WIDTH,
                    TOOL_HEIGHT,
                    1);
            }
        }
    }
    // current frame
    char cf[16];
    snprintf(cf, sizeof(cf), "f%zu/%zu", app->icon->current_frame + 1, app->icon->frame_count);
    ie_draw_str(canvas, 64 - 4, 64 - 10, AlignRight, AlignBottom, Font5x7, cf);
}

bool tools_input(InputEvent* event, void* context) {
    IconEdit* app = context;
    FURI_LOG_I("Tools", "input_callback: %d, tool: %d", event->type, toolModel.tool);
    bool handled = false;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp: {
            EditorTool next_tool = EditorTool_UP[toolModel.tool];
            if(next_tool == Tool_TABBAR) {
                app->panel = Panel_TabBar;
            } else if(next_tool != Tool_NONE) {
                toolModel.tool = next_tool;
            }
            break;
        }
        case InputKeyDown: {
            EditorTool next_tool = EditorTool_DOWN[toolModel.tool];
            if(next_tool != Tool_NONE) {
                toolModel.tool = next_tool;
            }
            break;
        }
        case InputKeyLeft: {
            EditorTool next_tool = EditorTool_LEFT[toolModel.tool];
            if(next_tool != Tool_NONE) {
                toolModel.tool = next_tool;
            }
            break;
        }
        case InputKeyRight: {
            EditorTool next_tool = EditorTool_RIGHT[toolModel.tool];
            if(next_tool != Tool_NONE) {
                toolModel.tool = next_tool;
            }
            break;
        }
        case InputKeyOk: {
            handled = true;
            switch(toolModel.tool) {
            case Tool_Draw:
            case Tool_Line:
            case Tool_Circle:
            case Tool_Rect:
                app->panel = Panel_Canvas;
                break;
            case Tool_AddFrame:
                ie_icon_duplicate_frame(app->icon, app->icon->current_frame);
                app->dirty = true;
                break;
            case Tool_DeleteFrame:
                if(app->icon->frame_count > 1) {
                    ie_icon_delete_frame(app->icon, app->icon->current_frame);
                } else {
                    notify_error(app);
                }
                app->dirty = true;
                break;
            case Tool_Play:
                // if(app->icon->frame_count > 1) {
                app->panel = Panel_Playback;
                playback_start(app->icon);
                // } else {
                // notify_error(app);
                // }
                break;
            case Tool_FPS:
                if(app->icon->frame_count > 1) {
                    app->panel = Panel_FPS;
                    fps_set_fps(app->icon->frame_rate);
                } else {
                    notify_error(app);
                }
                break;
            case Tool_PrevFrame:
                if(app->icon->frame_count > 1) {
                    app->icon->current_frame -= app->icon->current_frame > 0;
                } else {
                    notify_error(app);
                }
                break;
            case Tool_NextFrame:
                if(app->icon->frame_count > 1) {
                    app->icon->current_frame += app->icon->current_frame + 1 <
                                                app->icon->frame_count;
                } else {
                    notify_error(app);
                }
                break;
            default:
                break;
            }
            break;
        }
        case InputKeyBack: {
            app->panel = Panel_TabBar;
            break;
        }
        default:
            break;
        }
    }
    return handled;
}

#pragma once

typedef enum {
    Tool_START,
    Tool_Draw = Tool_START,
    Tool_Line,
    Tool_Circle,
    Tool_Rect,
    Tool_AddFrame, // Dupe Frame
    Tool_DeleteFrame,
    Tool_Play,
    Tool_FPS,
    Tool_PrevFrame,
    Tool_NextFrame,
    Tool_COUNT,
    Tool_NONE,
    Tool_TABBAR
} EditorTool;

typedef void (*ToolCallback)(EditorTool tool, void* context);
void tools_set_callback(ToolCallback cb);

EditorTool tools_get_selected_tool();

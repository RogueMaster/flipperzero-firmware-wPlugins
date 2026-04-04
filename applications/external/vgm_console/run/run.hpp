#pragma once
#include <malloc.h>
#include <furi.h>
#include <memory>
#include <gui/view.h>

class VideoGameModuleConsoleApp;

typedef enum
{
    DRAW_COMMAND_CHAR,
    DRAW_COMMAND_TEXT,
    DRAW_COMMAND_CLEAR,
    DRAW_COMMAND_BLIT,
    DRAW_COMMAND_BLIT1,
    DRAW_COMMAND_ROW
} DrawCommandType;

class VideoGameModuleConsoleRun
{
    void *appContext;        // reference to the app context
    bool shouldReturnToMenu; // Flag to signal return to menu
public:
    VideoGameModuleConsoleRun(void *appContext);
    ~VideoGameModuleConsoleRun();
    //
    void drawCommand(Canvas *canvas, DrawCommandType type, const char *data); // Draw command to update the run's visuals
    DrawCommandType getDrawCommandType(const char *commandStr);               // Helper to determine the draw command type from a string
    bool isActive() const { return shouldReturnToMenu == false; }             // Check if the run is active
    bool sendKey(InputEvent *event);                                          // Send a button press to UART
    void updateDraw(Canvas *canvas);                                          // update and draw the run
    void updateInput(InputEvent *event);                                      // update input for the run
};

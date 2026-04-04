#include "run/run.hpp"
#include "app.hpp"

VideoGameModuleConsoleRun::VideoGameModuleConsoleRun(void *appContext) : appContext(appContext), shouldReturnToMenu(false)
{
}

VideoGameModuleConsoleRun::~VideoGameModuleConsoleRun()
{
    // nothing to do
}

void VideoGameModuleConsoleRun::drawCommand(Canvas *canvas, DrawCommandType type, const char *data)
{
    if (!canvas || !data)
    {
        FURI_LOG_E("VideoGameModuleConsoleRun", "Invalid canvas or data for draw command");
        return;
    }
    switch (type)
    {
    case DRAW_COMMAND_CHAR:
    {
        // "[CHAR/%d/%d/%d]%d", x, y, color, c);
        int cx = 0, cy = 0, ccolor = 0, cval = 0;
        sscanf(data, "[CHAR/%d/%d/%d]%d", &cx, &cy, &ccolor, &cval);
        char c[2] = {(char)cval, '\0'};
        canvas_set_color(canvas, ccolor >= 0xE6 ? ColorWhite : ColorBlack);
        canvas_draw_str(canvas, cx, cy, c);
        break;
    }
    case DRAW_COMMAND_TEXT:
    {
        // "[TEXT/%d/%d/%d]%s", x, y, color, str);
        int tx = 0, ty = 0, tcolor = 0, offset = 0;
        sscanf(data, "[TEXT/%d/%d/%d]%n", &tx, &ty, &tcolor, &offset);
        const char *str = data + offset;
        ty = ty <= 10 ? 10 : ty; // adjust y for text baseline
        canvas_set_color(canvas, tcolor >= 0xE6 ? ColorWhite : ColorBlack);
        canvas_draw_str(canvas, tx, ty, str);
        break;
    }
    case DRAW_COMMAND_CLEAR:
        canvas_clear(canvas);
        break;
    case DRAW_COMMAND_BLIT:
    {
        // "[BLIT/%d/%d/%d/%d]<raw RGB332 pixel bytes>", x, y, width, height
        // 0x0A bytes in pixel data are sent as 0x09 (same black threshold)
        int x = 0, y = 0, width = 0, height = 0, offset = 0;
        sscanf(data, "[BLIT/%d/%d/%d/%d]%n", &x, &y, &width, &height, &offset);
        const uint8_t *pixels = (const uint8_t *)(data + offset);
        // Build 1bpp XBM bitmaps per row and blit via u8g2 (canvas_draw_xbm)
        // in two passes so every pixel is explicitly set to black or white.
        int stride = (width + 7) / 8; // bytes per XBM row
        for (uint16_t j = 0; j < (uint16_t)height; j++)
        {
            uint8_t xbm[16] = {0};     // bit=1 → white pixel (>= 0xE6)
            uint8_t inv_xbm[16] = {0}; // bit=1 → black pixel (< 0xE6)
            for (int i = 0; i < width && i < 128; i++)
            {
                uint8_t pixel = pixels[(int)j * width + i];
                if (pixel >= 0xE6)
                    xbm[i / 8] |= (uint8_t)(1u << (i % 8));
            }
            for (int k = 0; k < stride; k++)
                inv_xbm[k] = (uint8_t)(~xbm[k]);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_xbm(canvas, x, y + j, width, 1, inv_xbm);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_xbm(canvas, x, y + j, width, 1, xbm);
        }
        break;
    }
    case DRAW_COMMAND_BLIT1:
    {
        int width = 0, height = 0, offset = 0;
        sscanf(data, "[BLIT1/%d/%d]%n", &width, &height, &offset);
        const uint8_t *src = (const uint8_t *)(data + offset);

        for (int j = 0; j < height; j++)
        {
            const uint8_t *row_raw = src + j * 16;

            uint8_t xbm[16];
            uint8_t inv_xbm[16];
            for (int k = 0; k < 16; k++)
            {
                uint8_t b = row_raw[k];
                if (b == 0x02)
                    b = 0x00; // unescape all-black
                if (b == 0x0B)
                    b = 0x0A; // unescape newline
                xbm[k] = b;
                inv_xbm[k] = ~b;
            }

            canvas_set_color(canvas, ColorBlack);
            canvas_draw_xbm(canvas, 0, j, width, 1, inv_xbm);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_xbm(canvas, 0, j, width, 1, xbm);
        }
        break;
    }
    case DRAW_COMMAND_ROW:
    {
        // "[ROW/%d]<128 raw RGB332 bytes>\n"
        int row_y = 0, offset = 0;
        sscanf(data, "[ROW/%d]%n", &row_y, &offset);
        const uint8_t *pixels = (const uint8_t *)(data + offset);

        int width = 128;
        int stride = (width + 7) / 8; // = 16 bytes

        uint8_t xbm[16] = {0};     // white pixels
        uint8_t inv_xbm[16] = {0}; // black pixels

        for (int i = 0; i < width; i++)
        {
            uint8_t pixel = pixels[i];
            if (pixel >= 0xE6)
                xbm[i / 8] |= (uint8_t)(1u << (i % 8));
        }
        for (int k = 0; k < stride; k++)
            inv_xbm[k] = (uint8_t)(~xbm[k]);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_xbm(canvas, 0, row_y, width, 1, inv_xbm);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_xbm(canvas, 0, row_y, width, 1, xbm);
        break;
    }
    default:
        break;
    }
}

DrawCommandType VideoGameModuleConsoleRun::getDrawCommandType(const char *commandStr)
{
    if (strncmp(commandStr, "[CHAR", 5) == 0)
        return DRAW_COMMAND_CHAR;
    else if (strncmp(commandStr, "[TEXT", 5) == 0)
        return DRAW_COMMAND_TEXT;
    else if (strncmp(commandStr, "[CLEAR]", 7) == 0)
        return DRAW_COMMAND_CLEAR;
    else if (strncmp(commandStr, "[BLIT1", 6) == 0)
        return DRAW_COMMAND_BLIT1;
    else if (strncmp(commandStr, "[BLIT", 5) == 0)
        return DRAW_COMMAND_BLIT;
    else if (strncmp(commandStr, "[ROW/", 5) == 0)
        return DRAW_COMMAND_ROW;
    else
        return DRAW_COMMAND_CHAR; // default to char if unknown
}

bool VideoGameModuleConsoleRun::sendKey(InputEvent *event)
{
    VideoGameModuleConsoleApp *app = static_cast<VideoGameModuleConsoleApp *>(appContext);
    furi_check(app);
    /*
    #define KEY_A 0
    #define KEY_B 1
    #define KEY_START 3
    #define KEY_SELECT 2
    #define KEY_RIGHT 7
    #define KEY_DOWN 5
    #define KEY_LEFT 6
    #define KEY_UP 4
    */

    switch (event->key)
    {
    case InputKeyUp:
        return app->sendData("4");
    case InputKeyDown:
        return app->sendData("5");
    case InputKeyLeft:
        return app->sendData("6");
    case InputKeyRight:
        return app->sendData("7");
    case InputKeyOk:
        return app->sendData(event->type == InputTypeShort ? "0" : "3"); // OK sends "0" for A or "3" for Start if long pressed
    case InputKeyBack:
        return event->type == InputTypeShort ? app->sendData("1") : false; // Back sends "1" for B
    default:
        return false;
    };

    return false;
}

void VideoGameModuleConsoleRun::updateDraw(Canvas *canvas)
{
    VideoGameModuleConsoleApp *app = static_cast<VideoGameModuleConsoleApp *>(appContext);
    furi_check(app);

    // Drain every queued UART line so no draw commands are missed between timer ticks
    char *line;
    while ((line = app->popHttpResponse()) != nullptr)
    {
        DrawCommandType type = getDrawCommandType((const char *)line);
        this->drawCommand(canvas, type, line);
        free(line);
    }
}

void VideoGameModuleConsoleRun::updateInput(InputEvent *event)
{
    if (event->type == InputTypeLong && event->key == InputKeyBack)
    {
        // return to menu
        shouldReturnToMenu = true;
        VideoGameModuleConsoleApp *app = static_cast<VideoGameModuleConsoleApp *>(appContext);
        furi_check(app);
        app->sendData("9"); // send game exit command
    }
    else
    {
        sendKey(event);
    }
}

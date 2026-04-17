#pragma once

#include <furi.h>
#include <gui/gui.h>

// include panel specific methods
#include "tools.h"
#include "canvas.h"
#include "tabbar.h"
#include "playback.h"
#include "fps.h"
#include "send_usb.h"
#include "dialog.h"
#include "about.h"

// Define common panel methods
#define PANEL_DECLARE(PANEL)                          \
    void PANEL##_draw(Canvas* canvas, void* context); \
    bool PANEL##_input(InputEvent* event, void* context);

PANEL_DECLARE(tabbar)
PANEL_DECLARE(file)
PANEL_DECLARE(tools)
PANEL_DECLARE(playback)
PANEL_DECLARE(settings)
PANEL_DECLARE(about)
PANEL_DECLARE(canvas)
PANEL_DECLARE(new_icon)
PANEL_DECLARE(save_as)
PANEL_DECLARE(fps)
PANEL_DECLARE(send_usb)
PANEL_DECLARE(dialog)
PANEL_DECLARE(send_as)
PANEL_DECLARE(help)

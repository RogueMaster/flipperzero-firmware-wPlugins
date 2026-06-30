#pragma once

#include <furi.h>
#include <furi_hal.h>

#define FLIP_TDI_DEVELOPED "SkorP"
#define FLIP_TDI_GITHUB "https://github.com/flipperdevices/flipperzero-good-faps"

typedef enum {
    FlipTDIViewVariableItemList,
    FlipTDIViewSubmenu,
    FlipTDIViewMain,
    FlipTDIViewWidget,
} FlipTDIView;

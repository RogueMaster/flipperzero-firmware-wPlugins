#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/text_box.h>
#include <notification/notification.h>
#include <furi_hal_serial.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BunnyConnectApp BunnyConnectApp;

BunnyConnectApp* bunnyconnect_app_alloc(void);
void bunnyconnect_app_free(BunnyConnectApp* app);
int32_t bunnyconnect_app_run(BunnyConnectApp* app);

#ifdef __cplusplus
}
#endif

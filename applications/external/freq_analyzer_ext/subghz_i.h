#pragma once

#include "helpers/subghz_types.h"
#include <lib/subghz/types.h>
#include "subghz.h"
#include "views/subghz_frequency_analyzer.h"

#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <notification/notification_messages.h>
#include <gui/view_dispatcher.h>

#include "scenes/subghz_scene.h"
#include <lib/subghz/subghz_worker.h>
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>

#include "subghz_last_settings.h"

#include <lib/toolbox/path.h>

#include "helpers/subghz_txrx.h"

struct SubGhz {
    Gui* gui;
    NotificationApp* notifications;

    SubGhzTxRx* txrx;

    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    SubGhzFrequencyAnalyzer* subghz_frequency_analyzer;

    SubGhzLastSettings* last_settings;

    SubGhzProtocolFlag filter;
    SubGhzProtocolFlag ignore_filter;
};

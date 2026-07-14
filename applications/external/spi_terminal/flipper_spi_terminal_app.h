#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/scene_manager.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include <gui/modules/dialog_ex.h>
#include <gui/modules/text_box.h>
#include <gui/modules/variable_item_list.h>

#include <furi_hal_spi.h>
#include <furi_hal_spi_config.h>
#include <furi_hal_spi_types.h>

#include <cli/cli.h>

#include "views/terminal_view.h"

typedef enum {
    FlipperSPITerminalEventReceivedData
} FlipperSPITerminalEvent;

typedef struct {
    FuriString* debug_terminal_data;
} FlipperSPITerminalAppConfigDebug;

typedef enum {
    TerminalBufferBehaviourClear,
    TerminalBufferBehaviourKeep
} TerminalBufferBehaviour;

typedef struct {
    TerminalDisplayMode display_mode;
    TerminalBufferBehaviour terminal_buffer_behaviour;
    size_t rx_dma_buffer_size;
    LL_SPI_InitTypeDef spi;
    FlipperSPITerminalAppConfigDebug debug;
} FlipperSPITerminalAppConfig;

typedef struct {
    VariableItemList* view;

    TextBox* help_view;
    const char* help_string;
} FlipperSPITerminalAppScreenConfig;

typedef struct {
    TerminalView* view;
    bool is_active;

    uint8_t* rx_dma_buffer;
    FuriStreamBuffer* rx_buffer_stream;
    FuriTimer* recv_timer;
} FlipperSPITerminalAppScreenTerminal;

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    CliRegistry* cli;

    FlipperSPITerminalAppConfig config;

    DialogEx* main_screen;
    FlipperSPITerminalAppScreenConfig config_screen;
    FlipperSPITerminalAppScreenTerminal terminal_screen;
    TextBox* about_screen;
} FlipperSPITerminalApp;

#pragma once

#include "tpms_session.h"

#include <furi.h>
#include <cli/cli.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>

#define TPMS_CLI_COMMAND_NAME "tpms_rx"
#define TPMS_DEFAULT_FREQUENCY 433920000UL

/** Стек потока CLI-команды. */
#define TPMS_CLI_STACK_SIZE (4 * 1024)

/** Сколько ждать завершения CLI-сессии при выходе, мс. */
#define TPMS_CLI_STOP_TIMEOUT_MS 3000

/** Общее состояние: то, что рисуется на экране, и координация доступа
 * к радиомодулю между локальным приёмом и USB-сессией. */
typedef struct {
    FuriMutex* state_mutex;

    /* Радиомодуль занимает либо локальный приём, либо CLI-сессия. */
    FuriMutex* radio_mutex;

    /* Пока CLI-команда работает, приложение нельзя выгружать: её код
     * лежит в этом же .fap. */
    volatile uint32_t cli_sessions;
    volatile bool stop_requested;

    bool local_rx;
    bool usb_streaming;
    bool exit_blocked;
    volatile bool wake_requested;

    uint32_t frames;
    uint32_t last_id;
    uint16_t last_pressure_raw;
    int16_t last_temperature_c;
    uint32_t last_frame_tick;
    bool has_frame;

    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    CliRegistry* cli_registry;
    FuriThread* local_thread;
} TpmsBridgeApp;

/** Записать полученный кадр в общее состояние (для экрана). */
void tpms_bridge_report_frame(TpmsBridgeApp* app, const TpmsRenaultFrame* frame);

/** Реализация CLI-команды tpms_rx. */
void tpms_cli_command(PipeSide* pipe, FuriString* args, void* context);

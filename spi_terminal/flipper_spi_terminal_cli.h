#pragma once

#include "flipper_spi_terminal_app.h"

#define SPI_TERM_CLI_COMMAND "spi"

typedef struct FlipperSpiTerminalCliCommand FlipperSpiTerminalCliCommand;
typedef void (*FlipperSPITerminalCLICallback)(
    const FlipperSpiTerminalCliCommand* cmd,
    FlipperSPITerminalApp* app,
    FuriString* args);

void flipper_spi_terminal_cli_alloc(FlipperSPITerminalApp* app);
void flipper_spi_terminal_cli_free(FlipperSPITerminalApp* app);

const char* flipper_spi_terminal_cli_command_get_name(const FlipperSpiTerminalCliCommand* cmd);
const char* flipper_spi_terminal_cli_command_get_format(const FlipperSpiTerminalCliCommand* cmd);
const char*
    flipper_spi_terminal_cli_command_get_description(const FlipperSpiTerminalCliCommand* cmd);

void flipper_spi_terminal_cli_command_print_full_help();

void flipper_spi_terminal_cli_command_debug_terminal_data(
    FlipperSPITerminalApp* app,
    FuriString* data,
    bool reset_data);
void flipper_spi_terminal_cli_command_debug_data(FlipperSPITerminalApp* app, FuriString* data);

#ifndef CLI_COMMANDS_ADDED_INCLUDES
#include "views/terminal_view.h"
#include "flipper_spi_terminal_config.h"
#include "flipper_spi_terminal_cli.h"
#define CLI_COMMANDS_ADDED_INCLUDES
#endif

#ifndef CLI_COMMAND
#define CLI_COMMAND(cmdName, cmdFormat, cmdDescription, cmdImplementation)                       \
    void cmdName(                                                                                \
        const FlipperSpiTerminalCliCommand* cmd, FlipperSPITerminalApp* app, FuriString* args) { \
        UNUSED(cmd);                                                                             \
        UNUSED(app);                                                                             \
        UNUSED(args);                                                                            \
        cmdImplementation                                                                        \
    }
#endif

CLI_COMMAND(help,
            NULL,
            "Prints a list with all commands.",
            flipper_spi_terminal_cli_command_print_full_help();)

CLI_COMMAND(dbg_term_data_set,
            "<text>",
            "(DEBUG) Sets the <text> of the terminal view",
            flipper_spi_terminal_cli_command_debug_terminal_data(app, args, true);)
CLI_COMMAND(dbg_term_data_add,
            "<text>",
            "(DEBUG) Adds the <text> to the terminal view",
            flipper_spi_terminal_cli_command_debug_terminal_data(app, args, false);)
CLI_COMMAND(dbg_term_data_clear,
            "<text>",
            "(DEBUG) Removes everything from the terminal view",
            terminal_view_reset(app->terminal_screen.view);)
CLI_COMMAND(dbg_term_data_show,
            NULL,
            "(DEBUG) prints the current buffer",
            terminal_view_debug_print_buffer(app->terminal_screen.view);)

CLI_COMMAND(dbg_text_data_set,
            "<text>",
            "(DEBUG) Sets the <text> as test data in the config file",
            flipper_spi_terminal_cli_command_debug_data(app, args);)
CLI_COMMAND(dbg_text_data_clear,
            "<text>",
            "(DEBUG) removes the <text> test data from the config file",
            flipper_spi_terminal_cli_command_debug_data(app, NULL);)

CLI_COMMAND(dbg_config_print,
            NULL,
            "(DEBUG) Prints the content of the configuration file",
            flipper_spi_terminal_config_debug_print_saved();)

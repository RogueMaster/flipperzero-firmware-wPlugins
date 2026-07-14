#include "flipper_spi_terminal_cli.h"
#include "flipper_spi_terminal.h"
#include <toolbox/args.h>

struct FlipperSpiTerminalCliCommand {
    const char* name;
    const char* format;
    const char* description;
    const FlipperSPITerminalCLICallback callback;
};

const char* flipper_spi_terminal_cli_command_get_name(const FlipperSpiTerminalCliCommand* cmd) {
    furi_check(cmd);
    return cmd->name;
}

const char* flipper_spi_terminal_cli_command_get_format(const FlipperSpiTerminalCliCommand* cmd) {
    furi_check(cmd);
    return cmd->format;
}

const char*
    flipper_spi_terminal_cli_command_get_description(const FlipperSpiTerminalCliCommand* cmd) {
    furi_check(cmd);
    return cmd->description;
}

void flipper_spi_terminal_cli_print_usage(const FlipperSpiTerminalCliCommand* cmd) {
    furi_check(cmd);

    printf(" - " SPI_TERM_CLI_COMMAND " %s", cmd->name);

    if(cmd->format != NULL) {
        printf(" %s", cmd->format);
    }

    if(cmd->description != NULL) {
        printf(":\n\t%s", cmd->description);
    }

    printf("\n\n");
}

#define CLI_COMMAND_FULL_CALLBACK_NAME(cmdName) flipper_spi_terminal_cli_command_##cmdName

// Generate Callbacks for commands
#define CLI_COMMAND(cmdName, cmdFormat, cmdDescription, cmdImplementation)                       \
    static void CLI_COMMAND_FULL_CALLBACK_NAME(cmdName)(                                         \
        const FlipperSpiTerminalCliCommand* cmd, FlipperSPITerminalApp* app, FuriString* args) { \
        furi_check(cmd);                                                                         \
        SPI_TERM_LOG_D("Executing command: %s\n\t%s", cmd->name, cmd->description);              \
        furi_check(app);                                                                         \
        furi_check(args);                                                                        \
        cmdImplementation                                                                        \
    }
#include "flipper_spi_terminal_cli_commands.h"
#undef CLI_COMMAND

#define CLI_COMMAND(cmdName, cmdFormat, cmdDescription, cmdImplementation) \
    {                                                                      \
        .name = #cmdName,                                                  \
        .format = cmdFormat,                                               \
        .description = cmdDescription,                                     \
        .callback = CLI_COMMAND_FULL_CALLBACK_NAME(cmdName),               \
    },
static const FlipperSpiTerminalCliCommand commands[] = {
#include "flipper_spi_terminal_cli_commands.h"
};
#undef CLI_COMMAND

void flipper_spi_terminal_cli_command_print_full_help() {
    for(size_t i = 0; i < COUNT_OF(commands); i++) {
        const FlipperSpiTerminalCliCommand* cli_cmd = &commands[i];
        flipper_spi_terminal_cli_print_usage(cli_cmd);
    }
}

static void flipper_spi_terminal_cli_command(PipeSide* pipe, FuriString* args, void* context) {
    SPI_TERM_LOG_T("Received CLI command %s", furi_string_get_cstr(args));
    furi_check(pipe);
    furi_check(args);
    furi_check(context);

    FlipperSPITerminalApp* app = context;

    bool executed = false;
    FuriString* cmd = furi_string_alloc();
    args_read_string_and_trim(args, cmd);
    for(size_t i = 0; i < COUNT_OF(commands); i++) {
        const FlipperSpiTerminalCliCommand* cli_cmd = &commands[i];
        if(furi_string_equal_str(cmd, cli_cmd->name)) {
            SPI_TERM_LOG_T("Found command %s!", cli_cmd->name);
            cli_cmd->callback(cli_cmd, app, args);
            executed = true;
            break;
        }
    }
    furi_string_free(cmd);

    if(!executed) {
        SPI_TERM_LOG_T("Command not found! Printing usage...");
        flipper_spi_terminal_cli_command_print_full_help();
    }
}

void flipper_spi_terminal_cli_alloc(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Alloc CLI");
    furi_check(app);

    SPI_TERM_LOG_T("Open CLI");
    app->cli = furi_record_open(RECORD_CLI);

    cli_registry_add_command(
        app->cli,
        SPI_TERM_CLI_COMMAND,
        CliCommandFlagParallelSafe,
        flipper_spi_terminal_cli_command,
        app);
}

void flipper_spi_terminal_cli_free(FlipperSPITerminalApp* app) {
    SPI_TERM_LOG_T("Free CLI");
    furi_check(app);

    cli_registry_delete_command(app->cli, SPI_TERM_CLI_COMMAND);

    SPI_TERM_LOG_T("Closing CLI");
    furi_record_close(RECORD_CLI);
}

void flipper_spi_terminal_cli_command_debug_terminal_data(
    FlipperSPITerminalApp* app,
    FuriString* data,
    bool reset_data) {
    if(app->terminal_screen.is_active) {
        if(reset_data) {
            terminal_view_reset(app->terminal_screen.view);
        }

        const char* str = furi_string_get_cstr(data);
        const size_t length = furi_string_size(data);
        furi_stream_buffer_send(
            app->terminal_screen.rx_buffer_stream, str, length, FuriWaitForever);

        view_dispatcher_send_custom_event(
            app->view_dispatcher, FlipperSPITerminalEventReceivedData);
    } else {
        printf("Non on terminal screen!");
    }
}

void flipper_spi_terminal_cli_command_debug_data(FlipperSPITerminalApp* app, FuriString* data) {
    furi_check(app);

    if(!app->terminal_screen.is_active) {
        if(data != NULL && !furi_string_empty(data)) {
            furi_string_set(app->config.debug.debug_terminal_data, data);
        } else {
            furi_string_reset(app->config.debug.debug_terminal_data);
        }

        flipper_spi_terminal_config_save(&app->config);
    } else {
        printf("Can not set test data while terminal is active!");
    }
}

#include <furi.h>
#include <storage/storage.h>
#include <toolbox/pipe.h>

#include "upython.h"

static FuriStreamBuffer* stdout_buffer = NULL;

static void write_to_stdout_buffer(const char* data, size_t size, void* context) {
    UNUSED(context);

    furi_stream_buffer_send(stdout_buffer, data, size, 0);
}

void upython_cli(PipeSide* pipe, FuriString* args, void* ctx) {
    UNUSED(ctx);

    pipe_install_as_stdio(pipe);

    if(action != ActionNone) {
        printf("%s is busy!\n", TAG);

        return;
    }

    if(furi_string_empty(args)) {
        action = ActionRepl;

        upython_repl_execute();

        action = ActionNone;
    } else {
        furi_string_set(file_path, args);

        stdout_buffer = furi_stream_buffer_alloc(128, 1);

        stdout_callback = write_to_stdout_buffer;

        action = ActionExec;

        char data = '\0';

        while(action == ActionExec || !furi_stream_buffer_is_empty(stdout_buffer)) {
            if(furi_stream_buffer_receive(stdout_buffer, &data, 1, 0) > 0) {
                printf("%c", data);
            }
        }

        furi_stream_buffer_free(stdout_buffer);
    }
}

void upython_cli_register(void* args) {
    if(args != NULL) {
        file_path = furi_string_alloc_set_str(args);

        action = ActionExec;

        return;
    } else {
        file_path = furi_string_alloc();

        upython_reset_file_path();

        action = ActionNone;
    }

    CliRegistry* cli = furi_record_open(RECORD_CLI);

    cli_registry_add_command(cli, CLI, CliCommandFlagParallelSafe, upython_cli, NULL);

    furi_record_close(RECORD_CLI);
}

void upython_cli_unregister(void* args) {
    furi_string_free(file_path);

    if(args != NULL) {
        return;
    }

    CliRegistry* cli = furi_record_open(RECORD_CLI);

    cli_registry_delete_command(cli, CLI);

    furi_record_close(RECORD_CLI);
}

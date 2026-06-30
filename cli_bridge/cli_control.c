#include "cli_control.h"

#include <FreeRTOS.h>
#include <cli/cli.h>

#include <cli/cli_i.h>
#include <cli/cli_vcp.h>
#include <loader/loader.h>
#include <loader/loader_i.h>

FuriStreamBuffer* cli_tx_stream = NULL;
FuriStreamBuffer* cli_rx_stream = NULL;

static volatile bool restore_tx_stdout = false;

static void tx_handler(const uint8_t* buffer, size_t size) {
    furi_stream_buffer_send(cli_tx_stream, buffer, size, FuriWaitForever);
}

static void tx_handler_stdout(const char* buffer, size_t size, void* context) {
    UNUSED(context);
    tx_handler((const uint8_t*)buffer, size);
}

static size_t real_rx_handler(uint8_t* buffer, size_t size, uint32_t timeout) {
    size_t rx_cnt = 0;
    while(size > 0) {
        size_t batch_size = size;
        if(batch_size > 128) batch_size = 128;
        size_t len = furi_stream_buffer_receive(cli_rx_stream, buffer, batch_size, timeout);
        if(len == 0) break;
        size -= len;
        buffer += len;
        rx_cnt += len;
    }
    if(restore_tx_stdout) {
        furi_thread_set_stdout_callback(cli_vcp.tx_stdout, NULL);
    } else {
        furi_thread_set_stdout_callback(tx_handler_stdout, NULL);
    }
    return rx_cnt;
}

static CliSession* session;

static void session_init(void) {
}
static void session_deinit(void) {
    free(session);
    session = NULL;
}

static bool session_connected(void) {
    return true;
}

void clicontrol_hijack(size_t tx_size, size_t rx_size) {
    if(cli_rx_stream != NULL && cli_tx_stream != NULL) {
        return;
    }

    Cli* global_cli = furi_record_open(RECORD_CLI);

    cli_rx_stream = furi_stream_buffer_alloc(rx_size, 1);
    cli_tx_stream = furi_stream_buffer_alloc(tx_size, 1);

    session = (CliSession*)malloc(sizeof(CliSession));
    session->tx = &tx_handler;
    session->rx = &real_rx_handler;
    session->tx_stdout = &tx_handler_stdout;
    session->init = &session_init;
    session->deinit = &session_deinit;
    session->is_connected = &session_connected;

    CliCommandTree_it_t cmd_iterator;
    for(CliCommandTree_it(cmd_iterator, global_cli->commands); !CliCommandTree_end_p(cmd_iterator);
        CliCommandTree_next(cmd_iterator)) {
        CliCommand* t = CliCommandTree_cref(cmd_iterator)->value_ptr;
        // Move CliCommandFlagParallelSafe to another bit
        t->flags ^=
            ((t->flags & (CliCommandFlagParallelSafe << 8)) ^
             ((t->flags & CliCommandFlagParallelSafe) << 8));
        // Set parallel safe
        t->flags |= CliCommandFlagParallelSafe;
    }

    // Session switcharooney
    FuriThreadStdoutWriteCallback original_out_cb;
    void* original_out_ctx;
    furi_thread_get_stdout_callback(&original_out_cb, &original_out_ctx);
    cli_session_close(global_cli);
    restore_tx_stdout = false;
    cli_session_open(global_cli, session);
    furi_thread_set_stdout_callback(original_out_cb, original_out_ctx);

    furi_record_close(RECORD_CLI);
}

void clicontrol_unhijack(bool persist) {
    if(cli_rx_stream == NULL && cli_tx_stream == NULL) {
        return;
    }

    // Consume remaining tx data
    if(furi_stream_buffer_bytes_available(cli_tx_stream) > 0) {
        char sink = 0;
        while(!furi_stream_buffer_is_empty(cli_tx_stream)) {
            furi_stream_buffer_receive(cli_tx_stream, &sink, 1, FuriWaitForever);
        }
    }

    Cli* global_cli = furi_record_open(RECORD_CLI);

    if(persist) {
        // Don't trigger a terminal reset as the session switches
        //cli_vcp.is_connected = &furi_hal_version_do_i_belong_here;
    } else {
        // Send CTRL-C a few times
        char eot = 0x03;
        furi_stream_buffer_send(cli_rx_stream, &eot, 1, FuriWaitForever);
        furi_stream_buffer_send(cli_rx_stream, &eot, 1, FuriWaitForever);
        furi_stream_buffer_send(cli_rx_stream, &eot, 1, FuriWaitForever);
    }

    // Restore command flags
    CliCommandTree_it_t cmd_iterator;
    for(CliCommandTree_it(cmd_iterator, global_cli->commands); !CliCommandTree_end_p(cmd_iterator);
        CliCommandTree_next(cmd_iterator)) {
        CliCommand* t = CliCommandTree_cref(cmd_iterator)->value_ptr;
        t->flags ^=
            (((t->flags & CliCommandFlagParallelSafe) >> 8) ^
             ((t->flags & (CliCommandFlagParallelSafe << 8)) >> 8));
    }

    restore_tx_stdout = true; // Ready for next rx call

    // Session switcharooney again
    //FuriThreadStdoutWriteCallback prev_stdout = furi_thread_get_stdout_callback();
    FuriThreadStdoutWriteCallback original_out_cb;
    void* original_out_ctx;
    furi_thread_get_stdout_callback(&original_out_cb, &original_out_ctx);
    cli_session_close(global_cli);
    cli_session_open(global_cli, &cli_vcp);
    furi_thread_set_stdout_callback(original_out_cb, original_out_ctx);
    furi_record_close(RECORD_CLI);

    // Unblock waiting rx handler, restore old cli_vcp.tx_stdout
    furi_stream_buffer_send(cli_rx_stream, "_", 1, FuriWaitForever);

    // At this point, all cli_vcp functions should be restored.

    furi_stream_buffer_free(cli_rx_stream);
    furi_stream_buffer_free(cli_tx_stream);
    cli_rx_stream = NULL;
    cli_tx_stream = NULL;
}

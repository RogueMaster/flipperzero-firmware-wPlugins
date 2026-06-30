#pragma once

#include <cli/cli.h>
#include <furi.h>

#define TAG "uPython"
#define CLI "py"

typedef enum {
    ActionNone,
    ActionOpen,
    ActionRepl,
    ActionExec,
    ActionExit,
    ActionTerm
} Action;

extern FuriString* file_path;
extern volatile Action action;
extern volatile FuriThreadStdoutWriteCallback stdout_callback;

void upython_reset_file_path();

Action upython_splash_screen();
bool upython_confirm_exit_action();
bool upython_select_python_file(FuriString* file_path);

void upython_cli_register(void* args);
void upython_cli_unregister(void* args);

void upython_cli(PipeSide* pipe, FuriString* args, void* ctx);

void upython_repl_execute();

void upython_file_execute(FuriString* file);

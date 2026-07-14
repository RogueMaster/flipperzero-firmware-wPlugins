#pragma once

#include "badusb_pro.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialise the script engine to its default state.
 */
void script_engine_init(ScriptEngine* engine);

/**
 * Load parsed tokens into the engine for execution.
 * Takes ownership of the dynamically allocated token array.
 * Scans for FUNCTION/END_FUNCTION blocks and registers them.
 *
 * @param engine    engine context
 * @param tokens    dynamically allocated token array (engine takes ownership)
 * @param count     number of tokens in use
 * @param capacity  allocated capacity of the tokens array
 */
void script_engine_load(
    ScriptEngine* engine,
    ScriptToken* tokens,
    uint32_t count,
    uint32_t capacity);

/**
 * Run the loaded script to completion (or until paused / error).
 * This is intended to be called from a worker thread.
 * The function returns when the script finishes, is paused, or hits an error.
 *
 * @param engine  engine context
 */
void script_engine_run(ScriptEngine* engine);

/**
 * Pause a running script.  The engine will stop after the current command.
 */
void script_engine_pause(ScriptEngine* engine);

/**
 * Resume a paused script.
 */
void script_engine_resume(ScriptEngine* engine);

/**
 * Abort a running or paused script.
 */
void script_engine_stop(ScriptEngine* engine);

/**
 * Set a callback that is invoked after each command to update the UI.
 */
void script_engine_set_callback(ScriptEngine* engine, void (*callback)(void* ctx), void* ctx);

/**
 * Set the execution speed multiplier (0.5, 1.0, 2.0, 4.0).
 */
void script_engine_set_speed(ScriptEngine* engine, float multiplier);

/**
 * Look up a variable by name.  Returns pointer to value or NULL.
 */
const char* script_engine_get_var(ScriptEngine* engine, const char* name);

/**
 * Set / create a variable.
 */
bool script_engine_set_var(ScriptEngine* engine, const char* name, const char* value);

#ifdef __cplusplus
}
#endif

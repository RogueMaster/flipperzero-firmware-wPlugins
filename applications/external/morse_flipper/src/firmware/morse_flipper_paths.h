/*
 * Purpose: Centralise storage paths for firmware and host test builds.
 * Owns: state, custom character, session log, and ham keyer log paths.
 * Depends on: Flipper APP_DATA_PATH/EXT_PATH in FAP builds.
 * Tests: tests/test_trainer_files.c.
 */

#ifndef yo3gnd_paths_12039f
#define yo3gnd_paths_12039f

#ifdef MORSE_FLIPPER_FAP
#include <storage/storage.h>
#define MORSE_FLIPPER_STATE_PATH           APP_DATA_PATH("morse_flipper.state")
#define MORSE_FLIPPER_CUSTOM_CHARS_PATH    EXT_PATH("ham/flipper-cw-custom-characters.txt")
#define MORSE_FLIPPER_SESSION_LOG_PATH     EXT_PATH("ham/morse-flipper-session-log.txt")
#define MORSE_FLIPPER_HAM_KEYER_LOG_PREFIX EXT_PATH("ham/morse-flipper-ham-keyer-")
#define MORSE_FLIPPER_HAM_DIR              EXT_PATH("ham")
#else
#define MORSE_FLIPPER_STATE_PATH           "morse_flipper.state"
#define MORSE_FLIPPER_CUSTOM_CHARS_PATH    "ext/ham/flipper-cw-custom-characters.txt"
#define MORSE_FLIPPER_SESSION_LOG_PATH     "ext/ham/morse-flipper-session-log.txt"
#define MORSE_FLIPPER_HAM_KEYER_LOG_PREFIX "ext/ham/morse-flipper-ham-keyer-"
#define MORSE_FLIPPER_HAM_DIR              "ext/ham"
#endif

#endif

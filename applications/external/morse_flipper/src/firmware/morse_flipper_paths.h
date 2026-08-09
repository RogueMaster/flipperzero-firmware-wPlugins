/*
 * Purpose: Centralise storage paths for firmware and host test builds.
 * Owns: state, custom character, session log, and ham keyer log paths.
 * Depends on: Flipper APP_DATA_PATH in FAP builds.
 * Tests: tests/test_trainer_files.c.
 */

#ifndef yo3gnd_paths_12039f
#define yo3gnd_paths_12039f

#ifdef MORSE_FLIPPER_FAP
#include <storage/storage.h>
#define MORSE_FLIPPER_APP_DATA_DIR         EXT_PATH("apps_data/morse_flipper")
#define MORSE_FLIPPER_STATE_PATH           APP_DATA_PATH("morse_flipper.state")
#define MORSE_FLIPPER_ONBOARDING_PATH      APP_DATA_PATH("onboarding.flag")
#define MORSE_FLIPPER_PROGRESS_PATH        APP_DATA_PATH("progress.bin")
#define MORSE_FLIPPER_PROGRESS_HISTORY_DIR APP_DATA_PATH("history")
#define MORSE_FLIPPER_CUSTOM_CHARS_PATH    APP_DATA_PATH("flipper-cw-custom-characters.txt")
#define MORSE_FLIPPER_SESSION_LOG_PATH     APP_DATA_PATH("morse-flipper-session-log.txt")
#define MORSE_FLIPPER_HAM_KEYER_LOG_PREFIX APP_DATA_PATH("morse-flipper-ham-keyer-")
#else
#define MORSE_FLIPPER_APP_DATA_DIR         "ext/apps_data/morse_flipper"
#define MORSE_FLIPPER_STATE_PATH           "morse_flipper.state"
#define MORSE_FLIPPER_ONBOARDING_PATH      MORSE_FLIPPER_APP_DATA_DIR "/onboarding.flag"
#define MORSE_FLIPPER_PROGRESS_PATH        MORSE_FLIPPER_APP_DATA_DIR "/progress.bin"
#define MORSE_FLIPPER_PROGRESS_HISTORY_DIR MORSE_FLIPPER_APP_DATA_DIR "/history"
#define MORSE_FLIPPER_CUSTOM_CHARS_PATH \
    MORSE_FLIPPER_APP_DATA_DIR "/flipper-cw-custom-characters.txt"
#define MORSE_FLIPPER_SESSION_LOG_PATH     MORSE_FLIPPER_APP_DATA_DIR "/morse-flipper-session-log.txt"
#define MORSE_FLIPPER_HAM_KEYER_LOG_PREFIX MORSE_FLIPPER_APP_DATA_DIR "/morse-flipper-ham-keyer-"
#endif

#endif

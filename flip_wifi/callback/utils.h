#pragma once

// Below added by Derek Jamison
// FURI_LOG_DEV will log only during app development. Be sure that Settings/System/Log Device is "LPUART"; so we dont use serial port.
#ifdef DEVELOPMENT
#define FURI_LOG_DEV(tag, format, ...) furi_log_print_format(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define DEV_CRASH() furi_crash()
#else
#define FURI_LOG_DEV(tag, format, ...)
#define DEV_CRASH()
#endif

typedef enum MessageState MessageState;
enum MessageState
{
    MessageStateAbout,        // The about screen
    MessageStateLoading,      // The loading screen (for game)
    MessageStateWaitingLobby, // The waiting lobby screen
};
typedef struct MessageModel MessageModel;
struct MessageModel
{
    MessageState message_state;
};
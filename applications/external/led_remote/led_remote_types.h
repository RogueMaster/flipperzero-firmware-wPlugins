#pragma once

#define LED_REMOTE_MAX_BUTTONS  24
#define LED_REMOTE_MAX_PROFILES 20

typedef uint8_t LedButton;

typedef enum {
    LedRemoteType18Keys = 0,
    LedRemoteType24Keys,
    LedRemoteTypeCount,
} LedRemoteType;

typedef enum {
    LedRemoteViewIdSubmenu,
    LedRemoteViewIdRemote,
    LedRemoteViewIdPopup,
    LedRemoteViewIdTextInput,
} LedRemoteViewId;

typedef enum {
    LedRemoteLayoutCircle = 0,
    LedRemoteLayoutGrid,
    LedRemoteLayoutCount,
} LedRemoteLayout;

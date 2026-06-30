// Just a temporary polyfill until this cli/cli_ansi.h spread across all the FW and brances
#pragma once

typedef enum {
    CliKeyUnrecognized = 0,

    CliKeySOH = 0x01,
    CliKeyETX = 0x03,
    CliKeyEOT = 0x04,
    CliKeyBell = 0x07,
    CliKeyBackspace = 0x08,
    CliKeyTab = 0x09,
    CliKeyLF = 0x0A,
    CliKeyCR = 0x0D,
    CliKeyETB = 0x17,
    CliKeyEsc = 0x1B,
    CliKeyUS = 0x1F,
    CliKeySpace = 0x20,
    CliKeyDEL = 0x7F,

    CliKeySpecial = 0x80,
    CliKeyLeft,
    CliKeyRight,
    CliKeyUp,
    CliKeyDown,
    CliKeyHome,
    CliKeyEnd,
} CliKey;

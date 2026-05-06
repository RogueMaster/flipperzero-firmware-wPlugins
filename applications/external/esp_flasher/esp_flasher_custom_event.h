#pragma once

typedef enum {
    EspFlasherEventRefreshConsoleOutput = 0,
    EspFlasherEventStartConsole,
    EspFlasherEventStartKeyboard,
    EspFlasherEventStartFlasher,
    EspFlasherEventRefreshSubmenu,
    EspFlasherEventShowAddressInput, // show hex address text input for pending slot
    EspFlasherEventAddrInputDone,    // address text input confirmed
    EspFlasherEventShowPartConfirm,  // show partition address confirm widget
    EspFlasherEventPartConfirmYes,   // user accepted parsed partition addresses
    EspFlasherEventPartConfirmNo,    // user declined parsed partition addresses
} EspFlasherCustomEvent;

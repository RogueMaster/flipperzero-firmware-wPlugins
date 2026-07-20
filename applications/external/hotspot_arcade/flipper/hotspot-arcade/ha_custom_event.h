#pragma once

// Custom ViewDispatcher events. Kept above submenu item indices (0..N) so the
// two ranges never collide when routed to a scene's on_event.
typedef enum {
    HaEventSsidDone = 100,
    // Posted by the UART worker when bytes arrive; drained + parsed on the GUI
    // thread in hotspot_arcade.c, then a refresh is dispatched to the top scene.
    HaEventRxData = 101,
    HaEventRefreshView = 102,
    // Start flow: paint a status screen, then run the blocking step next loop.
    HaEventDetectBoard = 103,
    HaEventBeginStart = 104,
    // Lobby host actions.
    HaEventPickGame = 105,
    HaEventShowLeaderboard = 106,
    HaEventShowConsole = 107,
    HaEventHostGame = 110, // enter the active game's host screen from the dashboard
    // Flash Firmware (ESP-serial-flasher): start (board detected in download mode),
    // per-block progress, done.
    HaEventFlashStart = 111,
    HaEventFlashProgress = 112,
    HaEventFlashDone = 113,
    // From the lobby "No board" prompt: install firmware, then continue where headed.
    HaEventInstallFirmware = 114,
    HaEventFlashContinue = 115,
} HaCustomEvent;

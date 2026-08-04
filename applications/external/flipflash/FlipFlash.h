#ifndef FLIPFLASH_H
#define FLIPFLASH_H

#include <furi.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "FlipFlash"
#define APP_DATA_DIR "/ext/apps_data/FlipFlash"
#define DECK_FILE_PATH "/ext/apps_data/FlipFlash/spanish_words.txt"
#define STATE_FILE_PATH "/ext/apps_data/FlipFlash/flash_state.txt"

#define MAX_CARDS 128
#define MAX_TEXT_LENGTH 128
#define MAX_LINE_LENGTH 256

#define MAX_DECKS 16
#define MAX_PATH_LENGTH 192
#define SELECTED_DECK_FILE_PATH APP_DATA_DIR "/selected_deck.cfg"

#define HELP_LINE_HEIGHT 12

#define SETTINGS_VISIBLE_ITEMS 4

#endif // FLIPFLASH_H
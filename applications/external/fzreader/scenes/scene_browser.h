#pragma once

#include <stdbool.h>

#include <furi.h>

/**
 * Fill `folder` with the folder of the book at `book`: the browser starts there,
 * so another book next to the current one is one screen away. An unknown book
 * starts the browser at the root of the SD card.
 */
void scene_browser_folder(FuriString* folder, const char* book);

/** Show the system file browser filtered by EPUB files. @returns true if a file was selected. */
bool scene_browser_pick(FuriString* path, const FuriString* folder);

/** Show a modal message with a single button. */
void scene_browser_show_error(const char* header, const char* text);

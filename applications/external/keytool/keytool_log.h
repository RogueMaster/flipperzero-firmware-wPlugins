#pragma once

#include "keytool_i.h"

#define KEYTOOL_LOGS_FOLDER KEYTOOL_APP_FOLDER "/logs"

// Allocates the custom "saved logs" list view: shows every auto-saved
// scan as "<card name> - <date>", newest first.
//   UP/DOWN  move selection
//   OK       open the selected log (same TextBox used elsewhere)
//   LEFT     ask for confirmation, then delete the selected log
//   BACK     return to the main menu
View* keytool_log_list_alloc(KeyToolApp* app);
void keytool_log_list_free(View* view);

// Rescans KEYTOOL_LOGS_FOLDER and rebuilds the list (card name / date
// are read from the "Card: " / "Date: " header lines of each file).
// Call this whenever the Log screen is (re)entered and after a delete.
void keytool_log_list_refresh(KeyToolApp* app);

// Current RTC time as "DD/MM/YYYY HH:MM" (display_out) and as a
// sortable, filename-safe "YYYYMMDD_HHMMSS" prefix (prefix_out).
void keytool_log_datetime_now(FuriString* display_out, FuriString* prefix_out);

// Saves one scan's result under KEYTOOL_LOGS_FOLDER as
// "<filename_prefix>_<sanitized card name>.txt", with "Card: " / "Date: "
// header lines followed by a blank line and then `body` verbatim.
void keytool_log_save_entry(
    Storage* storage,
    const char* card_name,
    const char* date_display,
    const char* filename_prefix,
    FuriString* body);

// Reads one saved log file in full into `out` (replacing its content).
void keytool_log_read_file_to_string(Storage* storage, const char* path, FuriString* out);

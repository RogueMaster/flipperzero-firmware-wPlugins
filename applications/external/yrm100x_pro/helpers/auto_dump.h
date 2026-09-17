#pragma once
#include "../structures.h"

#define UHF_AUTO_DUMP_DIR APP_DATA_PATH("dumps")

// Save one tag to a unique standalone .uhf dump and to the normal Saved list.
// Returns true only when the standalone full-dump file was created.
bool uhf_auto_dump_save_tag(UHFReaderApp* App, UHFTag* tag);

// Save every tag currently present in the YRM wrapper.
// Each tag gets a separate .uhf file with a unique filename and is also
// appended to the application's normal Saved list.
size_t uhf_auto_dump_save_all(UHFReaderApp* App);

// Delete both the separate auto-dump directory and the normal Saved database.
// The Saved submenu is reset immediately.
bool uhf_delete_all_saved_data(UHFReaderApp* App);

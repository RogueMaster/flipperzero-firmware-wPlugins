#pragma once

#include "../models/session.h"
#include <storage/storage.h>

/* Export the session as JSON to /ext/apps_data/breach_map/exports/<name>.json.
 * On success, "out_path" (if provided) receives the written path. */
bool report_export_json(Storage* storage, const Session* session, FuriString* out_path);

/* Export the session as a Markdown report (Phase 2). */
bool report_export_markdown(Storage* storage, const Session* session, FuriString* out_path);

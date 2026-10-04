#pragma once

#include "archive_files.h"

/* APP_ARCHIVE_BROWSER queues a handoff; the resident helper starts directly. */
bool archive_launch_selected(const ArchiveFile_t* selected, bool favorites);

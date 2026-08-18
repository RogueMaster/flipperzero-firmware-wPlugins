#pragma once

#include "file_browser_worker.h"

// Internal APIs used only by FileBrowser and Archive in firmware binary

const char* file_browser_worker_get_path_current(BrowserWorker* browser);

const char* file_browser_worker_get_filter_ext(BrowserWorker* browser);

void file_browser_worker_set_filter_ext(
    BrowserWorker* browser,
    FuriString* path,
    const char* filter_ext);

void file_browser_worker_folder_refresh_sel(BrowserWorker* browser, const char* item_name);

#ifndef CSV_URL_H
#define CSV_URL_H

#include <furi.h>
#include <storage/storage.h>
#include "../../structs.h"
#include "../../app.h"

bool sync_csv_get_url_to_mem(App* app);
bool write_url_to_csv(App* app, const char* url);
bool read_urls_from_csv(App* app);
bool delete_url_from_csv(App* app, const char* url);
bool init_csv_get_url(App* app);

#endif // CSV_URL_H

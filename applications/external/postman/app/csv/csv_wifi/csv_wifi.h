#ifndef CSV_H
#define CSV_H

#include "../../app.h"
#include "../../structs.h"
#include <furi.h>
#include <storage/storage.h>

bool init_csv_wifi(App* app);
bool write_wifi_to_csv(App* app, const WifiCredential* wifi);
bool read_wifis_from_csv(App* app);
bool delete_wifi_from_csv(App* app, const char* ssid);
bool sync_csv_to_mem(App* app);

#endif // CSV_H

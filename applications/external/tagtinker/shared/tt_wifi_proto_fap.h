/*
 * FAP-side stub that pulls in the shared wire-protocol header. The ESP-IDF
 * project keeps the canonical copy under esp32-wifi-fw/shared/; this file
 * just re-exports it so FAP sources can `#include "../shared/tt_wifi_proto_fap.h"`
 * without escaping the FAP project root.
 *
 * If you ever need to edit the protocol, edit
 *   esp32-wifi-fw/shared/tt_wifi_proto.h
 * and run the sync script (or copy by hand) to update the body of this file.
 */
#ifndef TT_WIFI_PROTO_FAP_H
#define TT_WIFI_PROTO_FAP_H

#include "../esp32_wifi_fw/shared/tt_wifi_proto.h"

#endif

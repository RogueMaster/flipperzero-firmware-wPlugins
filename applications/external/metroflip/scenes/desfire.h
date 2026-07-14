#ifndef DESFIRE_H
#define DESFIRE_H

#include "../metroflip_i.h"
#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>

const char* desfire_type(const MfDesfireData* data);
bool is_desfire_locked(const char* card_name);
#endif // DESFIRE_H

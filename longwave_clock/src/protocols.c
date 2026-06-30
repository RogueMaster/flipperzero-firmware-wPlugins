#include "protocols.h"
#include "storage/storage.h"

static char* protocol_names[] = {"DCF77 (DE, 77.5kHz)", "MSF (UK, 60.0kHz)"};
static char* protocol_config[] = {APP_DATA_PATH("dcf77.conf"), APP_DATA_PATH("msf.conf")};

const char* get_protocol_name(LWCType type) {
    return protocol_names[type];
}

const char* get_protocol_config_filename(LWCType type) {
    return protocol_config[type];
}

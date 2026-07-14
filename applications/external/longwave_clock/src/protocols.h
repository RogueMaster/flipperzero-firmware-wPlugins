#ifndef PROTOCOL_HEADERS
#define PROTOCOL_HEADERS

typedef enum {
    DCF77,
    MSF,
    __lwc_number_of_protocols
} LWCType;

const char* get_protocol_name(LWCType type);
const char* get_protocol_config_filename(LWCType type);

#endif

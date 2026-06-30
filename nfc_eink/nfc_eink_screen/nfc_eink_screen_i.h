#include "nfc_eink_screen.h"

typedef struct {
    uint8_t* image_data;
    uint16_t image_size;
    NfcEinkScreenInfo base;
} NfcEinkScreenData;

typedef void NfcEinkScreenSpecificContext;

typedef struct {
    NfcDevice* nfc_device;
    uint16_t block_total;
    uint16_t block_current;
    uint16_t received_data;
    NfcEinkScreenSpecificContext* screen_context;
} NfcEinkScreenDevice;

typedef NfcEinkScreenDevice* (*EinkScreenAllocCallback)();
typedef void (*EinkScreenFreeCallback)(NfcEinkScreenDevice* instance);

typedef struct {
    EinkScreenAllocCallback alloc;
    EinkScreenFreeCallback free;
    NfcGenericCallback listener_callback;
    NfcGenericCallback poller_callback;
} NfcEinkScreenHandlers;

struct NfcEinkScreen {
    NfcEinkScreenData* data;
    NfcEinkScreenDevice* device;
    FuriString* name;
    NfcEinkScreenError error;
    BitBuffer* tx_buf;
    BitBuffer* rx_buf;
    const NfcEinkScreenHandlers* handlers;
    NfcEinkScreenEventCallback event_callback;
    NfcEinkScreenEventContext event_context;
};

void nfc_eink_screen_vendor_callback(NfcEinkScreen* instance, NfcEinkScreenEventType type);
void nfc_eink_screen_set_error(NfcEinkScreen* instance, NfcEinkScreenError error);
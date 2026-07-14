#pragma once

#include <furi.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>

#include "nfc_eink_types.h"

typedef struct NfcEinkScreen NfcEinkScreen;

const char* nfc_eink_screen_get_manufacturer_name(NfcEinkManufacturer type);

NfcEinkScreen* nfc_eink_screen_alloc(NfcEinkManufacturer manufacturer);
void nfc_eink_screen_init(NfcEinkScreen* screen, const char* name);
void nfc_eink_screen_free(NfcEinkScreen* screen);

void nfc_eink_screen_set_callback(
    NfcEinkScreen* screen,
    NfcEinkScreenEventCallback event_callback,
    NfcEinkScreenEventContext event_context);

NfcDevice* nfc_eink_screen_get_nfc_device(const NfcEinkScreen* screen);
NfcGenericCallback nfc_eink_screen_get_nfc_callback(const NfcEinkScreen* screen, NfcMode mode);

const NfcEinkScreenInfo* nfc_eink_screen_get_image_info(const NfcEinkScreen* screen);
const uint8_t* nfc_eink_screen_get_image_data(const NfcEinkScreen* screen);
uint16_t nfc_eink_screen_get_image_size(const NfcEinkScreen* screen);
uint16_t nfc_eink_screen_get_received_size(const NfcEinkScreen* screen);

void nfc_eink_screen_get_progress(const NfcEinkScreen* screen, size_t* current, size_t* total);
const char* nfc_eink_screen_get_name(const NfcEinkScreen* screen);
NfcEinkScreenError nfc_eink_screen_get_error(const NfcEinkScreen* screen);

bool nfc_eink_screen_save(const NfcEinkScreen* screen, const char* file_path);
bool nfc_eink_screen_delete(const char* file_path);
bool nfc_eink_screen_load_info(const char* file_path, const NfcEinkScreenInfo** info);
bool nfc_eink_screen_load_data(
    const char* file_path,
    NfcEinkScreen* destination,
    const NfcEinkScreenInfo* source_info);

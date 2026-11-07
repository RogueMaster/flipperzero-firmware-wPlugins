// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT

#pragma once

#include <furi_hal.h>
#include <storage/storage.h>

#define VF_LAYOUT_NAME_MAX 24

typedef struct {
    uint16_t layout[128];
    char name[VF_LAYOUT_NAME_MAX + 1];
    char dead[4]; /* dead keys of the layout: followed by a space */
    bool active;
    uint16_t skipped; /* characters vf_hid_type could not type since the last reset */
    FuriHalUsbInterface* prev;
} VfHid;

/* Reset to the built-in US layout ("en-US"). */
void vf_hid_init(VfHid* hid);

/* Load a layout: app assets (/ext/apps_assets/vaultflipper/layouts) first,
 * then BadUSB (/ext/badusb/assets/layouts). false = unchanged. */
bool vf_hid_load_layout(VfHid* hid, Storage* storage, const char* name);

/* Switch USB to keyboard mode (non-blocking). Stays until vf_hid_end(). */
bool vf_hid_start(VfHid* hid);
/* Keyboard mode on and enumerated by a host? */
bool vf_hid_connected(const VfHid* hid);
/* Start if needed and wait up to 3 s for a host. false = no host. */
bool vf_hid_ready(VfHid* hid);
/* true for ISO layouts (key right of left Shift exists), false for ANSI. */
bool vf_hid_is_iso(const VfHid* hid);
/* Type text with the loaded layout; characters it lacks are counted in skipped. */
void vf_hid_type(VfHid* hid, const char* text);
void vf_hid_key(VfHid* hid, uint16_t key);
/* Restore the previous USB mode (usually serial). */
void vf_hid_end(VfHid* hid);

/* Called once per layout name found (without .kl, duplicates removed). */
typedef void (*VfLayoutCallback)(const char* name, void* ctx);
void vf_hid_list_layouts(Storage* storage, VfLayoutCallback cb, void* ctx);

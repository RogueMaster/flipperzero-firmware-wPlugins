#include "../../include/nfc_tools_i.h"

// ── Standard ISO ──────────────────────────────────────────────────────────────

static const char* nfc_tools_iso_standard(NfcProtocol proto) {
    switch(proto) {
    case NfcProtocolIso14443_3a:
    case NfcProtocolMfUltralight:
    case NfcProtocolMfClassic:
    case NfcProtocolMfPlus:
        return "ISO 14443-3A";
    case NfcProtocolIso14443_3b:
    case NfcProtocolSt25tb:
        return "ISO 14443-3B";
    case NfcProtocolIso14443_4a:
    case NfcProtocolMfDesfire:
        return "ISO 14443-4A";
    case NfcProtocolIso14443_4b:
        return "ISO 14443-4B";
    case NfcProtocolIso15693_3:
    case NfcProtocolSlix:
        return "ISO 15693-3";
    case NfcProtocolFelica:
        return "ISO 18092 (NFC-F)";
    default:
        return NULL;
    }
}

// ── Type NFC Forum ────────────────────────────────────────────────────────────

static const char* nfc_tools_nfc_type(NfcProtocol proto) {
    switch(proto) {
    case NfcProtocolIso14443_3a:
    case NfcProtocolIso14443_4a:
    case NfcProtocolMfUltralight:
    case NfcProtocolMfClassic:
    case NfcProtocolMfPlus:
    case NfcProtocolMfDesfire:
        return "Type A";
    case NfcProtocolIso14443_3b:
    case NfcProtocolIso14443_4b:
    case NfcProtocolSt25tb:
        return "Type B";
    case NfcProtocolIso15693_3:
    case NfcProtocolSlix:
        return "Type V";
    case NfcProtocolFelica:
        return "Type F";
    default:
        return NULL;
    }
}

// ── Chip name ─────────────────────────────────────────────────────────────────

static void nfc_tools_append_chip_name(NfcToolsApp* app, FuriString* s) {
    if(app->detected_protocol == NfcProtocolMfUltralight) {
        // If GET_VERSION (0x60) replied with prod_type=0x04 (NTAG21x family without I2C),
        // use the version bytes for precise identification.
        // prod_subtype == 5 indicates NTAG I2C → let the SDK (mful_type) handle it.
        bool version_identified = false;
        if(app->mful_version_valid &&
           app->mful_version.prod_type == 0x04 &&
           app->mful_version.prod_subtype != 5) {
            version_identified = true;
            switch(app->mful_version.storage_size) {
            case 0x0B:
                // NTAG210 (NTAG210μ does not respond to GET_VERSION → indistinguishable)
                furi_string_cat_str(s, NTS_CHIP_NTAG210);
                break;
            case 0x0E:
                furi_string_cat_str(s, NTS_CHIP_NTAG212);
                break;
            case 0x0F:
                // NTAG213 TT : prod_ver_major == 0x03 (vs 0x01 pour NTAG213 standard)
                if(app->mful_version.prod_ver_major == 0x03)
                    furi_string_cat_str(s, NTS_CHIP_NTAG213_TT);
                else
                    furi_string_cat_str(s, NTS_CHIP_NTAG213);
                break;
            case 0x11:
                furi_string_cat_str(s, NTS_CHIP_NTAG215);
                break;
            case 0x13:
                furi_string_cat_str(s, NTS_CHIP_NTAG216);
                break;
            default:
                version_identified = false; // unknown storage_size → fallback
                break;
            }
        }

        if(!version_identified) {
            // Fallback: identification via SDK type (NTAG203, MF UL EV1, NTAG I2C, ...)
            switch(app->mful_type) {
            case MfUltralightTypeOrigin:         furi_string_cat_str(s, NTS_CHIP_MF_ULTRALIGHT);         break;
            case MfUltralightTypeNTAG203:        furi_string_cat_str(s, NTS_CHIP_NTAG203);               break;
            case MfUltralightTypeMfulC:          furi_string_cat_str(s, NTS_CHIP_MF_ULTRALIGHT_C);       break;
            case MfUltralightTypeUL11:
            case MfUltralightTypeUL21:           furi_string_cat_str(s, NTS_CHIP_MF_ULTRALIGHT_EV1);     break;
            case MfUltralightTypeNTAG213:        furi_string_cat_str(s, NTS_CHIP_NTAG213);               break;
            case MfUltralightTypeNTAG215:        furi_string_cat_str(s, NTS_CHIP_NTAG215);               break;
            case MfUltralightTypeNTAG216:        furi_string_cat_str(s, NTS_CHIP_NTAG216);               break;
            case MfUltralightTypeNTAGI2C1K:      furi_string_cat_str(s, NTS_CHIP_NTAG_I2C_1K);          break;
            case MfUltralightTypeNTAGI2C2K:      furi_string_cat_str(s, NTS_CHIP_NTAG_I2C_2K);          break;
            case MfUltralightTypeNTAGI2CPlus1K:  furi_string_cat_str(s, NTS_CHIP_NTAG_I2C_PLUS_1K);     break;
            case MfUltralightTypeNTAGI2CPlus2K:  furi_string_cat_str(s, NTS_CHIP_NTAG_I2C_PLUS_2K);     break;
            default:                             furi_string_cat_str(s, NTS_CHIP_MF_ULTRALIGHT);         break;
            }
        }
    } else if(app->detected_protocol == NfcProtocolMfClassic) {
        switch(app->mfc_type) {
        case MfClassicType1k:    furi_string_cat_str(s, NTS_CHIP_MIFARE_CLASSIC_1K); break;
        case MfClassicType4k:    furi_string_cat_str(s, NTS_CHIP_MIFARE_CLASSIC_4K); break;
        case MfClassicTypeMini:  furi_string_cat_str(s, NTS_CHIP_MIFARE_CLASSIC_MINI); break;
        default:                 furi_string_cat_str(s, NTS_CHIP_MIFARE_CLASSIC); break;
        }
    } else if(app->detected_protocol == NfcProtocolMfDesfire) {
        const char* ev;
        switch(app->desfire_hw_major) {
        case 0x01: ev = "EV1";    break;
        case 0x12: ev = "EV2";    break;
        case 0x22: ev = "EV2 XL"; break;
        case 0x33: ev = "EV3";    break;
        default:   ev = NULL;     break;
        }
        if(ev) furi_string_cat_printf(s, "NXP MF DESFire %s", ev);
        else   furi_string_cat_str(s, "NXP MF DESFire");
    } else if(app->detected_protocol == NfcProtocolIso15693_3 ||
              app->detected_protocol == NfcProtocolSlix) {
        if(app->uid_len >= 4 && app->uid[1] == 0x04) {
            // NXP ICODE: precise model identification
            if(app->detected_protocol == NfcProtocolSlix) {
                // SLIX family: uid[2] = icode_type, uid[3] bits[4:3] = type_indicator
                // (same logic as slix_get_type() in the SDK)
                uint8_t icode_type     = app->uid[2];
                uint8_t type_indicator = (app->uid[3] >> 3) & 0x03;
                if(icode_type == 0x01) {
                    // 0x01 = SLIX or SLIX2 depending on type_indicator
                    if(type_indicator == 0x01)
                        furi_string_cat_str(s, NTS_CHIP_NXP_ICODE_SLIX2);
                    else
                        furi_string_cat_str(s, NTS_CHIP_NXP_ICODE_SLIX);
                } else if(icode_type == 0x02) {
                    furi_string_cat_str(s, NTS_CHIP_NXP_ICODE_SLIX_S);
                } else if(icode_type == 0x03) {
                    furi_string_cat_str(s, NTS_CHIP_NXP_ICODE_SLIX_L);
                } else {
                    furi_string_cat_str(s, NTS_CHIP_NXP_ICODE);
                }
            } else {
                // SLI family (standard ISO 15693, without SLIX extensions)
                furi_string_cat_str(s, NTS_CHIP_NXP_ICODE_SLI);
            }
        } else {
            furi_string_cat_str(s, NTS_CHIP_ISO15693_TAG);
        }
    } else if(app->detected_protocol == NfcProtocolFelica) {
        if(app->felica_ic_name[0]) furi_string_cat_str(s, app->felica_ic_name);
        else                       furi_string_cat_str(s, NTS_CHIP_FELICA);
    } else {
        const char* proto_name = nfc_device_get_protocol_name(app->detected_protocol);
        furi_string_cat_str(s, proto_name ? proto_name : "Unknown");
    }
}

// ── Memory line ───────────────────────────────────────────────────────────────

static void nfc_tools_append_memory(NfcToolsApp* app, FuriString* s) {
    if(app->detected_protocol == NfcProtocolMfUltralight) {
        uint16_t pages = mf_ultralight_get_pages_total(app->mful_type);
        uint32_t bytes = (uint32_t)pages * MF_ULTRALIGHT_PAGE_SIZE;
        furi_string_cat_printf(s,
            "Memory: %lu bytes\n%u pages (4 bytes each)\n",
            (unsigned long)bytes, (unsigned)pages);
    } else if(app->detected_protocol == NfcProtocolMfClassic) {
        uint16_t blocks;
        switch(app->mfc_type) {
        case MfClassicType1k:   blocks = 64;  break;
        case MfClassicType4k:   blocks = 256; break;
        case MfClassicTypeMini: blocks = 20;  break;
        default:                blocks = 0;   break;
        }
        if(blocks > 0) {
            furi_string_cat_printf(s,
                "Memory: %lu bytes : %u blocks (16 bytes each)\n",
                (unsigned long)blocks * 16, (unsigned)blocks);
        }
    } else if(app->detected_protocol == NfcProtocolMfDesfire) {
        if(app->desfire_has_free_memory) {
            furi_string_cat_printf(s,
                "Memory free: %lu bytes\n",
                (unsigned long)app->desfire_free_memory);
        }
    } else if(app->detected_protocol == NfcProtocolIso15693_3 ||
              app->detected_protocol == NfcProtocolSlix) {
        if(app->iso15693_block_count > 0 && app->iso15693_block_size > 0) {
            uint32_t bytes =
                (uint32_t)app->iso15693_block_count * app->iso15693_block_size;
            furi_string_cat_printf(s,
                "Memory: %lu bytes : %u blocks (%u bytes each)\n",
                (unsigned long)bytes,
                (unsigned)app->iso15693_block_count,
                (unsigned)app->iso15693_block_size);
        }
    } else if(app->detected_protocol == NfcProtocolFelica) {
        if(app->felica_blocks_total > 0) {
            uint32_t bytes = (uint32_t)app->felica_blocks_total * 16;
            furi_string_cat_printf(s,
                "Memory: %lu bytes\n%u blocks (16 bytes each)\n",
                (unsigned long)bytes,
                (unsigned)app->felica_blocks_total);
        }
    }
}

// ── Chip info text construction ───────────────────────────────────────────────

static void build_chip_info(NfcToolsApp* app) {
    FuriString* s = app->info_str;
    furi_string_reset(s);

    furi_string_cat_str(s, NTS_LBL_CHIP);
    nfc_tools_append_chip_name(app, s);
    furi_string_cat_str(s, "\n");

    const char* iso = nfc_tools_iso_standard(app->detected_protocol);
    if(iso) furi_string_cat_printf(s, "Standard: %s\n", iso);

    const char* nfc_type = nfc_tools_nfc_type(app->detected_protocol);
    if(nfc_type) furi_string_cat_printf(s, "Type: %s\n", nfc_type);

    if(app->uid_len > 0) {
        furi_string_cat_str(s, NTS_LBL_UID);
        for(size_t i = 0; i < app->uid_len; i++) {
            if(i > 0) furi_string_cat_str(s, ":");
            furi_string_cat_printf(s, "%02X", app->uid[i]);
        }
        furi_string_cat_str(s, "\n");
    }

    bool is_iso15693 = (app->detected_protocol == NfcProtocolIso15693_3 ||
                        app->detected_protocol == NfcProtocolSlix);
    bool is_desfire  = (app->detected_protocol == NfcProtocolMfDesfire);
    bool is_felica   = (app->detected_protocol == NfcProtocolFelica);

    if(app->uid_len > 0 && !is_iso15693 && !is_desfire && !is_felica) {
        furi_string_cat_printf(s, "ATQA: 0x%02X%02X\n", app->atqa[0], app->atqa[1]);
        furi_string_cat_printf(s, "SAK: 0x%02X\n", app->sak);
    }

    if(is_desfire) {
        furi_string_cat_printf(s,
            "Applications: %lu\n", (unsigned long)app->desfire_app_count);
    }

    if(is_felica) {
        furi_string_cat_str(s, "PMm: ");
        for(uint8_t i = 0; i < FELICA_PMM_SIZE; i++) {
            if(i > 0) furi_string_cat_str(s, ":");
            furi_string_cat_printf(s, "%02X", app->felica_pmm[i]);
        }
        furi_string_cat_str(s, "\n");
    }

    nfc_tools_append_memory(app, s);
}

// ── Short NDEF type label ─────────────────────────────────────────────────────

static const char* nfc_tools_ndef_type_short(NfcToolsNdefType t) {
    switch(t) {
    case NfcToolsNdefTypeUri:         return "URL";
    case NfcToolsNdefTypeText:        return "Text";
    case NfcToolsNdefTypeWifi:        return "WiFi";
    case NfcToolsNdefTypeVcard:       return "vCard";
    case NfcToolsNdefTypeSmartPoster: return "Smart Poster";
    case NfcToolsNdefTypeMime:        return "MIME";
    case NfcToolsNdefTypeEmpty:       return "Empty";
    case NfcToolsNdefTypeExternal:    return "External";
    default:                          return "Unknown";
    }
}

// ── Callbacks submenu ─────────────────────────────────────────────────────────

static void nfc_tools_tag_info_submenu_cb(void* context, uint32_t index) {
    NfcToolsApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, (int32_t)index);
}

// No-op callback for non-selectable items
static void nfc_tools_tag_info_noop_cb(void* context, uint32_t index) {
    UNUSED(context);
    UNUSED(index);
}

// ── Persistent buffers for submenu labels ─────────────────────────────────────

static char s_chip_header[48];
static char s_record_labels[NFC_TOOLS_MAX_NDEF_RECORDS][32];

// ── Scene ─────────────────────────────────────────────────────────────────────

void nfc_tools_scene_tag_info_on_enter(void* context) {
    NfcToolsApp* app = context;

    build_chip_info(app);

    // Chip name for the header
    FuriString* tmp = furi_string_alloc();
    nfc_tools_append_chip_name(app, tmp);
    strlcpy(s_chip_header, furi_string_get_cstr(tmp), sizeof(s_chip_header));
    furi_string_free(tmp);

    // Always display the submenu (with or without records)
    Submenu* sub = app->submenu2;
    submenu_reset(sub);
    submenu_set_header(sub, s_chip_header);

    // Item 0: chip info
    submenu_add_item(sub, NTS_LBL_CHIP_INFO, 0,
                     nfc_tools_tag_info_submenu_cb, app);

    bool is_felica = (app->detected_protocol == NfcProtocolFelica);

    if(is_felica) {
        // FeliCa: no NDEF records, offer memory view
        submenu_add_item(sub, NTS_LBL_VIEW_BLOCKS, 100,
                         nfc_tools_tag_info_submenu_cb, app);
    } else if(app->ndef_record_count == 0) {
        // No NDEF records: non-selectable item
        submenu_add_item(sub, NTS_LBL_NO_RECORDS, UINT32_MAX,
                         nfc_tools_tag_info_noop_cb, app);
    } else {
        // Items 1..N : "Record 01: URL", "Record 02: Text", …
        for(uint8_t i = 0; i < app->ndef_record_count; i++) {
            snprintf(s_record_labels[i], sizeof(s_record_labels[i]),
                     "Record %02u: %s",
                     (unsigned)(i + 1),
                     nfc_tools_ndef_type_short(app->ndef_records[i].type));
            submenu_add_item(sub,
                s_record_labels[i],
                (uint32_t)(i + 1),
                nfc_tools_tag_info_submenu_cb,
                app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewSubmenu2);
}

bool nfc_tools_scene_tag_info_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 0) {
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdTagChipInfo);
            consumed = true;
        } else if(event.event == 100) {
            // FeliCa : View Memory → sector_analysis
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdSectorAnalysis);
            consumed = true;
        } else if(event.event >= 1 &&
                  event.event <= app->ndef_record_count) {
            app->ndef_selected_record = (uint8_t)(event.event - 1);
            scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdNdefRecordDetail);
            consumed = true;
        }
        // event == UINT32_MAX (no-op item): ignored
    }

    return consumed;
}

void nfc_tools_scene_tag_info_on_exit(void* context) {
    NfcToolsApp* app = context;
    submenu_reset(app->submenu2);
}

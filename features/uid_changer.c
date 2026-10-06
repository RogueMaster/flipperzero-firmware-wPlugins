#include "uid_changer.h"
#include "../ui/uid_changer_ui.h"
#include "../mtools_app.h"
#include "../nfc/card_info.h"
#include "../nfc/card_reader.h"

#include <furi.h>
#include <gui/modules/byte_input.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <notification/notification_messages.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <stdio.h>
#include <string.h>

#define UID_VIEW_BYTES 11
#define UID_VIEW_FLOW 12
#define UID_EVENT_SAVE 120
#define UID_EVENT_READ 121
#define UID_EVENT_POPUP_END 122
#define UID_EVENT_SCAN 123
#define UID_EVENT_ANIM 124
#define UID_EVENT_KEY 130

struct MToolsUidChanger {
    MToolsApp* app;
    ByteInput* input;
    View* flow;
    UidPage page;
    uint8_t source;
    uint8_t protocol; // 0=MFC / ISO14443-A, 1=ISO15693
    uint8_t card_type; // 0=MFC 1K, 1=MFC 4K, 2=ISO15693, 3=MFC unknown
    uint8_t target_card_type;
    uint8_t read_card_type;
    uint8_t uid[10];
    uint8_t draft[10];
    uint8_t block0[16];
    uint8_t block0_draft[16];
    bool block0_valid;
    bool block0_edited;
    bool block0_from_card;
    uint8_t sak;
    uint8_t atqa[2];
    uint8_t uid_len;
    uint8_t edit_len;
    bool edit_touched;
    bool uid_valid;
    MagicGenType gen;
    NfcPoller* poller;
    bool scanning;
    bool scan_found;
    FuriTimer* popup_timer;
    FuriTimer* scan_anim_timer;
    uint8_t scan_anim_phase;
    bool popup_active;
    UidPage popup_page;
    char popup_text[26];
    uint8_t read_uid[10];
    uint8_t read_len;
    char input_header[24];
};

static void uid_make_block0(MToolsUidChanger* instance) {
    static const uint8_t factory[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0xFF, 0xFF};
    memset(instance->block0, 0, sizeof(instance->block0));
    if(instance->uid_len == 4) {
        memcpy(instance->block0 + 8, factory, sizeof(factory));
        instance->block0[5] = instance->sak;
        instance->block0[6] = instance->atqa[0];
        instance->block0[7] = instance->atqa[1];
        mtools_mfc_prepare_block0(instance->block0, instance->uid);
    } else if(instance->uid_len == 7) {
        memcpy(instance->block0, instance->uid, 7);
        instance->block0[7] = instance->sak;
        instance->block0[8] = instance->atqa[0];
        instance->block0[9] = instance->atqa[1];
        memcpy(instance->block0 + 10, factory, 6);
    }
    instance->block0_valid = instance->uid_len == 4 || instance->uid_len == 7;
    instance->block0_edited = instance->block0_valid;
    instance->block0_from_card = false;
}

static void uid_show(MToolsUidChanger* instance);

static void uid_saved_callback(void* context) {
    MToolsUidChanger* instance = context;
    view_dispatcher_send_custom_event(instance->app->view_dispatcher, UID_EVENT_SAVE);
}

static void uid_changed_callback(void* context) {
    MToolsUidChanger* instance = context;
    instance->edit_touched = true;
}

static void uid_popup_timer_callback(void* context) {
    MToolsUidChanger* instance = context;
    view_dispatcher_send_custom_event(instance->app->view_dispatcher, UID_EVENT_POPUP_END);
}

static void uid_scan_anim_callback(void* context) {
    MToolsUidChanger* instance = context;
    view_dispatcher_send_custom_event(instance->app->view_dispatcher, UID_EVENT_ANIM);
}

static bool uid_flow_input(InputEvent* event, void* context) {
    MToolsUidChanger* instance = context;
    if(event->key == InputKeyBack) return false;
    if(event->type != InputTypeShort &&
       !(event->type == InputTypeRepeat && (event->key == InputKeyUp || event->key == InputKeyDown))) return false;
    view_dispatcher_send_custom_event(instance->app->view_dispatcher, UID_EVENT_KEY + event->key);
    return true;
}

static void uid_stop_reader(MToolsUidChanger* instance) {
    furi_timer_stop(instance->scan_anim_timer);
    mtools_card_reader_stop(
        instance->app->scanner,
        &instance->scanning,
        &instance->poller,
        instance->app->notifications);
}

static void uid_scanner_callback(NfcScannerEvent event, void* context) {
    MToolsUidChanger* instance = context;
    if(event.type != NfcScannerEventTypeDetected || instance->scan_found) return;
    MToolsCardProtocols protocols = mtools_card_protocols(&event);
    if(!protocols.iso14443a && !protocols.iso15693) return;
    instance->protocol = protocols.iso15693 && !protocols.iso14443a ? 1 : 0;
    instance->scan_found = true;
    view_dispatcher_send_custom_event(instance->app->view_dispatcher, UID_EVENT_SCAN);
}

static NfcCommand uid_read_callback(NfcGenericEvent event, void* context) {
    MToolsUidChanger* instance = context;
    const uint8_t* uid = NULL;
    size_t length = 0;
    if(event.protocol == NfcProtocolIso14443_3a) {
        Iso14443_3aPollerEvent* data = event.event_data;
        if(data->type != Iso14443_3aPollerEventTypeReady) return NfcCommandContinue;
        const Iso14443_3aData* card = nfc_poller_get_data(instance->poller);
        uid = iso14443_3a_get_uid(card, &length);
        instance->sak = iso14443_3a_get_sak(card);
        iso14443_3a_get_atqa(card, instance->atqa);
        instance->read_card_type = instance->sak == 0x08 ? 0 : instance->sak == 0x18 ? 1 : 3;
    } else if(event.protocol == NfcProtocolIso15693_3) {
        Iso15693_3PollerEvent* data = event.event_data;
        if(data->type != Iso15693_3PollerEventTypeReady) return NfcCommandContinue;
        uid = iso15693_3_get_uid(nfc_poller_get_data(instance->poller), &length);
        instance->read_card_type = 2;
    }
    if(!uid || (length != 4 && length != 7 && length != 8 && length != 10)) return NfcCommandContinue;
    memcpy(instance->read_uid, uid, length);
    instance->read_len = length;
    view_dispatcher_send_custom_event(instance->app->view_dispatcher, UID_EVENT_READ);
    return NfcCommandStop;
}

static void uid_start_reader(MToolsUidChanger* instance) {
    instance->scan_anim_phase = 0;
    instance->page = UidPageReading;
    uid_show(instance);
    furi_timer_start(instance->scan_anim_timer, furi_ms_to_ticks(180));
    instance->scan_found = false;
    mtools_card_scan_start(
        instance->app->scanner,
        &instance->scanning,
        instance->app->notifications,
        uid_scanner_callback,
        instance);
}

static void uid_start_poller(MToolsUidChanger* instance) {
    mtools_card_scan_stop(instance->app->scanner, &instance->scanning);
    NfcProtocol protocol = instance->protocol ? NfcProtocolIso15693_3 : NfcProtocolIso14443_3a;
    instance->poller = nfc_poller_alloc(instance->app->nfc, protocol);
    nfc_poller_start(instance->poller, uid_read_callback, instance);
}

static void uid_select_valid_gen(MToolsUidChanger* instance) {
    const bool iso = instance->card_type == 2;
    const bool selected_iso =
        instance->gen >= MagicGenIso15693Gen1 && instance->gen <= MagicGenIso15693Gen3;
    if(instance->gen < MagicGenCount && selected_iso == iso &&
       mtools_magic_uid_length_supported(instance->gen, instance->uid_len)) return;
    instance->gen = iso ? MagicGenIso15693Gen1 : MagicGenMfcGen1a;
}

static void uid_detect_gen(MToolsUidChanger* instance) {
    static const MagicGenType mfc_order[] = {
        MagicGenMfcGen4, MagicGenMfcGtu, MagicGenMfcGen3,
        MagicGenMfcGen1a, MagicGenMfcGen2};
    static const MagicGenType iso_order[] = {
        MagicGenIso15693Gen3, MagicGenIso15693Gen1};
    const MagicGenType* order = instance->card_type == 2 ? iso_order : mfc_order;
    const size_t count = instance->card_type == 2 ? COUNT_OF(iso_order) : COUNT_OF(mfc_order);
    for(size_t i = 0; i < count; i++) {
        if(mtools_detect_magic_tag(instance->app->nfc, order[i])) {
            instance->gen = order[i];
            return;
        }
    }
    /* The reference treats ISO Gen2 as the remaining supported format. The
     * choice is tentative; the write command still requires readback. */
    instance->gen = instance->card_type == 2 ? MagicGenIso15693Gen2 : MagicGenMfcGen1a;
}

static const char* uid_card_name(uint8_t card_type) {
    switch(card_type) {
    case 0: return "MFC 1K";
    case 1: return "MFC 4K";
    case 2: return "ISO15693";
    default: return "MFC";
    }
}

static void uid_show(MToolsUidChanger* instance) {
    UidFlowModel* model = view_get_model(instance->flow);
    memset(model, 0, sizeof(*model));
    model->page = instance->page;
    model->selected = -1;
    model->step = instance->page == UidPageMagic || instance->page == UidPageGen4Target ? 1 :
                  instance->page == UidPageWrite ? 2 : 0;
    model->uid_len = instance->uid_len;
    model->card_type = instance->card_type;
    model->scan_anim_phase = instance->scan_anim_phase;
    model->gen = instance->gen;
    model->popup_active = instance->popup_active;
    memcpy(model->popup_text, instance->popup_text, sizeof(model->popup_text));
    memcpy(model->uid, instance->uid, sizeof(model->uid));
    switch(instance->page) {
    case UidPageSource:
        snprintf(model->lines[0], sizeof(model->lines[0]), "Read from tag");
        snprintf(model->lines[1], sizeof(model->lines[1]), "Enter manually");
        model->selected = instance->source;
        break;
    case UidPageManualProtocol:
        snprintf(model->lines[0], sizeof(model->lines[0]), "MFC 1K");
        snprintf(model->lines[1], sizeof(model->lines[1]), "MFC 4K");
        snprintf(model->lines[2], sizeof(model->lines[2]), "ISO15693");
        model->selected = instance->card_type;
        break;
    case UidPageManualLength:
        snprintf(model->lines[0], sizeof(model->lines[0]), "4 bytes");
        snprintf(model->lines[1], sizeof(model->lines[1]), "7 bytes");
        model->selected = instance->edit_len == 7;
        break;
    case UidPageGen4Target:
        snprintf(model->title, sizeof(model->title), "Gen4 target type");
        snprintf(model->lines[0], sizeof(model->lines[0]), "MFC 1K");
        snprintf(model->lines[1], sizeof(model->lines[1]), "MFC 4K");
        model->selected = instance->target_card_type;
        break;
    case UidPageReading:
        snprintf(model->title, sizeof(model->title), "Reading UID...");
        snprintf(model->lines[0], sizeof(model->lines[0]), "Place card on back");
        break;
    case UidPageReadResult:
        snprintf(model->title, sizeof(model->title), "%s / %uB", uid_card_name(instance->card_type), instance->uid_len);
        {
            char hex[20] = {0};
            mtools_uid_format_hex(hex, sizeof(hex), instance->uid, MIN(instance->uid_len, 8));
            snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]), "UID: %s", hex);
            if(instance->uid_len > 8) {
                mtools_uid_format_hex(hex, sizeof(hex), instance->uid + 8, instance->uid_len - 8);
                snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]), "     %s", hex);
            }
            if(instance->card_type != 2) {
                snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]),
                         "SAK: %02X ATQA: %02X%02X", instance->sak,
                         instance->atqa[0], instance->atqa[1]);
                snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]),
                         "%s", instance->gen == MagicGenMfcGtu ?
                             "GDM Block0 auto-built" : "OK: Edit Block0");
            }
        }
        break;
    case UidPageMagic:
        break;
    case UidPageWrite:
        snprintf(model->title, sizeof(model->title), "%s / %uB", uid_card_name(instance->card_type), instance->uid_len);
        {
            char hex[20] = {0};
            mtools_uid_format_hex(hex, sizeof(hex), instance->uid, MIN(instance->uid_len, 8));
            snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]), "UID: %s", hex);
            if(instance->uid_len > 8) {
                mtools_uid_format_hex(hex, sizeof(hex), instance->uid + 8, instance->uid_len - 8);
                snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]), "     %s", hex);
            }
            if(instance->gen == MagicGenMfcGtu)
                snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]),
                         "GDM Block0 auto-built");
            else if(instance->card_type != 2)
                snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]),
                         "SAK: %02X ATQA: %02X%02X", instance->sak,
                         instance->atqa[0], instance->atqa[1]);
            snprintf(model->info_lines[model->info_count++], sizeof(model->info_lines[0]),
                     "%s", mtools_uid_gen_name(instance->gen));
        }
        break;
    case UidPageEdit:
    case UidPageBlock0Edit:
        break;
    }
    view_commit_model(instance->flow, true);
    view_dispatcher_switch_to_view(instance->app->view_dispatcher, UID_VIEW_FLOW);
}

static void uid_popup(MToolsUidChanger* instance, const char* message) {
    snprintf(instance->popup_text, sizeof(instance->popup_text), "%s", message);
    instance->popup_active = true;
    instance->popup_page = instance->page;
    furi_timer_start(instance->popup_timer, furi_ms_to_ticks(1000));
    uid_show(instance);
}

static void uid_edit(MToolsUidChanger* instance) {
    instance->page = UidPageEdit;
    if(instance->card_type == 2) instance->edit_len = 8;
    instance->edit_touched = false;
    memset(instance->draft, 0, sizeof(instance->draft));
    if(instance->uid_valid && instance->card_type != 2) {
        memcpy(instance->draft, instance->uid, MIN(instance->uid_len, instance->edit_len));
    } else if(instance->uid_valid && instance->edit_len == instance->uid_len) {
        memcpy(instance->draft, instance->uid, instance->uid_len);
    } else if(instance->card_type == 2) {
        instance->draft[0] = 0xE0;
    }
    snprintf(instance->input_header, sizeof(instance->input_header), "%s",
             instance->card_type == 2 ? "UID: Enter 8 bytes" :
             instance->edit_len == 7 ? "UID: Enter 7 bytes" : "UID: Enter 4 bytes");
    byte_input_set_header_text(instance->input, instance->input_header);
    byte_input_set_result_callback(instance->input, uid_saved_callback, uid_changed_callback, instance,
                                   instance->draft, instance->edit_len);
    view_dispatcher_switch_to_view(instance->app->view_dispatcher, UID_VIEW_BYTES);
}

static void uid_edit_block0(MToolsUidChanger* instance) {
    if((instance->uid_len != 4 && instance->uid_len != 7) || instance->card_type == 2) {
        uid_popup(instance, "MFC 4/7B only");
        return;
    }
    if(!instance->block0_valid) uid_make_block0(instance);
    memcpy(instance->block0_draft, instance->block0, 16);
    if(instance->uid_len == 4)
        mtools_mfc_prepare_block0(instance->block0_draft, instance->uid);
    else
        memcpy(instance->block0_draft, instance->uid, 7);
    instance->page = UidPageBlock0Edit;
    snprintf(instance->input_header, sizeof(instance->input_header), "Block0: Enter 16 bytes");
    byte_input_set_header_text(instance->input, instance->input_header);
    byte_input_set_result_callback(instance->input, uid_saved_callback, NULL, instance,
                                   instance->block0_draft, 16);
    view_dispatcher_switch_to_view(instance->app->view_dispatcher, UID_VIEW_BYTES);
}

MToolsUidChanger* mtools_uid_changer_alloc(MToolsApp* app) {
    MToolsUidChanger* instance = malloc(sizeof(MToolsUidChanger));
    furi_check(instance);
    memset(instance, 0, sizeof(*instance));
    instance->app = app;
    instance->uid_len = 4;
    instance->gen = MagicGenCount;
    instance->target_card_type = 0;
    instance->input = byte_input_alloc();
    instance->flow = view_alloc();
    instance->popup_timer = furi_timer_alloc(uid_popup_timer_callback, FuriTimerTypeOnce, instance);
    instance->scan_anim_timer = furi_timer_alloc(uid_scan_anim_callback, FuriTimerTypePeriodic, instance);
    view_allocate_model(instance->flow, ViewModelTypeLocking, sizeof(UidFlowModel));
    view_set_context(instance->flow, instance);
    view_set_draw_callback(instance->flow, mtools_uid_flow_draw);
    view_set_input_callback(instance->flow, uid_flow_input);
    view_dispatcher_add_view(app->view_dispatcher, UID_VIEW_BYTES, byte_input_get_view(instance->input));
    view_dispatcher_add_view(app->view_dispatcher, UID_VIEW_FLOW, instance->flow);
    return instance;
}

void mtools_uid_changer_enter(MToolsUidChanger* instance) {
    instance->page = UidPageSource;
    instance->target_card_type = 0;
    instance->uid_valid = false;
    instance->block0_valid = false;
    instance->block0_edited = false;
    instance->block0_from_card = false;
    instance->gen = MagicGenCount;
    instance->popup_active = false;
    uid_show(instance);
}

void mtools_uid_changer_exit(MToolsUidChanger* instance) {
    uid_stop_reader(instance);
    furi_timer_stop(instance->popup_timer);
    instance->popup_active = false;
}

bool mtools_uid_changer_back(MToolsUidChanger* instance) {
    if(instance->popup_active) {
        furi_timer_stop(instance->popup_timer);
        instance->popup_active = false;
        if(instance->popup_page == UidPageEdit)
            view_dispatcher_switch_to_view(instance->app->view_dispatcher, UID_VIEW_BYTES);
        else
            uid_show(instance);
        return true;
    }
    switch(instance->page) {
    case UidPageSource: return false;
    case UidPageManualProtocol: instance->page = UidPageSource; break;
    case UidPageManualLength: instance->page = UidPageManualProtocol; break;
    case UidPageGen4Target: instance->page = UidPageReadResult; break;
    case UidPageReading:
        uid_stop_reader(instance);
        instance->page = UidPageSource;
        break;
    case UidPageReadResult: instance->page = UidPageSource; break;
    case UidPageEdit:
        instance->page = instance->card_type == 2 ? UidPageManualProtocol : UidPageManualLength;
        break;
    case UidPageBlock0Edit: instance->page = UidPageReadResult; break;
    case UidPageMagic: instance->page = UidPageReadResult; break;
    case UidPageWrite: instance->page = UidPageMagic; break;
    }
    uid_show(instance);
    return true;
}

bool mtools_uid_changer_event(MToolsUidChanger* instance, uint32_t event) {
    if(event == UID_EVENT_ANIM) {
        if(instance->page == UidPageReading) {
            instance->scan_anim_phase = (instance->scan_anim_phase + 1) % 4;
            uid_show(instance);
        }
        return true;
    }
    if(event == UID_EVENT_POPUP_END) {
        if(instance->popup_active) {
            instance->popup_active = false;
            instance->page = instance->popup_page;
            if(instance->page == UidPageEdit)
                view_dispatcher_switch_to_view(instance->app->view_dispatcher, UID_VIEW_BYTES);
            else
                uid_show(instance);
        }
        return true;
    }
    if(event == UID_EVENT_SCAN && instance->page == UidPageReading) {
        uid_start_poller(instance);
        return true;
    }
    if(event == UID_EVENT_READ && instance->page == UidPageReading) {
        uid_stop_reader(instance);
        memcpy(instance->uid, instance->read_uid, instance->read_len);
        instance->uid_len = instance->read_len;
        instance->uid_valid = true;
        instance->card_type = instance->read_card_type;
        instance->gen = MagicGenCount;
        uid_select_valid_gen(instance);
        uid_detect_gen(instance);
        if(instance->gen == MagicGenMfcGen4) {
            uint8_t config[30];
            if(instance->card_type == 3 && mtools_mfc_gen4_read_config(instance->app->nfc, config)) {
                if(config[26] == 0x08 || config[26] == 0x18)
                    instance->card_type = config[26] == 0x18 ? 1 : 0;
                else if(config[28] == 0x3F || config[28] == 0xFF)
                    instance->card_type = config[28] == 0xFF ? 1 : 0;
            }
            if(instance->card_type <= 1) {
                instance->sak = instance->card_type == 1 ? 0x18 : 0x08;
                instance->atqa[0] = instance->uid_len == 7 ?
                    (instance->card_type == 1 ? 0x42 : 0x44) :
                    (instance->card_type == 1 ? 0x02 : 0x04);
                instance->atqa[1] = 0;
            }
        }
        if(instance->card_type != 2) uid_make_block0(instance);
        else {
            instance->block0_valid = false;
            instance->block0_edited = false;
        }
        notification_message(instance->app->notifications, &sequence_success);
        instance->page = UidPageReadResult;
        uid_show(instance);
        return true;
    }
    if(event == UID_EVENT_SAVE && instance->page == UidPageEdit) {
        if(instance->card_type != 2 && !instance->edit_touched) {
            uid_popup(instance, "Enter UID first");
            return true;
        }
        if(instance->card_type == 2 && instance->draft[0] != 0xE0) {
            uid_popup(instance, "ISO15: start E0");
            return true;
        }
        instance->uid_len = instance->edit_len;
        memcpy(instance->uid, instance->draft, instance->uid_len);
        instance->uid_valid = true;
        if(instance->card_type != 2) {
            instance->sak = instance->card_type == 1 ? 0x18 : 0x08;
            instance->atqa[0] = instance->uid_len == 7 ? (instance->card_type == 1 ? 0x42 : 0x44) :
                                 (instance->card_type == 1 ? 0x02 : 0x04);
            instance->atqa[1] = 0;
            uid_make_block0(instance);
        } else {
            instance->block0_valid = false;
            instance->block0_edited = false;
        }
        uid_select_valid_gen(instance);
        instance->page = UidPageReadResult;
        uid_show(instance);
        return true;
    }
    if(event == UID_EVENT_SAVE && instance->page == UidPageBlock0Edit) {
        memcpy(instance->block0, instance->block0_draft, 16);
        memcpy(instance->uid, instance->block0, instance->uid_len);
        if(instance->uid_len == 4) {
            /* A four-byte Classic block 0 carries UID BCC in byte 4. */
            mtools_mfc_prepare_block0(instance->block0, instance->uid);
            instance->sak = instance->block0[5];
            instance->atqa[0] = instance->block0[6];
            instance->atqa[1] = instance->block0[7];
        } else {
            instance->sak = instance->block0[7];
            instance->atqa[0] = instance->block0[8];
            instance->atqa[1] = instance->block0[9];
        }
        instance->card_type = instance->sak == 0x08 ? 0 : instance->sak == 0x18 ? 1 : 3;
        instance->block0_valid = true;
        instance->block0_edited = true;
        instance->page = UidPageReadResult;
        uid_show(instance);
        return true;
    }
    if(event < UID_EVENT_KEY || event > UID_EVENT_KEY + InputKeyBack) return false;
    if(instance->popup_active) return true;
    InputKey key = event - UID_EVENT_KEY;
    if(key == InputKeyLeft) {
        if(!mtools_uid_changer_back(instance))
            scene_manager_previous_scene(instance->app->scene_manager);
        return true;
    }
    if(key == InputKeyUp || key == InputKeyDown) {
        int direction = key == InputKeyDown ? 1 : -1;
        if(instance->page == UidPageSource) instance->source ^= 1;
        if(instance->page == UidPageManualProtocol)
            instance->card_type = (instance->card_type + direction + 3) % 3;
        if(instance->page == UidPageManualLength)
            instance->edit_len = instance->edit_len == 4 ? 7 : 4;
        if(instance->page == UidPageGen4Target)
            instance->target_card_type ^= 1;
        if(instance->page == UidPageMagic) {
            const int start = instance->card_type == 2 ? MagicGenIso15693Gen1 : MagicGenMfcGen1a;
            const int count = instance->card_type == 2 ? 3 : 5;
            int index = instance->gen - start;
            instance->gen = start + (index + direction + count) % count;
        }
        uid_show(instance);
        return true;
    }
    if(key != InputKeyRight && key != InputKeyOk) return false;
    switch(instance->page) {
    case UidPageSource:
        if(instance->source == 0) {
            uid_start_reader(instance);
            return true;
        }
        instance->page = UidPageManualProtocol;
        break;
    case UidPageManualProtocol:
        instance->uid_valid = false;
        instance->gen = MagicGenCount;
        if(instance->card_type != 2) {
            instance->edit_len = 4;
            instance->page = UidPageManualLength;
            break;
        }
        uid_edit(instance);
        return true;
    case UidPageManualLength:
        uid_edit(instance);
        return true;
    case UidPageGen4Target:
        instance->card_type = instance->target_card_type;
        instance->sak = instance->card_type == 1 ? 0x18 : 0x08;
        instance->atqa[0] = instance->uid_len == 7 ? (instance->card_type == 1 ? 0x42 : 0x44) :
                            (instance->card_type == 1 ? 0x02 : 0x04);
        instance->atqa[1] = 0;
        uid_make_block0(instance);
        instance->page = UidPageMagic;
        break;
    case UidPageReading:
        return true;
    case UidPageReadResult:
        if(key == InputKeyOk && instance->card_type != 2) {
            if(instance->gen == MagicGenMfcGtu) {
                uid_popup(instance, "GDM Block0 auto-built");
                return true;
            }
            uid_edit_block0(instance);
            return true;
        }
        instance->page = instance->gen == MagicGenMfcGen4 && instance->card_type == 3 ?
                             UidPageGen4Target : UidPageMagic;
        break;
    case UidPageMagic:
        if(!mtools_magic_uid_length_supported(instance->gen, instance->uid_len)) {
            uid_popup(instance, "UID length unsupported");
            return true;
        }
        instance->page = UidPageWrite;
        break;
    case UidPageWrite:
        if(key == InputKeyOk) {
            notification_message(instance->app->notifications, &sequence_blink_start_blue);
            furi_delay_ms(200);
            bool available = mtools_magic_uid_write_implemented(instance->gen);
            bool success = available && mtools_write_magic_uid_with_block0(
                instance->app->nfc, instance->gen, instance->uid, instance->uid_len,
                instance->block0_edited ? instance->block0 : NULL);
            notification_message(instance->app->notifications, &sequence_blink_stop);
            notification_message(instance->app->notifications,
                                 success ? &sequence_success : &sequence_error);
            uid_popup(instance, success ? "Write success" :
                                !available ? "Writer unavailable" :
                                mtools_magic_write_error()[0] ? mtools_magic_write_error() : "Write failed");
            return true;
        }
        break;
    case UidPageEdit:
    case UidPageBlock0Edit:
        return true;
    }
    uid_show(instance);
    return true;
}

void mtools_uid_changer_free(MToolsUidChanger* instance) {
    uid_stop_reader(instance);
    furi_timer_free(instance->scan_anim_timer);
    furi_timer_stop(instance->popup_timer);
    furi_timer_free(instance->popup_timer);
    view_dispatcher_remove_view(instance->app->view_dispatcher, UID_VIEW_BYTES);
    view_dispatcher_remove_view(instance->app->view_dispatcher, UID_VIEW_FLOW);
    byte_input_free(instance->input);
    view_free(instance->flow);
    free(instance);
}

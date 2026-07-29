#include "mf_settings_api.h"
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Storage {
    int unused;
} Storage;
typedef struct File {
    int open;
} File;

void* mf_settings_test_alloc(void);
void mf_settings_test_free(void* state);
bool mf_settings_test_enter(void* state, const MfSettingsEnterArgs* args);
void mf_settings_test_leave(void* state);
bool mf_settings_test_close(
    void* state,
    MfSettingsRequest* pending,
    MorseFlipperMappedFalResult* result);
uint8_t mf_settings_test_custom_count(const void* state);

static Storage storage;
static unsigned opens;
static unsigned closes;
static unsigned frees;
static const char* storage_fixture;
static MfSettingsRequest last_request;
static MfSettingsSnapshot response_snapshot = {
    .local_wpm = 20U,
    .lesson = 2U,
    .farnsworth_wpm = 20U,
    .answer_timeout_s = 3U,
    .group_pause_s = 3U,
    .group_size = 1U,
    .group_count = 3U,
    .straight_wpm = 10U,
    .straight_answer_timeout_s = 1U,
    .straight_next_delay_s = 1U,
    .gpio_dit_pin = 3U,
    .gpio_dah_pin = 4U};

void* furi_record_open(const char* name) {
    (void)name;
    opens++;
    return &storage;
}
void furi_record_close(const char* name) {
    (void)name;
    closes++;
}
File* storage_file_alloc(Storage* value) {
    (void)value;
    return calloc(1U, sizeof(File));
}
void storage_file_free(File* file) {
    frees++;
    free(file);
}
bool storage_file_open(File* file, const char* path, FS_AccessMode access, FS_OpenMode mode) {
    (void)file;
    (void)path;
    (void)access;
    (void)mode;
    return storage_fixture != NULL;
}
size_t storage_file_read(File* file, void* buffer, size_t size) {
    size_t length;
    (void)file;
    if(storage_fixture == NULL) return 0U;
    length = strlen(storage_fixture);
    if(length > size) length = size;
    memcpy(buffer, storage_fixture, length);
    return length;
}
bool storage_file_close(File* file) {
    (void)file;
    return true;
}

VariableItem* variable_item_list_add(
    VariableItemList* list,
    const char* label,
    uint8_t count,
    VariableItemChangeCallback changed,
    void* context) {
    VariableItem* item = &list->items[list->count++];
    *item = (VariableItem){
        .label = label, .values_count = count, .changed = changed, .context = context};
    return item;
}
void variable_item_list_reset(VariableItemList* list) {
    memset(list->items, 0, sizeof(list->items));
    list->count = 0U;
    list->enter = NULL;
    list->enter_context = NULL;
    list->resets++;
}
void variable_item_list_set_enter_callback(
    VariableItemList* list,
    VariableItemListEnterCallback callback,
    void* context) {
    assert(callback != NULL);
    list->enter = callback;
    list->enter_context = context;
}
void variable_item_list_set_selected_item(VariableItemList* list, uint8_t selected) {
    list->selected = selected;
}
uint8_t variable_item_list_get_selected_item_index(VariableItemList* list) {
    return list->selected;
}
void variable_item_set_values_count(VariableItem* item, uint8_t count) {
    item->values_count = count;
}
void variable_item_set_current_value_index(VariableItem* item, uint8_t index) {
    item->current_index = index;
}
uint8_t variable_item_get_current_value_index(VariableItem* item) {
    return item->current_index;
}
void variable_item_set_current_value_text(VariableItem* item, const char* text) {
    snprintf(item->value_text, sizeof(item->value_text), "%s", text == NULL ? "" : text);
    item->current_text = item->value_text;
}
void* variable_item_get_context(VariableItem* item) {
    return item->context;
}

static bool apply(void* context, const MfSettingsRequest* request, MfSettingsResponse* response) {
    unsigned* calls = context;
    (*calls)++;
    last_request = *request;
    response->accepted = true;
    response->snapshot = response_snapshot;
    if(request->kind == MfSettingsSetInputSource)
        response->snapshot.input_source = (uint8_t)request->value;
    if(request->kind == MfSettingsSetKeyerMode)
        response->snapshot.keyer_mode = (uint8_t)request->value;
    if(request->kind == MfSettingsSetLocalWpm) {
        response->snapshot.local_wpm = (uint16_t)request->value;
        response->snapshot.farnsworth_wpm = (uint8_t)request->value;
    }
    return true;
}

static void
    assert_item(const VariableItemList* list, uint8_t row, const char* label, const char* value) {
    assert(row < list->count);
    assert(strcmp(list->items[row].label, label) == 0);
    if(value != NULL && strcmp(list->items[row].current_text, value) != 0) {
        fprintf(
            stderr,
            "label mismatch row=%u label=%s got=%s expected=%s\n",
            (unsigned)row,
            label,
            list->items[row].current_text,
            value);
        assert(false);
    }
}

static void assert_page_labels(uint8_t page, const VariableItemList* list) {
    switch(page) {
    case MfSettingsEntryKeying:
        assert_item(list, 0U, "WPM", "20");
        assert_item(list, 1U, "Input", "buttons");
        assert_item(list, 2U, "Keyer", "Plain Iambic");
        assert_item(list, 3U, "Swap paddles", "Yes");
        break;
    case MfSettingsEntryAudio:
        assert_item(list, 0U, "Audio path", "Buzzer");
        assert_item(list, 1U, "Frequency", "B6");
        assert_item(list, 2U, "PWM Volume", "100%");
        assert_item(list, 3U, "Waveform", "Sine");
        break;
    case MfSettingsEntryListening:
        assert_item(list, 0U, "Lesson", "2 - U");
        assert_item(list, 1U, "WPM", "20");
        assert_item(list, 2U, "Farnsworth", "20");
        assert_item(list, 3U, "Answer timeout", "3");
        assert_item(list, 4U, "Group pause", "3");
        assert_item(list, 5U, "Group size", "1");
        assert_item(list, 6U, "Groups", "3");
        assert_item(list, 7U, "Chars", "lesson");
        break;
    case MfSettingsEntryStraight:
        assert_item(list, 0U, "WPM", "10");
        assert_item(list, 1U, "Answer timeout", "1");
        assert_item(list, 2U, "Next delay", "1");
        break;
    case MfSettingsEntryTxGroups:
        assert_item(list, 0U, "Difficulty", "Competition");
        break;
    case MfSettingsEntryRxCallsigns:
        assert_item(list, 0U, "Length", "4-6");
        assert_item(list, 1U, "WPM", "20");
        assert_item(list, 2U, "Farnsworth", "15");
        break;
    case MfSettingsEntryGpio:
        assert_item(list, 0U, "dit/SK", "P7");
        assert_item(list, 1U, "dah", "P5");
        assert_item(list, 2U, "Virtual gnd", "P3");
        assert_item(list, 3U, "PTT/TX", "off");
        break;
    case MfSettingsEntryUsb:
        assert_item(list, 0U, "Connection", "MIDI");
        assert_item(list, 1U, "Paddle keys", "X Z qwertz (DE)");
        assert_item(list, 2U, "Straight key", "Z qwertz (DE)");
        assert_item(list, 3U, "Invert mouse", "Yes");
        break;
    default:
        assert(false);
    }
}

int main(void) {
    VariableItemList list = {0};
    unsigned calls = 0U;
    MfSettingsHostServices services = {.struct_size = sizeof(services), .apply = apply};
    MfSettingsEnterArgs args = {
        .struct_size = sizeof(args),
        .entry = MfSettingsEntryKeying,
        .selected_state = 2U,
        .list = &list,
        .snapshot = {.local_wpm = 12U, .input_source = 2U, .lesson = 1U, .farnsworth_wpm = 12U},
        .services = &services,
        .service_context = &calls};
    void* state = mf_settings_test_alloc();
    assert(state != NULL);
    assert(mf_settings_test_enter(state, &args));
    assert(list.count == 4U && list.selected == 2U);
    assert(list.enter != NULL && list.enter_context == state);
    list.enter(list.enter_context, 0U);
    assert(calls == 0U);
    assert(strcmp(list.items[1].current_text, "buttons") == 0);
    list.items[0].current_index = 10U;
    list.items[0].changed(&list.items[0]);
    assert(calls == 1U);
    assert(list.items[0].current_index == 10U && strcmp(list.items[0].current_text, "20") == 0);
    list.items[1].current_index = 1U;
    list.items[1].changed(&list.items[1]);
    assert(calls == 2U);
    assert(last_request.kind == MfSettingsSetInputSource && last_request.value == 0U);
    assert(
        list.items[1].current_index == 1U && strcmp(list.items[1].current_text, "straight") == 0);
    list.items[1].current_index = 2U;
    list.items[1].changed(&list.items[1]);
    assert(last_request.kind == MfSettingsSetInputSource && last_request.value == 1U);
    assert(list.items[1].current_index == 2U && strcmp(list.items[1].current_text, "paddle") == 0);
    list.items[1].current_index = 0U;
    list.items[1].changed(&list.items[1]);
    assert(last_request.kind == MfSettingsSetInputSource && last_request.value == 2U);
    assert(
        list.items[1].current_index == 0U && strcmp(list.items[1].current_text, "buttons") == 0);
    {
        static const uint8_t keyer_values[] = {1U, 2U, 6U, 7U, 8U, 5U, 9U};
        static const char* const keyer_names[] = {
            "Straight", "Bug", "Plain Iambic", "Iambic A", "Iambic B", "Ultimatic", "Keyahead"};
        for(uint8_t index = 0U; index < sizeof(keyer_values); index++) {
            list.items[2].current_index = index;
            list.items[2].changed(&list.items[2]);
            assert(last_request.kind == MfSettingsSetKeyerMode);
            assert(last_request.value == keyer_values[index]);
            assert(list.items[2].current_index == index);
            assert(strcmp(list.items[2].current_text, keyer_names[index]) == 0);
        }
    }
    mf_settings_test_leave(state);
    assert(list.count == 0U && list.resets >= 2U);
    assert(opens == closes && frees == opens);
    mf_settings_test_free(state);

    list = (VariableItemList){0};
    calls = 0U;
    storage_fixture = "numbers=0123456789\nmore dits=EISH5AUVNDB\n";
    services.apply = apply;
    args.entry = MfSettingsEntryListening;
    args.list = &list;
    args.selected_state = 7U;
    args.service_context = &calls;
    args.snapshot = (MfSettingsSnapshot){
        .local_wpm = 20U,
        .lesson = 2U,
        .farnsworth_wpm = 20U,
        .answer_timeout_s = 3U,
        .group_pause_s = 3U,
        .group_size = 1U,
        .group_count = 3U};
    state = mf_settings_test_alloc();
    assert(mf_settings_test_enter(state, &args));
    assert(list.items[7].values_count == 3U && list.items[7].current_index == 0U);
    list.items[7].current_index = 2U;
    list.items[7].changed(&list.items[7]);
    assert(last_request.kind == MfSettingsSetListeningCustomSet && last_request.value == 2U);
    mf_settings_test_leave(state);
    mf_settings_test_free(state);
    storage_fixture = NULL;

    static const uint8_t page_rows[] = {4U, 4U, 8U, 3U, 1U, 3U, 4U, 4U};
    static const uint8_t expected_kinds[][8] = {
        {MfSettingsSetLocalWpm,
         MfSettingsSetInputSource,
         MfSettingsSetKeyerMode,
         MfSettingsSetHandedness},
        {MfSettingsSetAudioPath,
         MfSettingsSetTone,
         MfSettingsSetP2Volume,
         MfSettingsSetAudioWaveform},
        {MfSettingsSetListeningLesson,
         MfSettingsSetLocalWpm,
         MfSettingsSetListeningFarnsworth,
         MfSettingsSetListeningAnswerTimeout,
         MfSettingsSetListeningGroupPause,
         MfSettingsSetListeningGroupSize,
         MfSettingsSetListeningGroupCount,
         MfSettingsSetListeningCustomSet},
        {MfSettingsSetStraightWpm,
         MfSettingsSetStraightAnswerTimeout,
         MfSettingsSetStraightNextDelay},
        {MfSettingsSetTxGroupsDifficulty},
        {MfSettingsSetRxCallsignsLength,
         MfSettingsSetRxCallsignsWpm,
         MfSettingsSetRxCallsignsFarnsworth},
        {0U},
        {MfSettingsSetUsbMode,
         MfSettingsSetUsbPaddlePreset,
         MfSettingsSetUsbStraightPreset,
         MfSettingsSetUsbMouseInvert},
    };
    static const uint8_t expected_values[][8] = {
        {10U, 2U, 1U, 0U},
        {0U, 0U, 10U, 0U},
        {1U, 10U, 1U, 3U, 3U, 1U, 3U, 0U},
        {10U, 1U, 1U},
        {0U},
        {0U, 10U, 1U},
        {0U},
        {0U, 0U, 0U, 0U},
    };
    calls = 0U;
    for(uint8_t page = MfSettingsEntryKeying; page <= MfSettingsEntryUsb; page++) {
        list = (VariableItemList){0};
        args.entry = page;
        args.list = &list;
        args.selected_state = page == MfSettingsEntryListening ? 7U : 0U;
        args.snapshot = (MfSettingsSnapshot){
            .local_wpm = 20U,
            .input_source = 2U,
            .keyer_mode = 6U,
            .handedness = true,
            .audio_path = 0U,
            .tone_index = 30U,
            .p2_volume = 100U,
            .audio_waveform = 1U,
            .lesson = 2U,
            .farnsworth_wpm = 20U,
            .answer_timeout_s = 3U,
            .group_pause_s = 3U,
            .group_size = 1U,
            .group_count = 3U,
            .straight_wpm = 10U,
            .straight_answer_timeout_s = 1U,
            .straight_next_delay_s = 1U,
            .tx_groups_difficulty = 2U,
            .gpio_dit_pin = 5U,
            .rx_callsigns_length = 5U,
            .rx_callsigns_wpm = 20U,
            .rx_callsigns_farnsworth_wpm = 15U,
            .gpio_dah_pin = 3U,
            .gpio_ground_pin = 1U,
            .gpio_ptt_pin = 0xffU,
            .usb_mode = 3U,
            .usb_paddle_preset = 8U,
            .usb_straight_preset = 7U,
            .usb_mouse_invert = true};
        state = mf_settings_test_alloc();
        assert(mf_settings_test_enter(state, &args));
        assert(list.count == page_rows[page]);
        assert(list.selected == args.selected_state);
        assert_page_labels(page, &list);
        if(page == MfSettingsEntryListening) {
            assert(strcmp(list.items[0].label, "Lesson") == 0);
            assert(strcmp(list.items[7].label, "Chars") == 0);
        }
        for(uint8_t row = 0U; row < list.count; row++) {
            if(list.items[row].changed == NULL) continue;
            list.items[row].current_index = 0U;
            list.items[row].changed(&list.items[row]);
            if(page != MfSettingsEntryGpio) {
                assert(last_request.kind == expected_kinds[page][row]);
                assert(last_request.value == expected_values[page][row]);
            }
        }
        if(page != MfSettingsEntryGpio) {
            for(uint8_t row = 0U; row < list.count; row++) {
                uint32_t expected = 0U;
                uint8_t selected;

                if(list.items[row].changed == NULL) continue;
                selected = list.items[row].values_count - 1U;
                list.items[row].current_index = selected;
                list.items[row].changed(&list.items[row]);
                if(page == MfSettingsEntryKeying) {
                    static const uint8_t input_values[] = {2U, 0U, 1U};
                    static const uint8_t keyer_values[] = {1U, 2U, 6U, 7U, 8U, 5U, 9U};
                    expected = row == 0U ? 30U :
                               row == 1U ? input_values[selected] :
                               row == 2U ? keyer_values[selected] :
                                           selected;
                } else if(page == MfSettingsEntryAudio) {
                    expected = row == 2U ? 10U + selected * 5U : selected;
                } else if(page == MfSettingsEntryListening) {
                    static const uint8_t bases[] = {1U, 10U, 1U, 3U, 3U, 1U, 3U, 0U};
                    expected = bases[row] + selected;
                } else if(page == MfSettingsEntryStraight) {
                    expected = row == 0U ? 10U + selected : 1U + selected;
                } else if(page == MfSettingsEntryRxCallsigns) {
                    static const uint8_t bases[] = {0U, 10U, 1U};
                    expected = bases[row] + selected;
                } else {
                    expected = selected;
                }
                assert(last_request.kind == expected_kinds[page][row]);
                if(last_request.value != expected) {
                    fprintf(
                        stderr,
                        "upper mismatch page=%u row=%u got=%lu expected=%lu index=%u count=%u\n",
                        (unsigned)page,
                        (unsigned)row,
                        (unsigned long)last_request.value,
                        (unsigned long)expected,
                        (unsigned)list.items[row].current_index,
                        (unsigned)list.items[row].values_count);
                    return 1;
                }
            }
        }
        mf_settings_test_leave(state);
        assert(list.count == 0U);
        assert(list.enter == NULL && list.enter_context == NULL);
        mf_settings_test_free(state);
    }
    assert(calls == 54U);

    list = (VariableItemList){0};
    calls = 0U;
    services.apply = apply;
    args.entry = MfSettingsEntryGpio;
    args.list = &list;
    args.service_context = &calls;
    args.snapshot.gpio_dit_pin = 5U;
    args.snapshot.gpio_dah_pin = 3U;
    args.snapshot.gpio_ground_pin = 1U;
    args.snapshot.gpio_ptt_pin = 0xffU;
    state = mf_settings_test_alloc();
    assert(mf_settings_test_enter(state, &args));
    MfSettingsRequest pending = {.kind = MfSettingsRequestNone};
    MorseFlipperMappedFalResult close_result = {0};
    assert(mf_settings_test_close(state, &pending, &close_result));
    assert(calls == 0U && close_result.request_exit);
    assert(pending.kind == MfSettingsRequestNone);
    list.items[0].current_index = 5U;
    list.items[0].changed(&list.items[0]);
    list.items[1].current_index = 0U;
    list.items[1].changed(&list.items[1]);
    list.items[2].current_index = 0U;
    list.items[2].changed(&list.items[2]);
    list.items[3].current_index = 1U;
    list.items[3].changed(&list.items[3]);
    pending = (MfSettingsRequest){.kind = MfSettingsRequestNone};
    close_result = (MorseFlipperMappedFalResult){0};
    assert(mf_settings_test_close(state, &pending, &close_result));
    assert(calls == 0U && close_result.request_exit);
    assert(pending.kind == MfSettingsApplyGpioDraft);
    assert(pending.gpio_dit_pin == 7U);
    assert(pending.gpio_dah_pin == 1U);
    assert(pending.gpio_ground_pin == 0xffU);
    assert(pending.gpio_ptt_pin == 7U);
    mf_settings_test_leave(state);
    mf_settings_test_free(state);

    list = (VariableItemList){0};
    calls = 0U;
    services.apply = apply;
    args.entry = MfSettingsEntryGpio;
    args.list = &list;
    args.service_context = &calls;
    args.snapshot.gpio_dit_pin = 3U;
    args.snapshot.gpio_dah_pin = 4U;
    args.snapshot.gpio_ground_pin = 0xffU;
    args.snapshot.gpio_ptt_pin = 0xffU;
    state = mf_settings_test_alloc();
    assert(mf_settings_test_enter(state, &args));
    assert(list.count == 4U);
    assert(strcmp(list.items[3].current_text, "off") == 0);
    list.items[3].current_index = 1U;
    list.items[3].changed(&list.items[3]);
    assert(strcmp(list.items[3].current_text, "P16") == 0);
    pending = (MfSettingsRequest){.kind = MfSettingsRequestNone};
    close_result = (MorseFlipperMappedFalResult){0};
    assert(mf_settings_test_close(state, &pending, &close_result));
    assert(calls == 0U && close_result.request_exit);
    assert(pending.kind == MfSettingsApplyGpioDraft && pending.gpio_ptt_pin == 7U);
    mf_settings_test_leave(state);
    assert(list.count == 0U && list.resets >= 2U);
    mf_settings_test_free(state);
    printf("test_settings_plugin: passed\n");
    return 0;
}

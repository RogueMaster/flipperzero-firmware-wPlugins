#pragma once
#include "uhf_types.h"
#include <stdbool.h>
#include <stddef.h>

#define UHF_SAVED_TAG_LIMIT    999U
#define UHF_SAVED_TAG_TEXT_MAX 512U

typedef struct {
    char epc[UHF_EPC_HEX_MAX + 1U];
    char tid[UHF_TID_HEX_MAX + 1U];
    char user[UHF_USER_HEX_MAX + 1U];
    char timestamp[20];
    uint16_t pc;
    uint32_t rssi;
} UhfSavedTag;

bool uhf_saved_tag_parse(const char* text, UhfSavedTag* out);
bool uhf_saved_tag_format(const UhfSavedTag* tag, char* out, size_t size);
/* List only canonical tag_NNN.uhf records; IDs are sorted numerically. */
bool uhf_saved_tags_list(uint16_t* ids, size_t capacity, size_t* count);
bool uhf_saved_tag_load(uint16_t id, UhfSavedTag* out);
bool uhf_saved_tag_save(const UhfSavedTag* tag, uint16_t* id);
bool uhf_saved_tag_delete(uint16_t id);

bool uhf_saved_tag_update(uint16_t id, const UhfSavedTag* tag);

#include "uhf_tag_config_view.h"

#include <string.h>

static const uint8_t family_monza4qt[] = {0xE2, 0x80, 0x11, 0x05};
static const uint8_t family_higgs3[] = {0xE2, 0x00, 0x34, 0x12};
static const uint8_t oid_monza4qt_prefix[] =
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x81, 0xE4, 0x38, 0x01, 0x01, 0x02, 0x01, 0x1EU};
static const uint8_t oid_higgs3_prefix[] =
    {0x2B, 0x06, 0x01, 0x04, 0x01, 0x81, 0xE4, 0x38, 0x01, 0x01, 0x02, 0x01, 0x22U};

static bool seader_uhf_tag_config_match_oid(
    const uint8_t* ptr,
    size_t len,
    const uint8_t* oid,
    size_t oid_len) {
    return len >= oid_len && memcmp(ptr, oid, oid_len) == 0;
}

static size_t seader_uhf_tag_config_next_entry_offset(SeaderBytesView payload, size_t ordinal) {
    size_t offset = 0U;
    size_t seen = 0U;

    while(offset < payload.len) {
        uint8_t tag = payload.ptr[offset++];
        size_t value_len = 0U;

        if(tag == 0x04U) {
            value_len = 4U;
        } else if(tag == 0x11U) {
            value_len = 17U;
            if(seen == ordinal) {
                return offset - 1U;
            }
            seen++;
        } else {
            return payload.len;
        }

        if(offset + value_len > payload.len) {
            return payload.len;
        }
        offset += value_len;
    }

    return payload.len;
}

bool seader_uhf_tag_config_parse(SeaderBytesView payload, SeaderUhfTagConfigView* view) {
    size_t offset = 0U;

    if(!payload.ptr || payload.len == 0U || !view) return false;
    memset(view, 0, sizeof(*view));
    view->raw = payload;
    view->prefix.ptr = payload.ptr;

    while(offset < payload.len) {
        uint8_t tag = payload.ptr[offset++];
        if(tag == 0x04U) {
            if(offset + sizeof(family_monza4qt) > payload.len) {
                return false;
            }
            if(memcmp(payload.ptr + offset, family_monza4qt, sizeof(family_monza4qt)) == 0) {
                view->has_monza4qt = true;
                view->monza4qt_family_id =
                    (SeaderBytesView){payload.ptr + offset, sizeof(family_monza4qt)};
            } else if(memcmp(payload.ptr + offset, family_higgs3, sizeof(family_higgs3)) == 0) {
                view->has_higgs3 = true;
                view->higgs3_family_id =
                    (SeaderBytesView){payload.ptr + offset, sizeof(family_higgs3)};
            }
            offset += sizeof(family_monza4qt);
        } else if(tag == 0x11U) {
            if(view->entry_count == 0U) {
                view->prefix.len = (offset - 1U) - 0U;
            }
            if(offset + 17U > payload.len) {
                return false;
            }
            view->entry_count++;
            offset += 17U;
        } else {
            return false;
        }
    }

    if(view->entry_count == 0U) {
        view->prefix.len = payload.len;
    }

    return true;
}

bool seader_uhf_tag_config_get_entry(
    const SeaderUhfTagConfigView* view,
    size_t index,
    SeaderUhfTagConfigEntryView* entry) {
    if(!view || !entry || index >= view->entry_count) return false;
    memset(entry, 0, sizeof(*entry));

    const size_t start = seader_uhf_tag_config_next_entry_offset(view->raw, index);
    const size_t next = seader_uhf_tag_config_next_entry_offset(view->raw, index + 1U);
    if(start >= view->raw.len) return false;

    entry->raw.ptr = view->raw.ptr + start;
    entry->raw.len = 18U;

    entry->oid.ptr = view->raw.ptr + start + 1U;
    entry->oid.len = 17U;

    if(next > start + entry->raw.len) {
        entry->value.ptr = view->raw.ptr + start + entry->raw.len;
        entry->value.len = next - (start + entry->raw.len);
    }

    if(seader_uhf_tag_config_match_oid(
           entry->oid.ptr, entry->oid.len, oid_higgs3_prefix, sizeof(oid_higgs3_prefix))) {
        entry->kind = SeaderUhfTagConfigEntryHiggs3Access;
    } else if(seader_uhf_tag_config_match_oid(
                  entry->oid.ptr,
                  entry->oid.len,
                  oid_monza4qt_prefix,
                  sizeof(oid_monza4qt_prefix))) {
        entry->kind = SeaderUhfTagConfigEntryMonza4QtAccess;
    } else {
        entry->kind = SeaderUhfTagConfigEntryUnknown;
    }

    return true;
}

#pragma once

#include "snmp_ber_view.h"

typedef enum {
    SeaderUhfTagConfigEntryUnknown = 0,
    SeaderUhfTagConfigEntryHiggs3Access,
    SeaderUhfTagConfigEntryMonza4QtAccess,
} SeaderUhfTagConfigEntryKind;

typedef struct {
    SeaderBytesView raw;
    SeaderBytesView oid;
    SeaderBytesView value;
    SeaderUhfTagConfigEntryKind kind;
} SeaderUhfTagConfigEntryView;

typedef struct {
    SeaderBytesView raw;
    SeaderBytesView prefix;
    size_t entry_count;
    bool has_higgs3;
    bool has_monza4qt;
    SeaderBytesView higgs3_family_id;
    SeaderBytesView monza4qt_family_id;
} SeaderUhfTagConfigView;

bool seader_uhf_tag_config_parse(SeaderBytesView payload, SeaderUhfTagConfigView* view);

bool seader_uhf_tag_config_get_entry(
    const SeaderUhfTagConfigView* view,
    size_t index,
    SeaderUhfTagConfigEntryView* entry);

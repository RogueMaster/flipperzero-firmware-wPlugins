#include "intents.h"

#include <stdio.h>
#include <string.h>

void id_seq_init(IdSeq* seq, char prefix, uint32_t first) {
    seq->prefix = prefix;
    seq->next = first;
}

uint32_t id_seed_from_time(uint32_t unix_seconds) {
    return (unix_seconds % 1000000u) * 1000u;
}

bool id_seq_next(IdSeq* seq, char out[PROTO_ID_MAX + 1]) {
    snprintf(out, PROTO_ID_MAX + 1, "%c-%06lu", seq->prefix, (unsigned long)seq->next);
    if(!proto_id_valid(out)) return false;
    seq->next++;
    return true;
}

static bool intent_fill(Intent* out, IdSeq* ids, const Scan* scan, IntentAction action) {
    if(!proto_id_valid(scan->id)) return false;
    char id[PROTO_ID_MAX + 1];
    if(!id_seq_next(ids, id)) return false;
    memset(out, 0, sizeof(*out));
    memcpy(out->id, id, sizeof(id));
    memcpy(out->scan_id, scan->id, sizeof(scan->id));
    out->action = action;
    out->tag = scan->tag;
    return true;
}

bool intent_new_weigh(Intent* out, IdSeq* ids, const Scan* scan, uint16_t grams, WeightMode mode) {
    if(grams > PROTO_GRAMS_MAX) return false;
    if(mode != WeightModeGross && mode != WeightModeNet) return false;
    if(!intent_fill(out, ids, scan, IntentActionWeigh)) return false;
    out->grams = grams;
    out->mode = mode;
    return true;
}

bool intent_new_action(Intent* out, IdSeq* ids, const Scan* scan, IntentAction action) {
    switch(action) {
    case IntentActionNew:
    case IntentActionFull:
    case IntentActionEmpty:
    case IntentActionDelete:
    case IntentActionCreateOnHost:
    case IntentActionLinkOnHost:
        break;
    default:
        return false;
    }
    return intent_fill(out, ids, scan, action);
}

static const char* intent_action_name(IntentAction action) {
    switch(action) {
    case IntentActionWeigh:
        return "weigh";
    case IntentActionNew:
        return "new";
    case IntentActionCreateOnHost:
        return "create_on_host";
    case IntentActionLinkOnHost:
        return "link_on_host";
    case IntentActionFull:
        return "full";
    case IntentActionEmpty:
        return "empty";
    case IntentActionDelete:
        return "delete";
    }
    return NULL;
}

size_t intent_encode(const Intent* intent, char* buf, size_t cap) {
    const char* action = intent_action_name(intent->action);
    if(!action || !proto_id_valid(intent->id) || !proto_id_valid(intent->scan_id)) return 0;
    JsonWriter w;
    json_init(&w, buf, proto_line_cap(cap));
    json_object_open(&w);
    proto_write_header(&w, "intent");
    json_key(&w, "id");
    json_string(&w, intent->id);
    json_key(&w, "scan_id");
    json_string(&w, intent->scan_id);
    json_key(&w, "action");
    json_string(&w, action);
    if(intent->action == IntentActionWeigh) {
        json_key(&w, "grams");
        json_int(&w, intent->grams);
        json_key(&w, "mode");
        json_string(&w, intent->mode == WeightModeNet ? "net" : "gross");
    }
    json_key(&w, "tag");
    if(!proto_write_tag(&w, &intent->tag)) return 0;
    json_object_close(&w);
    return json_len(&w);
}

static bool single_slot_put(IntentStore* store, const Intent* intent) {
    SingleSlotIntentStore* s = (SingleSlotIntentStore*)store;
    s->slot = *intent;
    s->occupied = true;
    return true;
}

static bool single_slot_resolve(IntentStore* store, const char* id) {
    SingleSlotIntentStore* s = (SingleSlotIntentStore*)store;
    if(!s->occupied || strcmp(s->slot.id, id) != 0) return false;
    s->occupied = false;
    return true;
}

static const Intent* single_slot_current(const IntentStore* store) {
    const SingleSlotIntentStore* s = (const SingleSlotIntentStore*)store;
    return s->occupied ? &s->slot : NULL;
}

void single_slot_intent_store_init(SingleSlotIntentStore* store) {
    memset(store, 0, sizeof(*store));
    store->base.put = single_slot_put;
    store->base.resolve = single_slot_resolve;
    store->base.current = single_slot_current;
}

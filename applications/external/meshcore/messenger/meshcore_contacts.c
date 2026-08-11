#include "meshcore_contacts.h"

#include <stdio.h>
#include <string.h>

void meshcore_contacts_reset(MeshCoreContacts* contacts) {
    memset(contacts, 0, sizeof(*contacts));
}

/* Index of the contact heard least recently — the one to sacrifice. */
static size_t meshcore_contacts_stalest(const MeshCoreContacts* contacts) {
    size_t worst = 0;
    for(size_t i = 1; i < contacts->count; i++) {
        if(contacts->items[i].last_advert < contacts->items[worst].last_advert) worst = i;
    }
    return worst;
}

bool meshcore_contacts_add(MeshCoreContacts* contacts, const mc_contact_t* contact) {
    size_t slot;

    if(contacts->count < MESHCORE_CONTACTS_MAX) {
        slot = contacts->count++;
    } else {
        /* Full. Keeping the freshest is the useful behaviour: a list of nodes
         * last heard from months ago is not worth the screen. */
        slot = meshcore_contacts_stalest(contacts);
        if(contact->last_advert <= contacts->items[slot].last_advert) {
            contacts->dropped++;
            return false;
        }
        contacts->dropped++;
    }

    MeshCoreContact* out = &contacts->items[slot];
    memcpy(out->public_key, contact->public_key, sizeof(out->public_key));
    snprintf(out->name, sizeof(out->name), "%s", contact->adv_name);
    out->last_advert = contact->last_advert;
    out->type = contact->type;

    return true;
}

void meshcore_contacts_sort_by_last_seen(MeshCoreContacts* contacts) {
    /* Insertion sort: at most MESHCORE_CONTACTS_MAX items, already nearly
     * ordered in practice, and it keeps the code obvious. */
    for(size_t i = 1; i < contacts->count; i++) {
        MeshCoreContact key = contacts->items[i];
        size_t j = i;
        while(j > 0 && contacts->items[j - 1].last_advert < key.last_advert) {
            contacts->items[j] = contacts->items[j - 1];
            j--;
        }
        contacts->items[j] = key;
    }
}

void meshcore_contacts_format_age(uint32_t now, uint32_t last_advert, char* out, size_t cap) {
    /* last_advert is in the node's timebase, so callers pass the node's clock
     * rather than the Flipper's — the two are often minutes apart. */
    if(now == 0 || last_advert == 0) {
        snprintf(out, cap, "-");
        return;
    }
    if(now <= last_advert) {
        /* Heard from in the future: clock skew, not a bug worth surfacing. */
        snprintf(out, cap, "now");
        return;
    }

    uint32_t age = now - last_advert;
    if(age < 60u) {
        snprintf(out, cap, "now");
    } else if(age < 3600u) {
        snprintf(out, cap, "%lum", (unsigned long)(age / 60u));
    } else if(age < 86400u) {
        snprintf(out, cap, "%luh", (unsigned long)(age / 3600u));
    } else {
        snprintf(out, cap, "%lud", (unsigned long)(age / 86400u));
    }
}

bool meshcore_contacts_collect(const mc_event_t* event, void* context) {
    MeshCoreContacts* contacts = context;

    switch(event->code) {
    case MC_RESP_CONTACTS_START:
        /* The node tells us up front how many are coming, which is how we can
         * report "showing 48 of 112" rather than silently truncating. */
        contacts->reported = event->u.contacts_count;
        return true;

    case MC_RESP_CONTACT:
        meshcore_contacts_add(contacts, &event->u.contact);
        return true;

    default:
        return false;
    }
}

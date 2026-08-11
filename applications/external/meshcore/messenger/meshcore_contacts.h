/*
 * The contact list, as the node knows it.
 *
 * Identity and keys live on the node — the Flipper only mirrors enough to draw
 * a list and address a message. Nothing here is persisted and nothing here is
 * authoritative.
 *
 * Deliberately free of furi so the host tests can cover the parts that are
 * easy to get wrong: what happens when the node has more contacts than fit,
 * and how an age is rendered.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../protocol/meshcore_c/meshcore_companion.h"

/* Firmware allows up to 350 contacts. A Flipper has neither the screen nor a
 * reason to hold that many, so we keep the most recently heard and say so. */
#define MESHCORE_CONTACTS_MAX 48

/* "now" / "12d" / "-" all fit comfortably. */
#define MESHCORE_AGE_LEN 8

typedef struct {
    uint8_t public_key[32];
    char name[MC_NAME_LEN + 1];
    uint32_t last_advert;
    uint8_t type;
} MeshCoreContact;

typedef struct {
    MeshCoreContact items[MESHCORE_CONTACTS_MAX];
    size_t count;
    uint32_t reported; /* how many the node announced in CONTACTS_START */
    uint32_t dropped; /* how many did not fit */
} MeshCoreContacts;

void meshcore_contacts_reset(MeshCoreContacts* contacts);

/** Add one record. When full, a newer contact displaces the stalest one.
 *  Returns false if the record was dropped. */
bool meshcore_contacts_add(MeshCoreContacts* contacts, const mc_contact_t* contact);

/** Most recently heard first — the order the list should be read in. */
void meshcore_contacts_sort_by_last_seen(MeshCoreContacts* contacts);

/** Render how long ago a contact was heard, relative to the node's clock.
 *  `now` of 0 means the clock is unknown, which renders as "-". */
void meshcore_contacts_format_age(uint32_t now, uint32_t last_advert, char* out, size_t cap);

/** Stream collector for a GET_CONTACTS reply. Matches
 *  MeshCoreSessionStreamCallback; `context` is a MeshCoreContacts*. Returns
 *  false for anything that is not part of the reply, so a push landing
 *  mid-stream still reaches the application. */
bool meshcore_contacts_collect(const mc_event_t* event, void* context);

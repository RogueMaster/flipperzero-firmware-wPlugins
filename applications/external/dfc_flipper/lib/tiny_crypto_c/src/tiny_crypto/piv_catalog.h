/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV and TWIC data object catalogs and an inventory that reads every
 * catalog object the link's access state allows into one caller pool.
 * Standards: NIST SP 800-73-5 Part 1 sections 3.5 and 4.1.1, Tables 2, 3 and
 * 8, Part 2 section 3.1.2. TWIC Part 2 v5 sections 3.3.6, 4.5, 4.6, 4.7 and
 * 5.2.
 * Configuration: TC_ENABLE_PIV_CATALOG (requires TC_ENABLE_PIV_COMMAND).
 * Limitations: the catalogs hold the readable objects only. Keys, the TWIC
 * E-stickers (TWIC Part 2 v5 4.5 notes 1 and 3) and the PIV data model
 * application of a TWIC card under TWIC rules (4.2) are outside them. OCC is
 * never available, so an OCC alternative in an access rule is never met.
 * The inventory checks framing only. Authenticate objects with the Security
 * Object and their signatures before relying on them.
 * Contracts: docs/api.md.
 * Guide: docs/piv-card.md. */
#ifndef TINY_CRYPTO_PIV_CATALOG_H_
#define TINY_CRYPTO_PIV_CATALOG_H_
#include <tiny_crypto/piv_command.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Access rule for reading an object on one interface (SP 800-73-5 Part 1
 * Table 2, TWIC Part 2 v5 4.5). VCI is the security condition of Part 1
 * Table 2 footnote 9, reported by TC_PIV_link_info as vci. */
typedef enum {
  TC_PIV_ACCESS_ALWAYS,
  TC_PIV_ACCESS_PIN,
  TC_PIV_ACCESS_PIN_OR_OCC,
  TC_PIV_ACCESS_VCI,
  TC_PIV_ACCESS_VCI_PIN,
  TC_PIV_ACCESS_VCI_PIN_OR_OCC,
  TC_PIV_ACCESS_NEVER
} TC_PIV_access;

/* Presence requirement: M, C and O of Part 1 Table 2, and Y and O of TWIC
 * Part 2 v5 4.5. Conditional objects are the digital signature and key
 * management certificates. */
typedef enum { TC_PIV_MANDATORY, TC_PIV_CONDITIONAL, TC_PIV_OPTIONAL } TC_PIV_requirement;

/* What an object holds, for choosing its reader. On the TWIC application
 * the fingerprints, face, printed information, iris, personal information
 * and signature image are encrypted with the TWIC Privacy Key (TWIC Part 2
 * v5 4.6.4 and 4.7). */
typedef enum {
  TC_PIV_KIND_CCC,
  TC_PIV_KIND_CHUID,
  TC_PIV_KIND_UNSIGNED_CHUID,
  TC_PIV_KIND_CERTIFICATE,
  TC_PIV_KIND_FINGERPRINTS,
  TC_PIV_KIND_FACE,
  TC_PIV_KIND_IRIS,
  TC_PIV_KIND_SECURITY,
  TC_PIV_KIND_PRINTED,
  TC_PIV_KIND_DISCOVERY,
  TC_PIV_KIND_KEY_HISTORY,
  TC_PIV_KIND_BIT_GROUP,
  TC_PIV_KIND_SM_SIGNER,
  TC_PIV_KIND_PAIRING_CODE,
  TC_PIV_KIND_TWIC_PRIVACY_KEY,
  TC_PIV_KIND_TWIC_PERSONAL,
  TC_PIV_KIND_TWIC_SIGNATURE_IMAGE
} TC_PIV_object_kind;

/* TC_PIV_object_info.flags: the value is secret material, the pairing code
 * (Part 1 Table 44) or the TWIC Privacy Key (TWIC Part 2 v5 4.6.2). */
enum { TC_PIV_OBJECT_SECRET = 1u << 0 };

/* One catalog entry.
 * tag, tag_length    the GET DATA tag, 1 to 3 bytes.
 * kind               a TC_PIV_object_kind value.
 * contact            TC_PIV_access value on the contact interface.
 * contactless        TC_PIV_access value on the contactless interface.
 * requirement        a TC_PIV_requirement value.
 * key_reference      the key a certificate belongs to (Part 1 Table 8), or 0.
 * flags              TC_PIV_OBJECT_SECRET or 0.
 * container          the container ID, unique within one catalog.
 * minimum_capacity   PIV: the Part 1 Table 8 container capacity. TWIC: the
 *                    largest value of TWIC Part 2 v5 4.6 and 4.7 with its
 *                    structure bytes. */
typedef struct {
  uint8_t tag[3];
  uint8_t tag_length, kind, contact, contactless, requirement, key_reference, flags;
  uint16_t container, minimum_capacity;
} TC_PIV_object_info;

/* Number of entries in the PIV catalog. */
#define TC_PIV_CATALOG_PIV_OBJECTS 36u

/* Catalogs by application and profile:
 * - PIV application, TC_PIV_CARD: the 36 objects of SP 800-73-5 Part 1
 *   Table 3 in that order, with the Table 2 rules and the Table 8 container
 *   IDs, capacities and key references. The Pairing Code container 5FC123
 *   is PIN_OR_OCC on contact, VCI_PIN_OR_OCC on contactless and
 *   TC_PIV_OBJECT_SECRET.
 * - TWIC application, TC_TWIC_LEGACY_CARD: 5FC102, 5FC104, DFC101, DFC103
 *   and DFC10F, the Legacy column of TWIC Part 2 v5 4.5.
 * - TWIC application, TC_TWIC_NEXGEN_CARD: the twelve readable objects of
 *   the NEXGEN column in table order. DFC001, DFC002 and DFC121 are optional.
 * - TWIC: every object is Always, except the TWIC Privacy Key container
 *   DFC101, which is Always on contact, Never on contactless and SECRET.
 * Every other pair has no catalog.
 * The tables are constant. The returned pointers stay valid for the life of
 * the program. */

#if TC_ENABLE_PIV_CATALOG
/* Entries in the catalog, or 0 when the pair has none. */
size_t TC_PIV_catalog_count(TC_PIV_application_id application, TC_PIV_card_profile profile);

/* Entry index of the catalog, or NULL when index is out of range or the
 * pair has no catalog. */
const TC_PIV_object_info* TC_PIV_catalog_at(TC_PIV_application_id application,
                                            TC_PIV_card_profile profile, size_t index);

/* The entry with GET DATA tag tag, or NULL. tag with NULL data is never
 * found. */
const TC_PIV_object_info* TC_PIV_catalog_find(TC_PIV_application_id application,
                                              TC_PIV_card_profile profile, TC_bytes tag);
#endif

/* Result of reading one catalog object.
 * PRESENT     the card returned the object, with a nonempty value.
 * EMPTY       an empty object: 53 00 (Part 1 4.1.1), a TWIC empty form (TWIC
 *             Part 2 v5 3.3.6) or a TWIC 9000 without data for an optional
 *             object (4.5 note).
 * ABSENT      the card reported the object missing: 6A82, or 6A88 on the
 *             TWIC application (TWIC Part 2 v5 5.2).
 * RESTRICTED  the access rule is unmet in the link state, so nothing was
 *             sent.
 * DENIED      the rule was met, and the card refused with 6982 or 6A81.
 * OVERSIZED   the answer did not fit the pool or max_object_bytes on a link
 *             that stayed usable, or the pool left could not take one full
 *             answer and nothing was sent.
 * SKIPPED     the plan left the object out, so nothing was sent. */
typedef enum {
  TC_PIV_OBJECT_PRESENT,
  TC_PIV_OBJECT_EMPTY,
  TC_PIV_OBJECT_ABSENT,
  TC_PIV_OBJECT_RESTRICTED,
  TC_PIV_OBJECT_DENIED,
  TC_PIV_OBJECT_OVERSIZED,
  TC_PIV_OBJECT_SKIPPED
} TC_PIV_object_state;

/* One inventory entry.
 * info     the catalog entry.
 * encoded  PRESENT and EMPTY: the complete returned TLV. Otherwise empty.
 * value    PRESENT: its value. Otherwise empty.
 * status   the card status: 9000 or 6282 for PRESENT and EMPTY, the refusal
 *          for ABSENT and DENIED, 0 when nothing was answered.
 * state    a TC_PIV_object_state value.
 * secured  1 when the object was read under secure messaging.
 * Spans borrow the pool. */
typedef struct {
  const TC_PIV_object_info* info;
  TC_bytes encoded, value;
  uint16_t status;
  uint8_t state, secured;
} TC_PIV_object;

/* TC_PIV_inventory_plan.flags: read the Pairing Code container 5FC123 when
 * its rule is met. Without the flag it is SKIPPED. */
enum { TC_PIV_INVENTORY_PAIRING_CODE = 1u << 0 };

/* Inventory options. max_object_bytes caps the data bytes of one object,
 * and each read gets TC_PIV_RESPONSE_BYTES(max_object_bytes) of the pool at
 * most. 0 offers the rest of the pool. A NULL plan selects flags 0 and no
 * cap. */
typedef struct {
  unsigned flags;
  size_t max_object_bytes;
} TC_PIV_inventory_plan;

/* Inventory of one application. Set objects and capacity before
 * TC_PIV_inventory_read. The read writes the other members.
 * objects    caller array of capacity entries. TC_PIV_catalog_count gives
 *            the entries a catalog needs.
 * count      entries written, one per catalog entry in catalog order.
 * pool       the pool of the last successful read, or NULL.
 * pool_used  pool bytes that hold objects.
 * link       the link state the access decisions used, with the application
 *            and profile that chose the catalog. */
typedef struct {
  TC_PIV_object* objects;
  size_t capacity, count;
  uint8_t* pool;
  size_t pool_used;
  TC_PIV_link_info link;
} TC_PIV_inventory;

/* Pool bytes for every PIV catalog object at its Part 1 Table 8 capacity:
 * the sum of TC_PIV_RESPONSE_BYTES(minimum_capacity + 5) over the catalog,
 * where 5 bytes hold the 53 header. Table 8 capacities are floors, and a
 * card may store more. SD 33 card 2 needs about 26 KiB. */
#define TC_PIV_INVENTORY_POOL_BYTES 77620ul

#if TC_ENABLE_PIV_CATALOG
/* Read the catalog of the application selected on link, in catalog order,
 * with TC_PIV_get_data. Each object's rule for the link interface is checked
 * against the link state first: PIN needs pin_verified, VCI needs vci, and
 * the VCI rules need both. OCC is never met. An object whose rule is unmet is
 * RESTRICTED and nothing is sent. Each answer is received at the next free
 * pool byte, and the decrypted value of a secured read stays in place. A
 * read goes out only when its pool region can take one full answer: Ne and
 * SW1 SW2 on a plain link, 258 bytes under secure messaging (Part 2 4.2.6),
 * and at most the card's DO 7F66 response limit. A smaller region is
 * OVERSIZED and nothing is sent, since a short receive buffer can stop the
 * transport. The pool stays unchanged until
 * TC_PIV_inventory_clear.
 *
 * Outcomes per object:
 * - TC_PIV_OK: PRESENT, or EMPTY for an empty value. A TWIC 9000 without
 *   data is EMPTY for an optional object and aborts with TC_PIV_INVALID for
 *   another.
 * - TC_PIV_CARD_STATUS: the TC_PIV_status_classify meaning for GET DATA on
 *   the application. NOT_FOUND is ABSENT. SECURITY_STATUS and NOT_SUPPORTED
 *   are DENIED. Another status aborts.
 * - TC_PIV_LIMIT on a link that keeps its channel, its exchange budget and
 *   any secure messaging session: OVERSIZED, and the read continues. A limit
 *   that ended the session or spent the budget aborts.
 * - Every other result, and any session loss, aborts with that result.
 *
 * Work: one unit per catalog entry and one per pool byte an object keeps.
 * A charge above the remaining budget sets *work to 0 and aborts with
 * TC_PIV_LIMIT.
 *
 * TC_PIV_ARGUMENT     NULL link, work or inventory, a cleared link, objects
 *                     NULL with a nonzero capacity, an array size above
 *                     SIZE_MAX, unknown plan flags, pool with NULL data and a
 *                     nonzero capacity, pool overlapping *link, its scratch
 *                     buffers, *plan, *work, *inventory or the objects array,
 *                     the objects array overlapping *link, its scratch
 *                     buffers, *work or *inventory, or *work and *inventory
 *                     overlapping *link, its scratch buffers or each other.
 * TC_PIV_REFUSED      no application is selected, or the link lost its
 *                     secure messaging session.
 * TC_PIV_UNSUPPORTED  the selected application and profile have no catalog.
 * TC_PIV_LIMIT        capacity below the catalog count, before anything is
 *                     sent, or an abort above.
 * TC_PIV_INVALID, TC_PIV_CARD_STATUS, TC_PIV_ERROR
 *                     an abort above. TC_PIV_link_status holds the card
 *                     status of a CARD_STATUS abort.
 *
 * An argument error, a refusal, UNSUPPORTED and the capacity LIMIT send
 * nothing and leave the pool, *work and *inventory unchanged. An abort wipes
 * every pool byte offered to the card and the objects array, and leaves
 * count, pool_used and link zero and pool NULL. On TC_PIV_OK the pool bytes
 * after pool_used that answers reached are wiped. */
TC_PIV_result TC_PIV_inventory_read(TC_PIV_link* link, const TC_PIV_inventory_plan* plan,
                                    TC_buffer pool, size_t* work, TC_PIV_inventory* inventory);

/* The entry for container ID container, or NULL when inventory is NULL or
 * holds none. */
const TC_PIV_object* TC_PIV_inventory_find(const TC_PIV_inventory* inventory, uint16_t container);

/* Wipe pool_used bytes of the pool and the objects array, and reset count,
 * pool_used, pool and link. objects and capacity stay, so the inventory can
 * be read again. The pool holds PIN-gated data and possibly the pairing code
 * or the TWIC Privacy Key, so call this on every exit path. Accepts NULL. */
void TC_PIV_inventory_clear(TC_PIV_inventory* inventory);
#endif

#ifdef __cplusplus
}
#endif
#endif

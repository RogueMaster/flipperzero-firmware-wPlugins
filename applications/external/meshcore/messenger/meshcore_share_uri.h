/*
 * meshcore_share_uri — parse MeshCore share links (meshcore://…).
 *
 * The official share formats (docs.meshcore.io/qr_codes) are query URIs:
 *
 *   meshcore://contact/add?name=<url-enc>&public_key=<64 hex>&type=<1-4>
 *   meshcore://channel/add?name=<url-enc>&secret=<32 hex>&region_scope=<...>
 *
 * A contact link becomes an mc_contact_t ready for ADD_UPDATE_CONTACT (cmd 9);
 * a channel link becomes a name + 16-byte secret ready for SET_CHANNEL (cmd 32)
 * — the same commands the reference client (meshcore_py) uses. Kept furi-free so
 * the host tests can cover the fiddly parts: hex decoding, percent/plus
 * decoding, and rejecting malformed links before they reach the node.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "../protocol/meshcore_c/meshcore_companion.h"

/* Parse a `meshcore://contact/add?…` link into `out`. Params may appear in any
 * order; `name` is optional and is percent/plus-decoded. `public_key` must be
 * exactly 64 hex chars and `type` must be 1..4, else this returns false and
 * `out` is left fully zeroed. On success `out` is zeroed then filled, with
 * out_path_len set to 0xFF so the node floods to rediscover the route to a
 * brand-new contact. */
bool meshcore_contact_uri_parse(const char* uri, mc_contact_t* out);

/* Parse a `meshcore://channel/add?…` link. `name_out` (cap incl. NUL) gets the
 * decoded, length-capped channel name; `secret` gets the 16 decoded bytes.
 * region_scope is ignored. Returns false if the scheme is wrong or `secret` is
 * not exactly 32 hex chars; on failure the outputs are left cleared. */
bool meshcore_channel_uri_parse(
    const char* uri,
    char* name_out,
    size_t name_cap,
    uint8_t secret[MC_SECRET_LEN]);

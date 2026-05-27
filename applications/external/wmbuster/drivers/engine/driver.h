#pragma once
/* wM-Buster driver engine — public contract.
 * =============================================================
 *
 * The engine is intentionally small. It does three things:
 *
 *   1. Walks a hand-curated array of `WmbusDriver` descriptors
 *      (see `drivers/engine/registry.c`) and finds the first one
 *      that matches the (manufacturer, version, medium) triple
 *      from the wM-Bus link layer. Wildcards are allowed.
 *
 *   2. Calls the driver's `decode` function with the plain-text
 *      APDU and a buffer to fill. If `decode` is NULL the engine
 *      uses the generic OMS DIF/VIF walker (see
 *      `protocol/wmbus_app_layer.c`) prefixed with the driver's
 *      `title` so the detail-view subtitle and meter-list short
 *      summary read nicely.
 *
 *   3. Provides `wmbus_engine_model_name()`, the *single* source
 *      of truth for friendly meter names — it is the same array.
 *      No separate model table.
 *
 * Adding a new meter is one file under `drivers/europe/<mfr>/`
 * plus one extern + one entry in `drivers/engine/registry.c`.
 * The contributing walkthrough is in `drivers/CONTRIBUTING.md`.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Manufacturer/version/type tuple as carried by the wmbus link
 * header. `version == 0xFF` and `medium == 0xFF` are wildcards.
 * The list is null-terminated by an entry with manuf[0] == 0. */
typedef struct {
    char    manuf[4];   /* 3 letters + NUL, e.g. "TCH"            */
    uint8_t version;    /* link-header byte 5 (or 0xFF wildcard)  */
    uint8_t medium;     /* link-header byte 6 (or 0xFF wildcard)  */
} WmbusMVT;

/* Decode a plain-text APDU into a multi-line, human-readable
 * report. The first line is treated as the meter title (used as
 * the detail-view subtitle and the meter-list short summary).
 * Subsequent lines are "Label  Value" pairs separated by
 * whitespace; the detail view renders them as a table.
 *
 * Drivers should keep labels under 11 characters and values
 * under 19 characters to avoid truncation in the detail canvas.
 *
 * Returns the number of bytes written (excluding the trailing
 * NUL). If the buffer is too small the function may write a
 * truncated result; the engine guarantees `cap >= 256` when it
 * dispatches a driver itself. */
typedef size_t (*WmbusDecodeFn)(
    uint16_t manuf, uint8_t version, uint8_t medium,
    const uint8_t* apdu, size_t apdu_len,
    char* out, size_t cap);

/* Extended decode context — used by drivers that need the meter ID or CI
 * byte (e.g. Diehl PRIOS LFSR seeding) on top of the (M,V,T) triple.
 * Always provided when a driver opts in via `decode_ex`; legacy drivers
 * keep using the plain `decode` callback above. */
typedef struct {
    uint16_t       manuf;
    uint8_t        version;
    uint8_t        medium;
    uint8_t        ci;
    uint32_t       id;        /* link-header A field, little-endian       */
    const uint8_t* apdu;
    size_t         apdu_len;
} WmbusDecodeCtx;

typedef size_t (*WmbusDecodeExFn)(const WmbusDecodeCtx* ctx,
                                  char* out, size_t cap);

/* One row per logical meter family. Drivers live in their own .c
 * files under `drivers/europe/<manufacturer>/`; they expose a
 * `const WmbusDriver` global that `drivers/engine/registry.c`
 * references via `extern`. Keep `id` short and stable — it shows
 * up in log lines and is the recommended PR title prefix. */
typedef struct {
    const char*       id;       /* short kebab-case id (e.g. "iperl")     */
    const char*       title;    /* user-visible (e.g. "Sensus iPERL water") */
    const WmbusMVT*   mvt;      /* null-terminated MVT list                 */
    WmbusDecodeFn     decode;   /* NULL → generic OMS walker + `title`     */
    WmbusDecodeExFn   decode_ex;/* preferred when set; receives full ctx   */
} WmbusDriver;

/* Find the first registered driver whose MVT list matches the
 * triple. Never returns NULL — when nothing matches, a sentinel
 * "generic OMS" driver is returned so callers can decode the
 * payload using the standard walker. */
const WmbusDriver* wmbus_engine_find(
    uint16_t manuf, uint8_t version, uint8_t medium);

/* Convenience: render an APDU into `out` using whichever driver
 * matches. Returns bytes written. */
size_t wmbus_engine_decode(
    uint16_t manuf, uint8_t version, uint8_t medium,
    const uint8_t* apdu, size_t apdu_len,
    char* out, size_t cap);

/* Same as `wmbus_engine_decode` but with full context — callers that have
 * the meter ID and CI byte (i.e. anyone driving from a parsed link frame)
 * should use this so drivers like Diehl PRIOS can derive their stream-cipher
 * seed. Falls back to the legacy callback when `decode_ex` is unset. */
size_t wmbus_engine_decode_ctx(const WmbusDecodeCtx* ctx,
                               char* out, size_t cap);

/* Friendly model name for a (M,V,T) triple — the same data the
 * dispatcher uses, kept consistent between the meter list and the
 * detail screen. Returns NULL when no driver matches. */
const char* wmbus_engine_model_name(
    uint16_t manuf, uint8_t version, uint8_t medium);

/* Internal: shared helper used by every "OMS-walker only" driver.
 * Writes "<title>\n" then runs `wmbus_app_render` into the rest
 * of the buffer. Exposed so a custom driver can fall back to the
 * walker for the tail of its frame. */
size_t wmbus_engine_render_oms(
    const char* title,
    const uint8_t* apdu, size_t apdu_len,
    char* out, size_t cap);

/* The hand-curated registry. Defined in `drivers/engine/registry.c`.
 * Walk it from index 0 to `wmbus_engine_registry_len - 1`. */
extern const WmbusDriver* const wmbus_engine_registry[];
extern const size_t              wmbus_engine_registry_len;

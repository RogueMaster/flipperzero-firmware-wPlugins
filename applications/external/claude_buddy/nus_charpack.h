/**
 * Receiver for Claude Desktop's "character pack" folder push.
 *
 * Anthropic's reference firmware uses this to stream GIFs + a manifest
 * for the on-device pet rendering.  Flipper has no GIF renderer, but we
 * still accept the transfer and persist files under
 *   /ext/apps_data/claude_buddy/packs/<pack>/
 * so the desktop sees a complete handshake (refusing char_begin makes
 * the desktop surface an error to the user).
 *
 * The wire protocol is:
 *   cmd:char_begin(name,total) → cmd:file(path,size) → cmd:chunk(b64)*
 *   → cmd:file_end → (next file) → cmd:char_end
 *
 * All functions are GUI-thread only.  State is module-wide — only one
 * transfer is in flight at a time (the protocol is sequential).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate internal resources.  Call once at app startup. */
void nus_charpack_init(void);
void nus_charpack_free(void);

/* Start a new pack.  Wipes any in-flight transfer, creates the pack
 * directory.  name must be non-empty and free of `..` or `/`; returns
 * true iff the pack dir now exists. */
bool nus_charpack_begin(const char* name);

/* Open the next file for writing.  path is relative, validated against
 * traversal.  Overwrites any existing file of the same name. */
bool nus_charpack_file_open(const char* path);

/* Decode a base64-encoded chunk and append to the current file.
 * Returns cumulative bytes written to the current file, or -1 on error. */
int32_t nus_charpack_chunk_write(const char* b64, int b64_len);

/* Close the current file.  Returns final size, or -1 if no file open. */
int32_t nus_charpack_file_close(void);

/* Finalize the pack (currently just closes any lingering file and logs). */
void nus_charpack_end(void);

/* Abort whatever is in flight — called when the BLE link drops. */
void nus_charpack_reset(void);

#ifdef __cplusplus
}
#endif

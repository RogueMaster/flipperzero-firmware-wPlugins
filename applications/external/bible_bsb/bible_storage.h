#pragma once

#include "bible_state.h"

/* ============================================================================
 * Collection storage — read / write / save passage / NFC export
 *
 * Full implementation: streaming JSON writer, chunked reads, heap defrag,
 * duplicate checking, and Kindled-format JSON export builder.
 * ============================================================================ */

/**
 * Save collection entries to collection.json.
 *
 * Uses a streaming JSON writer with a 512-byte fixed buffer, writes directly
 * to the SD file. String escaping: quote, backslash, newline.
 *
 * @param state  app state (collection[] and collection_count must be valid)
 * @return true if write succeeded
 */
bool bible_storage_save_collection(const BibleAppState* state);

/**
 * Load collection entries from collection.json.
 *
 * Reads in 1KB chunks, max 2KB total. Parses version and passages array.
 * If file doesn't exist, returns empty collection (not an error).
 *
 * @param state  app state (output into collection[] and collection_count)
 * @return true if load succeeded (or file missing)
 */
bool bible_storage_load_collection(BibleAppState* state);

/**
 * Save the current passage to collection.json.
 *
 * Performs the full save flow:
 * 1. Clears lines (heap defrag)
 * 2. Loads collection from SD if not already loaded
 * 3. Checks for duplicate scripture_ref → toast "Already saved"
 * 4. Appends new entry
 * 5. Writes collection.json via streaming JSON writer
 * 6. Reloads lines (restore reader display)
 * 7. Sets toast "Saved!" or "Save failed"
 *
 * @param state  app state (passage must be valid)
 */
void bible_save_passage(BibleAppState* state);

/**
 * Load a saved passage back from the collection into the reader.
 *
 * Clears lines (heap defrag), loads the chapter from SD filtered to the
 * saved verse range, wraps verses, and sets navigation state so the reader
 * shows the saved passage.
 *
 * @param state        app state
 * @param entry_index  index into collection[] (0..collection_count-1)
 * @return true if passage loaded successfully
 */
bool bible_storage_load_verses_for_entry(BibleAppState* state, uint8_t entry_index);

/**
 * Build Kindled-format JSON export into caller-provided buffer.
 *
 * Reloads verse text from SD for each entry. Returns false if buffer too
 * small or collection empty.
 *
 * @param state    app state
 * @param out_buf  output buffer (must be large enough for the JSON text)
 * @param out_len  capacity of out_buf
 * @return true if JSON was fully written into buffer
 */
bool bible_storage_build_kindled_json(BibleAppState* state, char* out_buf, size_t out_len);

/**
 * Persist the last-read book, chapter, and scroll_offset to APP_DATA_PATH("last_read.bin").
 */
bool bible_storage_save_last_read(const BibleAppState* state);

/**
 * Load the last-read passage on startup and populate state->lines and state->scroll_offset.
 * Returns true if a valid last-read chapter was restored.
 */
bool bible_storage_load_last_read(BibleAppState* state);

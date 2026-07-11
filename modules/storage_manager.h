#pragma once

#include "../models/session.h"
#include <storage/storage.h>

#define RECON_MAX_SESSION_FILES 32

/* Create the app data directories if they do not exist. */
void recon_storage_ensure_dirs(Storage* storage);

/* Turn an arbitrary engagement name into a safe file base name: only
 * [A-Za-z0-9 ._-] survive, the rest become '_'. Falls back to "engagement"
 * when the result would be empty. */
void recon_sanitize_filename(const char* name, char* out, size_t out_len);

/* True if a session file with this (already sanitized) base name exists. */
bool recon_storage_session_exists(Storage* storage, const char* name);

/* Persist a session to /ext/apps_data/flipper_recon/sessions/<name>.recon
 * using the Flipper file format. Returns true on success. */
bool recon_storage_save_session(Storage* storage, const Session* session);

/* Load a session previously saved under the given name. Returns true on success. */
bool recon_storage_load_session(Storage* storage, Session* session, const char* name);

/* Delete a stored session file by name. */
bool recon_storage_delete_session(Storage* storage, const char* name);

/* Fill "names" with up to "capacity" stored session names (without extension).
 * Returns the number found. Each entry is an owned FuriString the caller frees. */
size_t recon_storage_list_sessions(Storage* storage, FuriString** names, size_t capacity);

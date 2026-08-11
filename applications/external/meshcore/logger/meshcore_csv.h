/*
 * CSV files on the SD card, flushed line by line.
 *
 * Every line is synced to the card as it is written. That costs throughput and
 * buys the thing a field logger actually needs: pulling the power mid-walk
 * loses at most the line being written, not the session. Nothing here buffers.
 */
#pragma once

#include <furi.h>
#include <storage/storage.h>

typedef struct MeshCoreCsv MeshCoreCsv;

/** Open (creating if needed) and append. The header is written only when the
 *  file is new, so reopening a session appends cleanly. */
MeshCoreCsv* meshcore_csv_open(Storage* storage, const char* path, const char* header);

void meshcore_csv_close(MeshCoreCsv* csv);

/** Append one line; the newline is added here. Synced before returning. */
bool meshcore_csv_write(MeshCoreCsv* csv, const char* line);

uint32_t meshcore_csv_lines(const MeshCoreCsv* csv);

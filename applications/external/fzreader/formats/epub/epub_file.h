#pragma once

#include "reader_io.h"

/** File opened through the firmware storage service, hidden from the drivers. */
typedef struct EpubFile EpubFile;

/** Open a file for random reading. @returns NULL on failure. */
EpubFile* epub_file_open(const char* path);

/** Close the file and release everything. Safe with NULL. */
void epub_file_close(EpubFile* file);

/** Byte source over the opened file. */
ReaderIo epub_file_io(EpubFile* file);

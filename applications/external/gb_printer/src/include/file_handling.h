#ifndef FILE_HANDLING_H
#define FILE_HANDLING_H

#pragma once

#include <stdint.h>

void fgp_storage_next_count(void* fgp_storage);

/* True if file opened successfully */
bool fgp_storage_open(void* fgp_storage, const char* extension);

size_t fgp_storage_write(void* fgp_storage, const void* buf, size_t len);

bool fgp_storage_close(void* fgp_storage);

bool fgp_storage_seek(void* fgp_storage, off_t offs, bool from_start);

void fgp_storage_free(void* fgp_storage);

/* Extension is used for making a full file */
void* fgp_storage_alloc(char* file_prefix, char* extension);

#endif // FILE_HANDLING_H

#pragma once

#define RECORD_STORAGE "storage"
#define UNUSED(value) (void)(value)

void* furi_record_open(const char* name);
void furi_record_close(const char* name);

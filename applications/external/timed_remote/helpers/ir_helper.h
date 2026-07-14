#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <furi.h>
#include <infrared.h>
#include "../signal/infrared_signal.h"

typedef struct {
    InfraredSignal* signal;
    FuriString* name;
} IrSignalItem;

typedef struct {
    IrSignalItem* items;
    size_t count;
    size_t capacity;
} IrSignalList;

IrSignalList* ir_list_alloc(void);
void ir_list_free(IrSignalList*);
bool ir_load(const char*, IrSignalList*);
void ir_tx(InfraredSignal*);
bool ir_files(const char*, FuriString***, size_t*);
void ir_files_free(FuriString**, size_t);

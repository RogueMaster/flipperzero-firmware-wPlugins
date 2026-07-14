/*
This file is part of UDECard App.
A Flipper Zero application to analyse student ID cards from the University of Duisburg-Essen (Intercard)

Copyright (C) 2025-2026 Alexander Hahn

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef TARGET_MANAGER_H
#define TARGET_MANAGER_H

#include <furi.h>

#include <nfc/protocols/mf_classic/mf_classic.h>

#include "udecard.h"

typedef struct ReadTarget {
    uint8_t sector_num;
    MfClassicKey keyA;
    struct ReadTarget* next;
} ReadTarget;

ReadTarget* read_target_alloc(uint8_t sector_num, const uint8_t* keyA);
void read_target_free(ReadTarget* target);
void read_target_free_list(ReadTarget* target);
void read_target_append(ReadTarget* head, ReadTarget* target);

typedef struct ReadTargetManager {
    ReadTarget* head;
    ReadTarget* current;
} ReadTargetManager;

ReadTargetManager* read_target_manager_alloc();
void read_target_manager_free(ReadTargetManager* manager);
ReadTarget* read_target_manager_get_next(ReadTargetManager* manager);

#endif

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

#include "target_manager.h"

#include <furi.h>

/* TARGET */

ReadTarget* read_target_alloc(uint8_t sector_num, const uint8_t* keyA) {
    ReadTarget* target = malloc(sizeof(ReadTarget));
    target->sector_num = sector_num;
    memcpy(&target->keyA.data, keyA, UDECARD_KEY_SIZE);
    target->next = NULL;
    return target;
}

void read_target_free(ReadTarget* target) {
    if(target) {
        free(target);
    }
}

void read_target_free_list(ReadTarget* target) {
    ReadTarget* current = target;
    while(current) {
        ReadTarget* next = current->next;
        read_target_free(current);
        current = next;
    }
}

void read_target_append(ReadTarget* head, ReadTarget* target) {
    ReadTarget* current = head;
    while(current->next) {
        current = current->next;
    }
    current->next = target;
}

/* TARGET MANAGER */

ReadTargetManager* read_target_manager_alloc() {
    ReadTargetManager* manager = malloc(sizeof(ReadTargetManager));
    manager->head = NULL;
    manager->current = NULL;
    return manager;
}

void read_target_manager_free(ReadTargetManager* manager) {
    if(manager) {
        read_target_free_list(manager->head);
        free(manager);
    }
}

ReadTarget* read_target_manager_get_next(ReadTargetManager* manager) {
    if(manager->current == NULL) {
        manager->current = manager->head;
    } else {
        manager->current = manager->current->next;
    }
    return manager->current;
}

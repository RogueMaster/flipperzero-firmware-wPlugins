#include "logic.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Simple CRC32 implementation to replace restricted furi_hal_crc_calc_crc32
uint32_t calculate_crc32(uint32_t crc, const uint8_t* data, size_t size) {
    crc = ~crc;
    for(size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for(int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return ~crc;
}

// Simple Insertion Sort to replace restricted qsort
void sort_records(FileRecord* records, size_t count) {
    for(size_t i = 1; i < count; i++) {
        FileRecord key = records[i];
        int j = (int)i - 1;
        while(j >= 0 && records[j].hash > key.hash) {
            records[j + 1] = records[j];
            j--;
        }
        records[j + 1] = key;
    }
}

void db_remove_record(HashDatabase* db, const char* filename) {
    for(size_t i = 0; i < db->count; i++) {
        if(strcmp(db->records[i].path, filename) == 0) {
            for(size_t j = i; j < db->count - 1; j++) {
                db->records[j] = db->records[j + 1];
            }
            db->count--;
            return;
        }
    }
}

void process_duplicates(HashDatabase* db) {
    db->num_groups = 0;
    if(db->count == 0) return;

    sort_records(db->records, db->count);

    for(size_t i = 0; i < db->count; i++) {
        bool is_start = false;
        if(i < db->count - 1 && db->records[i].hash == db->records[i + 1].hash) {
            if(i == 0 || db->records[i].hash != db->records[i - 1].hash) {
                is_start = true;
            }
        }

        if(is_start) {
            size_t j = i;
            while(j < db->count && db->records[j].hash == db->records[i].hash) {
                j++;
            }

            // Only add group if we have actual duplicates (> 1 file)
            if(j - i > 1) {
                db->groups[db->num_groups].hash = db->records[i].hash;
                db->groups[db->num_groups].start_index = i;
                db->groups[db->num_groups].count = j - i;
                db->num_groups++;
            }
        }
    }
}

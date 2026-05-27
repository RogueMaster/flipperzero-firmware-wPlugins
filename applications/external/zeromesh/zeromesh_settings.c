#include "zeromesh_settings.h"
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define SETTINGS_VERSION 1

void settings_save(ZeroMeshApp* app) {
    if(!app) return;
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    
    storage_common_mkdir(storage, "/ext/zeromesh");
    
    File* file = storage_file_alloc(storage);
    
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[64];
        
        snprintf(line, sizeof(line), "version=%d\n", SETTINGS_VERSION);
        storage_file_write(file, line, strlen(line));
        
        snprintf(line, sizeof(line), "uart_id=%d\n", (int)app->uart_id);
        storage_file_write(file, line, strlen(line));
        
        snprintf(line, sizeof(line), "baud=%lu\n", (unsigned long)app->baud);
        storage_file_write(file, line, strlen(line));
        
        snprintf(line, sizeof(line), "vibro=%d\n", app->notify_vibro ? 1 : 0);
        storage_file_write(file, line, strlen(line));
        
        snprintf(line, sizeof(line), "led=%d\n", app->notify_led ? 1 : 0);
        storage_file_write(file, line, strlen(line));
        
        snprintf(line, sizeof(line), "ringtone=%d\n", (int)app->notify_ringtone);
        storage_file_write(file, line, strlen(line));
        
        snprintf(line, sizeof(line), "scroll_speed=%d\n", app->scroll_speed);
        storage_file_write(file, line, strlen(line));
        
        snprintf(line, sizeof(line), "scroll_fps=%d\n", app->scroll_framerate);
        storage_file_write(file, line, strlen(line));
        
        snprintf(line, sizeof(line), "lmh_mode=%d\n", (int)app->lmh_mode);
        storage_file_write(file, line, strlen(line));
        
        storage_file_close(file);
    }
    
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void settings_load(ZeroMeshApp* app) {
    if(!app) return;
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[256];
        uint16_t bytes_read = storage_file_read(file, buffer, sizeof(buffer) - 1);
        buffer[bytes_read] = '\0';
        
        storage_file_close(file);
        
        char* pos = buffer;
        while(pos < buffer + bytes_read) {
            char* line_end = strchr(pos, '\n');
            if(!line_end) line_end = buffer + bytes_read;
            
            size_t line_len = line_end - pos;
            if(line_len > 0 && line_len < 128) {
                char line[128];
                memcpy(line, pos, line_len);
                line[line_len] = '\0';
                
                char* equals = strchr(line, '=');
                if(equals) {
                    *equals = '\0';
                    char* key = line;
                    char* value_str = equals + 1;
                    int value = atoi(value_str);
                    
                    if(strcmp(key, "uart_id") == 0) {
                        app->uart_id = (FuriHalSerialId)value;
                    } else if(strcmp(key, "baud") == 0) {
                        app->baud = (uint32_t)value;
                    } else if(strcmp(key, "vibro") == 0) {
                        app->notify_vibro = (value != 0);
                    } else if(strcmp(key, "led") == 0) {
                        app->notify_led = (value != 0);
                    } else if(strcmp(key, "ringtone") == 0) {
                        if(value >= 0 && value < RINGTONE_COUNT) {
                            app->notify_ringtone = (RingtoneType)value;
                        }
                    } else if(strcmp(key, "scroll_speed") == 0) {
                        if(value >= 1 && value <= 10) {
                            app->scroll_speed = (uint8_t)value;
                        }
                    } else if(strcmp(key, "scroll_fps") == 0) {
                        if(value >= 1 && value <= 10) {
                            app->scroll_framerate = (uint8_t)value;
                        }
                    } else if(strcmp(key, "lmh_mode") == 0) {
                        if(value >= 0 && value < LMH_COUNT) {
                            app->lmh_mode = (LongMessageHandling)value;
                        }
                    }
                }
            }
            
            pos = line_end + 1;
        }
    }
    
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
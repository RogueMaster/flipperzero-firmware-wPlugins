#include "passwordStorage.h"
#include "../main.h"
#include <string.h>
#include <stdio.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

bool parse_escaped_line(const char* line, char* field1, char* field2, char* field3) {
    char* fields[3] = {field1, field2, field3};
    int current_field = 0;
    int field_letter = 0;
    bool escape = false;

    for(int i = 0; line[i] != '\0'; ++i) {
        char c = line[i];

        if(escape) {
            // Copy the next character
            if(field_letter < 99) {
                fields[current_field][field_letter++] = c;
            }
            escape = false;
        } else if(c == '\\') {
            escape = true;
        } else if(c == ',' && current_field < 2) {
            // End of current field, move to next
            fields[current_field][field_letter] = '\0';
            current_field++;
            field_letter = 0;
        } else {
            if(field_letter < 99) {
                fields[current_field][field_letter++] = c;
            }
        }
    }

    if(escape) {
        // Dangling backslash at end of input
        return false;
    }

    // Terminate last field
    if(current_field > 2) return false; // Too many commas
    fields[current_field][field_letter] = '\0';

    // Require exactly 3 fields
    return current_field == 2;
}

// Add entry to password list
bool password_list_add(
    Credential* credentials,
    size_t i,
    const char* service,
    const char* username,
    const char* password) {
    strcpy(credentials[i].name, service);
    strcpy(credentials[i].username, username);
    strcpy(credentials[i].password, password);

    return true;
}

// Custom function to read a single line from file
ssize_t storage_file_read_line(File* file, char* buffer, size_t max_len) {
    if(!file || !buffer || max_len == 0) return -1;

    size_t count = 0;
    uint8_t ch;

    while(count < max_len - 1) {
        ssize_t read = storage_file_read(file, &ch, 1);
        if(read != 1) break; // EOF or read error

        if(ch == '\n') break; // End of line

        if(ch != '\r') { // Skip carriage returns
            buffer[count++] = (char)ch;
        }
    }

    if(count == 0) return 0; // No data read (EOF or empty line)

    buffer[count] = '\0'; // Null-terminate the string
    return count;
}

size_t read_passwords_from_file(const char* filename, Credential* credentials) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return 0;

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, filename, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("PasswordManager", "Failed to open file: %s", filename);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return 0;
    }

    char line[300];
    char service[100], username[100], password[100];

    size_t i = 0;
    while(true) {
        // Read a line
        memset(line, 0, sizeof(line));
        if(storage_file_read_line(file, line, sizeof(line)) <= 0) {
            break; // End of file or error
        }
        // Parse the line (expected format: "service,username,password")
        if(parse_escaped_line(line, service, username, password)) {
            // Add to our list
            password_list_add(credentials, i, service, username, password);
            i++;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return i;
}

// Write a new entry to the password file
bool write_password_to_file(
    const char* filename,
    const char* service,
    const char* username,
    const char* password) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, filename, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        // If file doesn't exist, create it
        if(!storage_file_open(file, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            FURI_LOG_E("PasswordManager", "Failed to create file: %s", filename);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }
    }

    // Format the line
    char line[100];
    snprintf(line, sizeof(line), "%s,%s,%s\n", service, username, password);

    // Write to file
    bool success = storage_file_write(file, line, strlen(line)) == strlen(line);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

bool delete_line_from_file(const char* path, size_t line_to_delete) {
    // Open original file for reading
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* source = storage_file_alloc(storage);

    if(!storage_file_open(source, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("FileEdit", "Failed to open source file");
        return false;
    }

    // Open a temporary file for writing
    File* tmp = storage_file_alloc(storage);
    if(!storage_file_open(tmp, "/ext/passowordManager_tmp.txt", FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E("FileEdit", "Failed to open temporary file");
        storage_file_close(source);
        return false;
    }

    // Line read buffer
    char line[300];
    size_t current_line = 0;

    while(true) {
        ssize_t len = storage_file_read_line(source, line, sizeof(line));
        if(len <= 0) break;

        // Write all lines except the one to delete
        if(current_line != line_to_delete) {
            storage_file_write(tmp, line, strlen(line));
            storage_file_write(tmp, "\n", 1); // Add newline back
        }
        current_line++;
    }

    storage_file_close(source);
    storage_file_close(tmp);

    // Delete original and rename temp
    storage_simply_remove(storage, path);
    storage_common_rename(storage, "/ext/passowordManager_tmp.txt", path);

    furi_record_close(RECORD_STORAGE);

    return true;
}

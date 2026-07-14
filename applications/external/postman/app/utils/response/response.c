#include <furi.h>
#include <furi_hal.h>
#include <string.h>
#include <regex.h>
#include "../../app.h" // Include the app header to access app structure

size_t find_start_marker(const FuriString* furi_str, const char* start_marker) {
    size_t start_pos = furi_string_search_str(furi_str, start_marker, 0);
    if(start_pos != FURI_STRING_FAILURE) {
        FURI_LOG_T(TAG, "Start marker found at position: %zu", start_pos);
    } else {
        FURI_LOG_T(TAG, "Start marker not found");
    }
    return start_pos;
}

size_t find_end_marker(const FuriString* furi_str, const char* end_marker, size_t start_pos) {
    size_t end_pos = furi_string_search_str(furi_str, end_marker, start_pos);
    if(end_pos != FURI_STRING_FAILURE) {
        FURI_LOG_T(TAG, "End marker found at position: %zu", end_pos);
    } else {
        FURI_LOG_T(TAG, "End marker not found");
    }
    return end_pos;
}

bool extract_response_text(App* app, const char* start_marker, const char* end_marker) {
    FuriString* furi_str = furi_string_alloc_set(app->text_box_store);
    size_t start_pos = find_start_marker(furi_str, start_marker);
    size_t end_pos = find_end_marker(furi_str, end_marker, start_pos);

    // If the start marker is found
    if(start_pos != FURI_STRING_FAILURE) {
        start_pos += strlen(start_marker); // Move past the start marker

        // If the end marker is found and is after the start marker
        if(end_pos != FURI_STRING_FAILURE && end_pos > start_pos) {
            size_t length = end_pos - start_pos; // Calculate the length of the text to extract

            // Extract the text between the markers
            FuriString* extracted_str = furi_string_alloc();
            furi_string_set_n(extracted_str, furi_str, start_pos, length);

            // Trim leading and trailing spaces
            furi_string_trim(extracted_str, " ");

            // Copy the extracted text back to text_box_store
            furi_string_set(app->text_box_store, extracted_str);

            // Free the allocated FuriStrings
            furi_string_free(furi_str);
            furi_string_free(extracted_str);

            return true;
        } else {
            // If no end marker is found, extract till the end of the string
            FuriString* remaining_str = furi_string_alloc();
            furi_string_set_n(
                remaining_str, furi_str, start_pos, furi_string_size(furi_str) - start_pos);

            // Trim leading and trailing spaces
            furi_string_trim(remaining_str, " ");

            furi_string_set(app->text_box_store, remaining_str);

            // Free the allocated FuriStrings
            furi_string_free(furi_str);
            furi_string_free(remaining_str);

            return true;
        }
    }

    // Free the allocated FuriString
    furi_string_free(furi_str);

    return false; // No valid response text found
}

void clear_new_lines(App* app) {
    FuriString* furi_str = furi_string_alloc_set(app->text_box_store);
    FuriString* cleaned_str = furi_string_alloc();

    for(size_t i = 0; i < furi_string_size(furi_str); i++) {
        char ch = furi_string_get_char(furi_str, i);
        if(ch == '\n' || ch == '\r' || ch == '\t') {
            furi_string_push_back(cleaned_str, ' '); // Replace new line with space
        } else {
            furi_string_push_back(cleaned_str, ch);
        }
    }

    // Copy the cleaned text back to text_box_store
    furi_string_set(app->text_box_store, cleaned_str);

    // Free the allocated FuriStrings
    furi_string_free(furi_str);
    furi_string_free(cleaned_str);
}

bool extract_status_line(App* app, FuriString* status_line) {
    const char* status_marker = "STATUS: ";
    FuriString* furi_str = furi_string_alloc_set(app->text_box_store);
    size_t start_pos = 0;

    while((start_pos = furi_string_search_str(furi_str, status_marker, start_pos)) !=
          FURI_STRING_FAILURE) {
        start_pos += strlen(status_marker);

        // Ensure there are at least three characters after the status marker
        if(furi_string_size(furi_str) >= start_pos + 3) {
            // Copy the entire status line including the marker and the next three characters
            furi_string_set_n(
                status_line,
                furi_str,
                start_pos - strlen(status_marker),
                3 + strlen(status_marker));
            furi_string_free(furi_str);
            return true;
        }

        // Move to the next character to continue searching
        start_pos++;
    }

    furi_string_free(furi_str);
    return false; // No valid status line found
}
#include <ctype.h>
#include <stdbool.h>

bool is_json(const char* text) {
    if(!text || !*text) return false;

    // Skip leading whitespace
    while(*text && isspace((unsigned char)*text))
        text++;

    // Must start with { or [
    return (*text == '{' || *text == '[');
}

void remove_packet_markers(FuriString* furi_str) {
    if(furi_string_empty(furi_str)) return;

    FuriString* cleaned_str = furi_string_alloc();
    const char* text = furi_string_get_cstr(furi_str);

    // Skip leading whitespace and initial packet marker
    while(*text && isspace((unsigned char)*text))
        text++;
    while(*text && isxdigit((unsigned char)*text))
        text++;
    while(*text && isspace((unsigned char)*text))
        text++;

    // Find JSON start
    const char* json_start = text;
    while(*json_start && *json_start != '{' && *json_start != '[')
        json_start++;

    if(!*json_start) {
        furi_string_free(cleaned_str);
        furi_string_reset(furi_str);
        return;
    }

    bool in_string = false;
    bool in_escape = false;

    while(*text) {
        char c = *text;

        // Handle string literals
        if(c == '"' && !in_escape) {
            in_string = !in_string;
            furi_string_push_back(cleaned_str, c);
        }
        // Handle escape sequences
        else if(c == '\\' && in_string && !in_escape) {
            in_escape = true;
            furi_string_push_back(cleaned_str, c);
        }
        // Handle potential packet markers
        else if(!in_string && (c == '\n' || c == '\r')) {
            // Look ahead for packet marker
            const char* next = text + 1;
            while(*next && isspace((unsigned char)*next))
                next++;

            // Check if next non-space characters are hex digits
            const char* hex_start = next;
            while(*next && isxdigit((unsigned char)*next))
                next++;

            // If we found hex digits followed by space or newline, skip this section
            if(next > hex_start && (!*next || isspace((unsigned char)*next))) {
                // Skip until next non-hex, non-space character
                text = next;
                while(*text && isspace((unsigned char)*text))
                    text++;
                continue;
            } else {
                // Not a packet marker, keep the newline
                furi_string_push_back(cleaned_str, ' ');
            }
        }
        // Copy regular characters
        else {
            if(!in_string && isspace((unsigned char)c)) {
                // Normalize whitespace outside strings
                if(furi_string_size(cleaned_str) > 0 &&
                   !isspace((unsigned char)furi_string_get_char(
                       cleaned_str, furi_string_size(cleaned_str) - 1))) {
                    furi_string_push_back(cleaned_str, ' ');
                }
            } else {
                furi_string_push_back(cleaned_str, c);
            }
        }

        in_escape = (in_escape && c == '\\') ? false : false;
        text++;
    }

    // Trim any trailing whitespace
    while(furi_string_size(cleaned_str) > 0 &&
          isspace((unsigned char)furi_string_get_char(
              cleaned_str, furi_string_size(cleaned_str) - 1))) {
        furi_string_left(cleaned_str, furi_string_size(cleaned_str) - 1);
    }

    furi_string_set(furi_str, cleaned_str);
    furi_string_free(cleaned_str);
}

bool is_json_response(App* app) {
    if(!app || !app->text_box_store) return false;

    FuriString* furi_str = furi_string_alloc_set(app->text_box_store);
    remove_packet_markers(furi_str);

    if(furi_string_empty(furi_str)) {
        furi_string_free(furi_str);
        return false;
    }

    // Debug: Print cleaned string
    const char* cleaned_text = furi_string_get_cstr(furi_str);
    bool result = is_json(cleaned_text);
    furi_string_free(furi_str);
    return result;
}

bool prettify_json(App* app, FuriString* output) {
    size_t indent = 0;
    bool in_string = false;

    for(size_t i = 0; i < furi_string_size(app->text_box_store); i++) {
        char ch = furi_string_get_char(app->text_box_store, i);

        switch(ch) {
        case '{':
        case '[':
            if(!in_string) {
                furi_string_push_back(output, ch);
                // Check for empty array or object
                if(furi_string_get_char(app->text_box_store, i + 1) == '}' ||
                   furi_string_get_char(app->text_box_store, i + 1) == ']') {
                    furi_string_push_back(output, furi_string_get_char(app->text_box_store, ++i));
                } else {
                    furi_string_push_back(output, '\n');
                    indent++;
                    for(size_t j = 0; j < indent; j++) {
                        furi_string_push_back(output, ' ');
                    }
                }
            } else {
                furi_string_push_back(output, ch);
            }
            break;
        case '}':
        case ']':
            if(!in_string) {
                furi_string_push_back(output, '\n');
                indent--;
                for(size_t j = 0; j < indent; j++) {
                    furi_string_push_back(output, ' ');
                }
                furi_string_push_back(output, ch);
            } else {
                furi_string_push_back(output, ch);
            }
            break;
        case ',':
            if(!in_string) {
                furi_string_push_back(output, ch);
                furi_string_push_back(output, '\n');
                for(size_t j = 0; j < indent; j++) {
                    furi_string_push_back(output, ' ');
                }
            } else {
                furi_string_push_back(output, ch);
            }
            break;
        case ':':
            if(!in_string) {
                furi_string_push_back(output, ch);
                furi_string_push_back(output, ' ');
            } else {
                furi_string_push_back(output, ch);
            }
            break;
        case '"':
            furi_string_push_back(output, ch);
            if(i > 0 && furi_string_get_char(app->text_box_store, i - 1) != '\\') {
                in_string = !in_string;
            }
            break;
        default:
            furi_string_push_back(output, ch);
            break;
        }
    }
    return true;
}

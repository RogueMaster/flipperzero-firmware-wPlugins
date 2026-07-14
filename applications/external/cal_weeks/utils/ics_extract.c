#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// =============================================================================
// DATA STRUCTURES
// =============================================================================

// Represents a single calendar event parsed from an ICS file
typedef struct {
    FuriString* summary; // Event title (e.g., "Team Meeting")
    FuriString* location; // Where the event takes place
    FuriString* description; // Additional event details
    DateTime start_time; // When the event starts
    DateTime end_time; // When the event ends
    bool all_day; // True if event spans entire day (no specific time)
} CalendarEvent;

// Dynamic list to store multiple calendar events
typedef struct {
    CalendarEvent* events; // Array of events
    size_t count; // Number of events currently stored
    size_t capacity; // Total allocated space for events
} CalendarEventList;

// =============================================================================
// MEMORY MANAGEMENT
// =============================================================================

// Allocate a new event list with initial capacity
CalendarEventList* calendar_event_list_alloc(size_t initial_capacity) {
    CalendarEventList* list = malloc(sizeof(CalendarEventList));
    list->events = malloc(sizeof(CalendarEvent) * initial_capacity);
    list->count = 0;
    list->capacity = initial_capacity;
    return list;
}

// Free event list and all contained events
void calendar_event_list_free(CalendarEventList* list) {
    // Free all strings in each event
    for(size_t i = 0; i < list->count; i++) {
        furi_string_free(list->events[i].summary);
        furi_string_free(list->events[i].location);
        furi_string_free(list->events[i].description);
    }
    // Free the arrays and list itself
    free(list->events);
    free(list);
}

// =============================================================================
// DATE/TIME PARSING
// =============================================================================

// Parse ICS date/time format: "20241225" or "20241225T153000"
// Returns true if all-day event (no time component), false otherwise
static bool parse_ics_datetime(const char* ics_date, DateTime* dt) {
    if(!ics_date || strlen(ics_date) < 8) return false;

    // Extract date components (YYYYMMDD)
    char year[5] = {0}, month[3] = {0}, day[3] = {0};
    strncpy(year, ics_date, 4);
    strncpy(month, ics_date + 4, 2);
    strncpy(day, ics_date + 6, 2);

    dt->year = atoi(year);
    dt->month = atoi(month);
    dt->day = atoi(day);

    // Check if time component exists (format: ...T153000)
    if(strlen(ics_date) >= 15 && ics_date[8] == 'T') {
        // Extract time components (HHMMSS)
        char hour[3] = {0}, minute[3] = {0}, second[3] = {0};
        strncpy(hour, ics_date + 9, 2);
        strncpy(minute, ics_date + 11, 2);
        strncpy(second, ics_date + 13, 2);

        dt->hour = atoi(hour);
        dt->minute = atoi(minute);
        dt->second = atoi(second);
        return false; // Not an all-day event
    } else {
        // All-day event: set time to midnight
        dt->hour = 0;
        dt->minute = 0;
        dt->second = 0;
        return true; // All-day event
    }
}

// Compare two DateTime objects
// Returns: negative if dt1 < dt2, 0 if equal, positive if dt1 > dt2
static int datetime_compare(const DateTime* dt1, const DateTime* dt2) {
    if(dt1->year != dt2->year) return dt1->year - dt2->year;
    if(dt1->month != dt2->month) return dt1->month - dt2->month;
    if(dt1->day != dt2->day) return dt1->day - dt2->day;
    if(dt1->hour != dt2->hour) return dt1->hour - dt2->hour;
    if(dt1->minute != dt2->minute) return dt1->minute - dt2->minute;
    return dt1->second - dt2->second;
}

// =============================================================================
// ICS FILE PARSING
// =============================================================================

// Extract calendar events from an ICS file within a date range
//
// Parameters:
//   ics_file_path: Path to the .ics file to read
//   date_after: Only include events starting at or after this date
//   date_before: Only include events starting at or before this date
//
// Returns: CalendarEventList containing matching events (empty if none found)
CalendarEventList* extract_calendar_events(
    const char* ics_file_path,
    const DateTime* date_after,
    const DateTime* date_before) {
    CalendarEventList* event_list = calendar_event_list_alloc(10);

    // Open the ICS file
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, ics_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // File doesn't exist or can't be opened - return empty list
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return event_list;
    }

    // Read file character by character to build lines
    // (Flipper's File API doesn't have a read_line function)
    FuriString* line = furi_string_alloc();
    bool in_event = false; // Track if we're inside a VEVENT block
    CalendarEvent current_event = {0};

    char buffer[1];
    while(storage_file_read(file, (uint8_t*)buffer, 1) == 1) {
        char c = buffer[0];

        // Check if we've reached end of line
        if(c == '\n' || c == '\r') {
            const char* line_str = furi_string_get_cstr(line);

            // --- Parse event boundaries ---
            if(strcmp(line_str, "BEGIN:VEVENT") == 0) {
                // Start of new event
                in_event = true;
                current_event.summary = furi_string_alloc();
                current_event.location = furi_string_alloc();
                current_event.description = furi_string_alloc();
                current_event.all_day = false;
            } else if(strcmp(line_str, "END:VEVENT") == 0 && in_event) {
                // End of event - check if it's in our date range
                if(datetime_compare(&current_event.start_time, date_after) >= 0 &&
                   datetime_compare(&current_event.start_time, date_before) <= 0) {
                    // Event is in range - add to list
                    if(event_list->count >= event_list->capacity) {
                        // Need more space - double the capacity
                        event_list->capacity *= 2;
                        event_list->events = realloc(
                            event_list->events, sizeof(CalendarEvent) * event_list->capacity);
                    }
                    event_list->events[event_list->count++] = current_event;
                } else {
                    // Event is out of range - discard it
                    furi_string_free(current_event.summary);
                    furi_string_free(current_event.location);
                    furi_string_free(current_event.description);
                }
                in_event = false;
            }
            // --- Parse event properties ---
            else if(in_event) {
                // SUMMARY: Event title
                if(strncmp(line_str, "SUMMARY:", 8) == 0) {
                    furi_string_set_str(current_event.summary, line_str + 8);
                }
                // LOCATION: Event location
                else if(strncmp(line_str, "LOCATION:", 9) == 0) {
                    furi_string_set_str(current_event.location, line_str + 9);
                }
                // DESCRIPTION: Event details
                else if(strncmp(line_str, "DESCRIPTION:", 12) == 0) {
                    furi_string_set_str(current_event.description, line_str + 12);
                }
                // DTSTART: Event start date/time
                else if(strncmp(line_str, "DTSTART", 7) == 0) {
                    const char* value = strchr(line_str, ':');
                    if(value) {
                        current_event.all_day =
                            parse_ics_datetime(value + 1, &current_event.start_time);
                    }
                }
                // DTEND: Event end date/time
                else if(strncmp(line_str, "DTEND", 5) == 0) {
                    const char* value = strchr(line_str, ':');
                    if(value) {
                        parse_ics_datetime(value + 1, &current_event.end_time);
                    }
                }
            }

            // Reset line buffer for next line
            furi_string_reset(line);
        } else {
            // Add character to current line
            furi_string_push_back(line, c);
        }
    }

    // Cleanup
    furi_string_free(line);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return event_list;
}

# ICS Calendar Event Extractor for Flipper Zero

A comprehensive guide to extracting calendar events from ICS files within a specified date range. It handles both all-day and timed events.

## Data Structures
A calendar event with all its properties can be saved within `CalendarEvent`:
```c
typedef struct {
    FuriString* summary;        // Event title/summary
    FuriString* location;       // Event location
    FuriString* description;    // Event description
    DateTime start_time;        // Event start date/time
    DateTime end_time;          // Event end date/time
    bool all_day;              // True if event is all-day
} CalendarEvent;
```
`CalendarEventList` is a dynamic array container for holding multiple calendar events.
```c
typedef struct {
    CalendarEvent* events;  // Array of events
    size_t count;          // Current number of events
    size_t capacity;       // Allocated capacity
} CalendarEventList;
```
## Memory Management Functions

### calendar_event_list_alloc()

**Purpose:** Creates and initializes a new calendar event list with a specified initial capacity.

**Parameters:**
- `initial_capacity` - Initial size of the events array

**Returns:** Pointer to newly allocated CalendarEventList

**Implementation:**
```c
CalendarEventList* calendar_event_list_alloc(size_t initial_capacity) {
    CalendarEventList* list = malloc(sizeof(CalendarEventList));
    list->events = malloc(sizeof(CalendarEvent) * initial_capacity);
    list->count = 0;
    list->capacity = initial_capacity;
    return list;
}
```

**Explanation:** 
- Allocates memory for the list structure
- Allocates an array to hold events
- Initializes count to 0 (no events yet)
- Sets the capacity for future growth

### calendar_event_list_free()

**Purpose:** Properly frees all memory associated with a calendar event list, including all strings within each event.

**Parameters:**
- `list` - Pointer to the CalendarEventList to free

```c
void calendar_event_list_free(CalendarEventList* list) {
    for(size_t i = 0; i < list->count; i++) {
        furi_string_free(list->events[i].summary);
        furi_string_free(list->events[i].location);
        furi_string_free(list->events[i].description);
    }
    free(list->events);
    free(list);
}
```

**Explanation:**
- Iterates through all events in the list
- Frees each FuriString (summary, location, description) for each event
- Frees the events array itself
- Finally frees the list structure
- **Important:** Always call this when done with the event list to prevent memory leaks

## Parsing Functions

### parse_ics_datetime()

**Purpose:** Converts an ICS date/datetime string into a Flipper Zero DateTime structure.

**Parameters:**
- `ics_date` - String in ICS format (YYYYMMDD or YYYYMMDDTHHMMSS)
- `dt` - Pointer to DateTime structure to populate

**Returns:** 
- `true` if the event is all-day (no time component)
- `false` if the event has a specific time

**Supported Formats:**
- `YYYYMMDD` - Date only (all-day event)
- `YYYYMMDDTHHMMSS` - Date and time
- `YYYYMMDDTHHMMSSZ` - Date and time in UTC (Z is ignored)

```c
static bool parse_ics_datetime(const char* ics_date, DateTime* dt) {
    if(!ics_date || strlen(ics_date) < 8) return false;
    
    // Parse YYYYMMDD
    char year[5] = {0}, month[3] = {0}, day[3] = {0};
    strncpy(year, ics_date, 4);
    strncpy(month, ics_date + 4, 2);
    strncpy(day, ics_date + 6, 2);
    
    dt->year = atoi(year);
    dt->month = atoi(month);
    dt->day = atoi(day);
    
    // Check if it includes time (YYYYMMDDTHHMMSS)
    if(strlen(ics_date) >= 15 && ics_date[8] == 'T') {
        char hour[3] = {0}, minute[3] = {0}, second[3] = {0};
        strncpy(hour, ics_date + 9, 2);
        strncpy(minute, ics_date + 11, 2);
        strncpy(second, ics_date + 13, 2);
        
        dt->hour = atoi(hour);
        dt->minute = atoi(minute);
        dt->second = atoi(second);
        return false; // Not all-day
    } else {
        dt->hour = 0;
        dt->minute = 0;
        dt->second = 0;
        return true; // All-day event
    }
}
```

**Explanation:**
- First extracts the date portion (year, month, day) using fixed positions
- Checks if there's a 'T' at position 8, indicating time follows
- If time present, extracts hour, minute, second
- Returns whether event is all-day based on presence of time component
- Sets time to 00:00:00 for all-day events

**Example ICS dates:**
- `20241225` → December 25, 2024 (all-day)
- `20241225T143000` → December 25, 2024 at 2:30 PM

### datetime_compare()

**Purpose:** Compares two DateTime structures to determine their chronological order.

**Parameters:**
- `dt1` - First DateTime to compare
- `dt2` - Second DateTime to compare

**Returns:**
- Negative value if dt1 < dt2 (dt1 is earlier)
- 0 if dt1 == dt2 (same date/time)
- Positive value if dt1 > dt2 (dt1 is later)

```c
static int datetime_compare(const DateTime* dt1, const DateTime* dt2) {
    if(dt1->year != dt2->year) return dt1->year - dt2->year;
    if(dt1->month != dt2->month) return dt1->month - dt2->month;
    if(dt1->day != dt2->day) return dt1->day - dt2->day;
    if(dt1->hour != dt2->hour) return dt1->hour - dt2->hour;
    if(dt1->minute != dt2->minute) return dt1->minute - dt2->minute;
    return dt1->second - dt2->second;
}
```

**Explanation:**
- Compares date/time components in hierarchical order (year → month → day → hour → minute → second)
- Returns immediately when a difference is found
- Used for filtering events within the specified date range
- Standard comparison function pattern for sorting and filtering

## Main Extraction Function

### extract_calendar_events()

**Purpose:** The primary function that reads an ICS file and extracts all events within a specified date range.

**Parameters:**
- `ics_file_path` - Full path to the ICS file (e.g., "/ext/apps_data/cal_weeks/calendar.ics")
- `date_after` - Start of date range (inclusive)
- `date_before` - End of date range (inclusive)

**Returns:** CalendarEventList containing all events within the date range

```c
CalendarEventList* extract_calendar_events(
    const char* ics_file_path,
    const DateTime* date_after,
    const DateTime* date_before) {
    
    CalendarEventList* event_list = calendar_event_list_alloc(10);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    if(!storage_file_open(file, ics_file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return event_list;
    }
    
    FuriString* line = furi_string_alloc();
    bool in_event = false;
    CalendarEvent current_event = {0};
    
    while(storage_file_read(file, (uint8_t*)furi_string_get_cstr(line), 1) == 1) {
        // Read line by line
        char c = furi_string_get_char(line, 0);
        if(c == '\n' || c == '\r') {
            const char* line_str = furi_string_get_cstr(line);
            
            if(strcmp(line_str, "BEGIN:VEVENT") == 0) {
                in_event = true;
                current_event.summary = furi_string_alloc();
                current_event.location = furi_string_alloc();
                current_event.description = furi_string_alloc();
                current_event.all_day = false;
            }
            else if(strcmp(line_str, "END:VEVENT") == 0 && in_event) {
                // Check if event is within date range
                if(datetime_compare(&current_event.start_time, date_after) >= 0 &&
                   datetime_compare(&current_event.start_time, date_before) <= 0) {
                    
                    // Add event to list
                    if(event_list->count >= event_list->capacity) {
                        event_list->capacity *= 2;
                        event_list->events = realloc(
                            event_list->events,
                            sizeof(CalendarEvent) * event_list->capacity);
                    }
                    event_list->events[event_list->count++] = current_event;
                } else {
                    // Event outside range, free strings
                    furi_string_free(current_event.summary);
                    furi_string_free(current_event.location);
                    furi_string_free(current_event.description);
                }
                in_event = false;
            }
            else if(in_event) {
                // Parse event properties
                if(strncmp(line_str, "SUMMARY:", 8) == 0) {
                    furi_string_set_str(current_event.summary, line_str + 8);
                }
                else if(strncmp(line_str, "LOCATION:", 9) == 0) {
                    furi_string_set_str(current_event.location, line_str + 9);
                }
                else if(strncmp(line_str, "DESCRIPTION:", 12) == 0) {
                    furi_string_set_str(current_event.description, line_str + 12);
                }
                else if(strncmp(line_str, "DTSTART", 7) == 0) {
                    const char* value = strchr(line_str, ':');
                    if(value) {
                        current_event.all_day = parse_ics_datetime(value + 1, &current_event.start_time);
                    }
                }
                else if(strncmp(line_str, "DTEND", 5) == 0) {
                    const char* value = strchr(line_str, ':');
                    if(value) {
                        parse_ics_datetime(value + 1, &current_event.end_time);
                    }
                }
            }
            
            furi_string_reset(line);
        } else {
            furi_string_push_back(line, c);
        }
    }
    
    furi_string_free(line);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    
    return event_list;
}
```

## Usage Example

```c
// Define your date range
DateTime start_date = {
    .year = 2024,
    .month = 12,
    .day = 1,
    .hour = 0,
    .minute = 0,
    .second = 0
};

DateTime end_date = {
    .year = 2024,
    .month = 12,
    .day = 31,
    .hour = 23,
    .minute = 59,
    .second = 59
};

// Extract events from ICS file
CalendarEventList* events = extract_calendar_events(
    "/ext/apps_data/cal_weeks/calendar.ics",
    &start_date,
    &end_date
);

// Process the events
for(size_t i = 0; i < events->count; i++) {
    CalendarEvent* event = &events->events[i];
    
    // Access event data
    const char* title = furi_string_get_cstr(event->summary);
    const char* location = furi_string_get_cstr(event->location);    
    // Display or process event
    FURI_LOG_I("Calendar", "Event: %s at %s", title, location);
    FURI_LOG_I("Calendar", "Date: %04d-%02d-%02d %02d:%02d",
        event->start_time.year,
        event->start_time.month,
        event->start_time.day,
        event->start_time.hour,
        event->start_time.minute
    );   
    if(event->all_day) {
        FURI_LOG_I("Calendar", "All-day event");
    }
}
calendar_event_list_free(events);
```

## Limitations and Considerations

1. **No Recurring Events:** Does not handle RRULE (recurrence rules). Each occurrence of a recurring event would need to be explicitly listed in the ICS file.

2. **No Line Continuations:** ICS files can have long lines split across multiple lines (continuation lines start with space/tab). This implementation doesn't handle that.

3. **No Timezone Support:** TZID parameters are ignored. All times are treated as-is.

4. **Basic Property Parsing:** Doesn't handle `ATTENDEES`, `ORGANIZER`, `STATUS`, `PRIORITY`, `CATEGORIES`, ...

5. **No VALARM:** Alarm/reminder information is not extracted.

#ifndef ICS_EXTRACT_H
#define ICS_EXTRACT_H

#include <furi.h>
#include <furi_hal_rtc.h>
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
// FUNCTION DECLARATIONS
// =============================================================================

// Allocate a new event list with initial capacity
CalendarEventList* calendar_event_list_alloc(size_t initial_capacity);

// Free event list and all contained events
void calendar_event_list_free(CalendarEventList* list);

// Extract calendar events from an ICS file within a date range
CalendarEventList* extract_calendar_events(
    const char* ics_file_path,
    const DateTime* date_after,
    const DateTime* date_before);

#endif // ICS_EXTRACT_H

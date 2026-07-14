/*
 * Guido Music Score Reader for Flipper Zero
 * 
 * This application reads GUIDO music notation format files and plays them
 * using the Flipper Zero's built-in speaker. It supports multi-voice playback
 * by rapidly alternating between voices to create a pseudo-polyphonic effect.
 */

#include <furi.h> // Core Flipper Zero OS functions
#include <furi_hal.h> // Hardware abstraction layer (speaker, etc.)
#include <gui/gui.h> // GUI system for drawing
#include <gui/elements.h> // GUI element helpers
#include <storage/storage.h> // File system access
#include <dialogs/dialogs.h> // File browser and dialog boxes
#include <notification/notification.h> // Notification system (LED, vibration, etc.)
#include <ctype.h> // Character type functions (tolower, etc.)

#define TAG             "GuidoPlayer" // Logging tag for debug output
#define MAX_NOTES       256 // Maximum number of notes that can be stored per voice
#define MAX_VOICES      2 // Maximum number of simultaneous voices (limited by monophonic speaker)
#define GUIDO_FILE_PATH "/ext/data/pachelbel_canon.gmn"
#define GUIDO_FILE_DIR  "/ext/data" // Default directory for file browser
#define GUIDO_FILE_EXT  ".gmn" // File extension filter for browser

// ============================================================================
// Data Structures
// ============================================================================

/**
 * Represents a single musical note with its frequency and duration
 */
typedef struct {
    float frequency; // Frequency in Hz (e.g., 440.0 for A4)
    uint32_t duration_ms; // Duration in milliseconds
} Note;

/**
 * Represents a voice (musical line) containing multiple notes
 */
typedef struct {
    Note notes[MAX_NOTES]; // Array of notes in this voice
    size_t note_count; // Number of notes currently stored
} Voice;

/**
 * Main application state containing all voices and playback information
 */
typedef struct {
    Voice voices[MAX_VOICES]; // Array of voices (typically 2)
    size_t voice_count; // Number of voices in current piece
    bool playing; // Playback status flag
    size_t current_note[MAX_VOICES]; // Current note index for each voice
    FuriString* file_path; // Path to currently loaded file
    FuriString* status; // Status message for display
} GuidoState;

// ============================================================================
// note_to_frequency
// ============================================================================
/**
 * Convert a musical note to its frequency in Hz
 * 
 * Uses equal temperament tuning with A4 = 440Hz as reference.
 * The function handles note names (a-g), octaves, and sharps.
 * 
 * @param note_name The note letter (a, b, c, d, e, f, g)
 * @param octave The octave number (can be negative for bass notes)
 * @param sharp Whether the note is sharp (raised by a semitone)
 * @return Frequency in Hz, or 0 if note is invalid
 */
static float note_to_frequency(char note_name, int octave, bool sharp) {
    // Base frequencies for octave 4 in equal temperament
    // C4 = 261.63 Hz as reference point
    float base_freqs[] = {
        261.63, // C
        293.66, // D
        329.63, // E
        349.23, // F
        392.00, // G
        440.00, // A
        493.88 // B
    };

    // Map note letter to index in frequency array
    int note_index = 0;
    switch(note_name) {
    case 'c':
        note_index = 0;
        break;
    case 'd':
        note_index = 1;
        break;
    case 'e':
        note_index = 2;
        break;
    case 'f':
        note_index = 3;
        break;
    case 'g':
        note_index = 4;
        break;
    case 'a':
        note_index = 5;
        break;
    case 'b':
        note_index = 6;
        break;
    default:
        FURI_LOG_W(TAG, "Invalid note name: %c", note_name);
        return 0;
    }

    float freq = base_freqs[note_index];
    FURI_LOG_D(TAG, "Base frequency for %c: %.2f Hz", note_name, (double)freq);

    // Adjust for octave
    // Each octave doubles (up) or halves (down) the frequency
    int octave_diff = octave - 4;
    for(int i = 0; i < abs(octave_diff); i++) {
        if(octave_diff > 0)
            freq *= 2.0;
        else
            freq /= 2.0;
    }

    // Adjust for sharp (semitone up)
    // One semitone = multiply by 2^(1/12) ≈ 1.059463
    if(sharp) {
        freq *= 1.059463;
        FURI_LOG_D(TAG, "Applied sharp: %.2f Hz", (double)freq);
    }

    FURI_LOG_D(
        TAG,
        "Final frequency for %c%s%d: %.2f Hz",
        note_name,
        sharp ? "#" : "",
        octave,
        (double)freq);

    return freq;
}

// ============================================================================
// parse_duration
// ============================================================================
/**
 * Parse a GUIDO duration string and convert to milliseconds
 * 
 * GUIDO uses fractional notation like "1/4" for quarter notes.
 * Assumes tempo of 60 BPM where quarter note = 1000ms.
 * 
 * @param dur_str Duration string (e.g., "1/4", "1/2")
 * @return Duration in milliseconds
 */
static uint32_t parse_duration(const char* dur_str) {
    int numerator = 1, denominator = 4;

    // Parse the fraction (e.g., "1/4" -> numerator=1, denominator=4)
    if(sscanf(dur_str, "%d/%d", &numerator, &denominator) >= 2) {
        // Calculate duration based on 60 BPM (quarter note = 1000ms)
        // Whole note (4/4) = 4000ms, half note (2/4) = 2000ms, etc.
        uint32_t duration = (uint32_t)((4000.0 * numerator) / denominator);
        FURI_LOG_D(TAG, "Parsed duration %s -> %lu ms", dur_str, duration);
        return duration;
    }

    FURI_LOG_W(TAG, "Failed to parse duration: %s, using default 1000ms", dur_str);
    return 1000; // Default to quarter note
}

// ============================================================================
// parse_guido_file
// ============================================================================
/**
 * Parse a GUIDO music notation file and extract notes
 * 
 * This is a simplified parser that handles basic GUIDO notation:
 * - Note format: letter[#]octave/duration (e.g., "d1/2", "f#1/4")
 * - Voice separation: \staff<2> for second voice
 * - Supports sharps (#) and negative octaves for bass notes
 * 
 * @param storage Pointer to storage API
 * @param path Path to the GUIDO file
 * @param state Pointer to application state to populate
 * @return true if parsing succeeded, false otherwise
 */
static bool parse_guido_file(Storage* storage, const char* path, GuidoState* state) {
    FURI_LOG_I(TAG, "Attempting to parse file: %s", path);

    File* file = storage_file_alloc(storage);

    // Open the file for reading
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file: %s", path);
        storage_file_free(file);
        return false;
    }

    // Read entire file into buffer
    char buffer[2048];
    size_t bytes_read = storage_file_read(file, buffer, sizeof(buffer) - 1);
    buffer[bytes_read] = '\0';
    FURI_LOG_I(TAG, "Read %d bytes from file", bytes_read);

    storage_file_close(file);
    storage_file_free(file);

    // Parse the buffer character by character
    char* ptr = buffer;
    int current_voice = 0; // Start with voice 0 (first staff)

    FURI_LOG_I(TAG, "Beginning parse loop...");
    FURI_LOG_D(TAG, "First 100 chars: %.100s", buffer);

    while(*ptr && current_voice < MAX_VOICES) {
        // Skip whitespace, bar lines, commas, and newlines
        while(*ptr && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r' ||
                       *ptr == '|' || *ptr == ',')) {
            ptr++;
        }

        if(!*ptr) break; // End of file

        // Skip GUIDO comments (* ... *)
        if(*ptr == '(' && *(ptr + 1) == '*') {
            FURI_LOG_D(TAG, "Skipping comment");
            ptr += 2;
            // Find end of comment
            while(*ptr && !(*ptr == '*' && *(ptr + 1) == ')')) {
                ptr++;
            }
            if(*ptr) ptr += 2; // Skip past *)
            continue;
        }

        // Skip opening braces and brackets
        if(*ptr == '{' || *ptr == '[' || *ptr == ']' || *ptr == '}') {
            ptr++;
            continue;
        }

        // Check for voice/staff marker (e.g., \staff<2>)
        if(*ptr == '\\') {
            if(strncmp(ptr, "\\staff<2>", 9) == 0) {
                current_voice = 1; // Switch to second voice
                ptr += 9;
                FURI_LOG_I(TAG, "Switched to voice 2 (staff 2)");
                continue;
            } else if(strncmp(ptr, "\\staff<1>", 9) == 0) {
                current_voice = 0; // Explicitly set to voice 1
                ptr += 9;
                FURI_LOG_I(TAG, "Set to voice 1 (staff 1)");
                continue;
            } else {
                // Skip other GUIDO commands like \title, \composer, \tempo, \key, \meter, \clef
                FURI_LOG_D(TAG, "Skipping GUIDO command starting with \\");
                while(*ptr && *ptr != '>' && *ptr != '\n')
                    ptr++;
                if(*ptr == '>') ptr++;
                continue;
            }
        }

        // Parse a note (format: letter[#]octave/duration)
        if((*ptr >= 'a' && *ptr <= 'g') || (*ptr >= 'A' && *ptr <= 'G')) {
            char note_name = tolower(*ptr); // Convert to lowercase
            ptr++;
            FURI_LOG_D(TAG, "Found note: %c", note_name);
            bool sharp = false;

            // Check for sharp symbol
            if(*ptr == '#') {
                sharp = true;
                ptr++;
                FURI_LOG_D(TAG, "Found sharp");
            }

            // Parse octave number (can be negative for bass)
            int octave = 1;
            if(*ptr == '-') {
                // Negative octave (e.g., "-1")
                ptr++;
                octave = -(*ptr++ - '0');
                FURI_LOG_D(TAG, "Negative octave: %d", octave);
            } else if(*ptr >= '0' && *ptr <= '9') {
                octave = *ptr++ - '0';
            }

            // Parse duration (e.g., "/4" for quarter note)
            if(*ptr == '/') {
                ptr++;
                char dur_buf[16] = {0};
                int dur_idx = 0;

                // Build duration string "1/X"
                dur_buf[dur_idx++] = '1';
                dur_buf[dur_idx++] = '/';
                while(*ptr >= '0' && *ptr <= '9' && dur_idx < 15) {
                    dur_buf[dur_idx++] = *ptr++;
                }

                FURI_LOG_D(TAG, "Duration string: %s", dur_buf);

                // Add note to current voice
                Voice* voice = &state->voices[current_voice];
                if(voice->note_count < MAX_NOTES) {
                    voice->notes[voice->note_count].frequency =
                        note_to_frequency(note_name, octave, sharp);
                    voice->notes[voice->note_count].duration_ms = parse_duration(dur_buf);

                    FURI_LOG_D(
                        TAG,
                        "Added note to voice %d: %c%s%d, %.2f Hz, %lu ms",
                        current_voice,
                        note_name,
                        sharp ? "#" : "",
                        octave,
                        (double)voice->notes[voice->note_count].frequency,
                        voice->notes[voice->note_count].duration_ms);

                    voice->note_count++;
                } else {
                    FURI_LOG_W(TAG, "Voice %d is full, skipping note", current_voice);
                }
            } else {
                FURI_LOG_W(
                    TAG,
                    "Note %c%d has no duration (next char: '%c'), skipping",
                    note_name,
                    octave,
                    *ptr ? *ptr : '?');
            }
        }
    }

    // Store total number of voices found
    state->voice_count = current_voice + 1;

    FURI_LOG_I(TAG, "Parsing complete!");
    FURI_LOG_I(TAG, "Total voices: %d", state->voice_count);
    FURI_LOG_I(TAG, "Voice 1: %d notes", state->voices[0].note_count);
    if(state->voice_count > 1) {
        FURI_LOG_I(TAG, "Voice 2: %d notes", state->voices[1].note_count);
    }

    return state->voices[0].note_count > 0;
}

// ============================================================================
// playback_thread
// ============================================================================
/**
 * Playback thread that plays multiple voices in parallel
 * 
 * Since the Flipper Zero has a monophonic speaker, this function creates
 * a pseudo-polyphonic effect by rapidly alternating between voices.
 * Each voice gets a time slice proportional to the total number of voices.
 * 
 * @param context Pointer to GuidoState
 * @return Thread exit code (always 0)
 */
static int32_t playback_thread(void* context) {
    GuidoState* state = (GuidoState*)context;

    FURI_LOG_I(TAG, "Playback thread started");

    // Initialize playback state
    state->playing = true;
    for(int i = 0; i < MAX_VOICES; i++) {
        state->current_note[i] = 0;
    }

    bool any_notes_remaining = true;

    // Main playback loop - continues until all voices finish or stopped
    while(state->playing && any_notes_remaining) {
        any_notes_remaining = false;

        // Iterate through each voice and play a portion of the current note
        for(size_t v = 0; v < state->voice_count; v++) {
            Voice* voice = &state->voices[v];

            // Check if this voice still has notes to play
            if(state->current_note[v] < voice->note_count) {
                any_notes_remaining = true;
                Note* note = &voice->notes[state->current_note[v]];

                // Only play if frequency is valid (> 0)
                if(note->frequency > 0) {
                    // Calculate how long to play this note chunk
                    // Split the duration among all voices for pseudo-polyphony
                    uint32_t play_duration = note->duration_ms / state->voice_count;

                    FURI_LOG_D(
                        TAG,
                        "Voice %d: Playing %.2f Hz for %lu ms",
                        v,
                        (double)note->frequency,
                        play_duration);

                    // Acquire speaker (with 1 second timeout)
                    if(furi_hal_speaker_acquire(1000)) {
                        // Play the note at 50% volume
                        furi_hal_speaker_start(note->frequency, 0.5);
                        furi_delay_ms(play_duration);
                        furi_hal_speaker_stop();
                        furi_hal_speaker_release();
                    } else {
                        FURI_LOG_W(TAG, "Failed to acquire speaker");
                    }
                }

                // Track note timing to determine when to advance
                static uint32_t note_start[MAX_VOICES] = {0};
                if(note_start[v] == 0) {
                    note_start[v] = furi_get_tick();
                    FURI_LOG_D(TAG, "Voice %d: Started note %d", v, state->current_note[v]);
                }

                // Check if this note's full duration has elapsed
                if(furi_get_tick() - note_start[v] >= note->duration_ms) {
                    state->current_note[v]++;
                    note_start[v] = 0;
                    FURI_LOG_D(
                        TAG,
                        "Voice %d: Advanced to note %d/%d",
                        v,
                        state->current_note[v],
                        voice->note_count);
                }
            }
        }

        // Small delay to prevent tight loop
        furi_delay_ms(10);
    }

    state->playing = false;
    FURI_LOG_I(TAG, "Playback thread finished");

    return 0;
}

// ============================================================================
// draw_callback
// ============================================================================
/**
 * GUI draw callback - renders the application interface
 * 
 * Displays:
 * - Application title
 * - Current file name
 * - Number of voices
 * - Playback progress for each voice
 * - Current status message
 * - Playback indicator
 * 
 * @param canvas Pointer to canvas for drawing
 * @param context Pointer to GuidoState
 */
static void draw_callback(Canvas* canvas, void* context) {
    GuidoState* state = (GuidoState*)context;

    canvas_clear(canvas);

    // Draw title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Guido Player");

    canvas_set_font(canvas, FontSecondary);

    // Display loaded file information
    if(furi_string_size(state->file_path) > 0) {
        canvas_draw_str(canvas, 2, 22, "File loaded");
        canvas_draw_str(canvas, 2, 32, furi_string_get_cstr(state->file_path));
    } else {
        canvas_draw_str(canvas, 2, 22, "No file loaded");
    }

    // Display voice count
    char info[64];
    snprintf(info, sizeof(info), "Voices: %d", state->voice_count);
    canvas_draw_str(canvas, 2, 44, info);

    // Display progress for each voice
    for(size_t v = 0; v < state->voice_count; v++) {
        snprintf(
            info,
            sizeof(info),
            "V%d: %d/%d notes",
            v + 1,
            state->current_note[v],
            state->voices[v].note_count);
        canvas_draw_str(canvas, 2, 54 + v * 10, info);
    }

    // Display status message
    canvas_draw_str(canvas, 2, 54, furi_string_get_cstr(state->status));

    // Display playback indicator
    if(state->playing) {
        canvas_draw_str(canvas, 80, 54, "[Playing]");
    }
}

// ============================================================================
// input_callback
// ============================================================================
/**
 * GUI input callback - handles button presses
 * 
 * Forwards input events to the message queue for processing
 * in the main loop.
 * 
 * @param event Pointer to input event
 * @param context Pointer to message queue
 */
static void input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* queue = (FuriMessageQueue*)context;
    furi_message_queue_put(queue, event, 0);
}

// ============================================================================
// guido_main
// ============================================================================
/**
 * Main entry point for the Guido Player application
 * 
 * Initializes the GUI, handles user input, and manages the playback thread.
 * 
 * Button Controls:
 * - OK: Open file browser to load a GUIDO file
 * - UP: Start playback
 * - DOWN: Stop playback
 * - BACK: Exit application
 * 
 * @param p Unused parameter
 * @return Exit code (always 0)
 */
int32_t guido_main(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "Guido Player starting...");

    // Allocate and initialize application state
    GuidoState* state = malloc(sizeof(GuidoState));
    memset(state, 0, sizeof(GuidoState));
    state->file_path = furi_string_alloc();
    state->status = furi_string_alloc();
    furi_string_set(state->status, "Press OK to load file");

    FURI_LOG_D(TAG, "State initialized");

    // Create message queue for input events
    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Create and configure viewport
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, state);
    view_port_input_callback_set(view_port, input_callback, queue);

    // Register GUI and viewport
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FURI_LOG_D(TAG, "GUI initialized");

    // Open required system services
    Storage* storage = furi_record_open(RECORD_STORAGE);
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    FuriThread* playback = NULL;
    InputEvent event;
    bool running = true;

    FURI_LOG_I(TAG, "Entering main event loop");

    // Auto-load default file if it exists
    FURI_LOG_I(TAG, "Attempting to auto-load default file: %s", GUIDO_FILE_PATH);
    if(storage_file_exists(storage, GUIDO_FILE_PATH)) {
        furi_string_set(state->file_path, GUIDO_FILE_PATH);

        if(parse_guido_file(storage, GUIDO_FILE_PATH, state)) {
            furi_string_set(state->status, "Loaded! Press UP to play");
            FURI_LOG_I(TAG, "Default file loaded successfully");
        } else {
            furi_string_set(state->status, "Parse failed. Press OK");
            FURI_LOG_E(TAG, "Failed to parse default file");
        }
    } else {
        FURI_LOG_W(TAG, "Default file not found: %s", GUIDO_FILE_PATH);
        furi_string_set(state->status, "Press OK to load file");
    }
    view_port_update(view_port);

    // Main event loop
    while(running) {
        // Wait for input event (with 100ms timeout)
        if(furi_message_queue_get(queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                FURI_LOG_D(TAG, "Button pressed: %d", event.key);

                // Handle BACK button - exit application
                if(event.key == InputKeyBack) {
                    FURI_LOG_I(TAG, "Back button pressed, exiting...");
                    running = false;

                    // Handle OK button - open file browser
                } else if(event.key == InputKeyOk) {
                    FURI_LOG_I(TAG, "OK button pressed, opening file browser");

                    // Create path for file picker
                    FuriString* path = furi_string_alloc();
                    furi_string_set(path, GUIDO_FILE_DIR);

                    // Configure file browser options
                    DialogsFileBrowserOptions browser_options;
                    dialog_file_browser_set_basic_options(&browser_options, GUIDO_FILE_EXT, NULL);
                    browser_options.hide_ext = false; // Show file extensions

                    // Show file browser
                    if(dialog_file_browser_show(dialogs, path, path, &browser_options)) {
                        FURI_LOG_I(TAG, "File selected: %s", furi_string_get_cstr(path));
                        furi_string_set(state->file_path, path);

                        // Reset and parse the new file
                        memset(state->voices, 0, sizeof(state->voices));
                        state->voice_count = 0;

                        if(parse_guido_file(storage, furi_string_get_cstr(path), state)) {
                            furi_string_set(state->status, "Loaded! Press Play");
                            FURI_LOG_I(TAG, "File parsed successfully");
                        } else {
                            furi_string_set(state->status, "Parse failed");
                            FURI_LOG_E(TAG, "File parsing failed");
                        }
                    } else {
                        FURI_LOG_I(TAG, "File selection cancelled");
                    }

                    furi_string_free(path);
                    view_port_update(view_port);

                    // Handle UP button - start playback
                } else if(
                    event.key == InputKeyUp && state->voices[0].note_count > 0 &&
                    !state->playing) {
                    FURI_LOG_I(TAG, "UP button pressed, starting playback");

                    furi_string_set(state->status, "Playing...");

                    // Create and start playback thread
                    playback = furi_thread_alloc();
                    furi_thread_set_name(playback, "GuidoPlayback");
                    furi_thread_set_stack_size(playback, 2048);
                    furi_thread_set_callback(playback, playback_thread);
                    furi_thread_set_context(playback, state);
                    furi_thread_start(playback);

                    FURI_LOG_I(TAG, "Playback thread started");

                    // Handle DOWN button - stop playback
                } else if(event.key == InputKeyDown && state->playing) {
                    FURI_LOG_I(TAG, "DOWN button pressed, stopping playback");

                    // Signal playback thread to stop
                    state->playing = false;

                    if(playback) {
                        furi_thread_join(playback);
                        furi_thread_free(playback);
                        playback = NULL;
                        FURI_LOG_I(TAG, "Playback thread stopped");
                    }

                    furi_string_set(state->status, "Stopped");
                }
            } else if(event.type == InputTypeLong && event.key == InputKeyBack) {
                // Handle LONG BACK button press - force exit even during playback
                FURI_LOG_I(TAG, "Long back button pressed, forcing exit...");
                running = false;
            }
        }

        // Update display
        view_port_update(view_port);
    }

    FURI_LOG_I(TAG, "Cleaning up...");

    // Ensure playback is stopped before cleanup
    if(playback) {
        FURI_LOG_D(TAG, "Stopping playback thread");
        state->playing = false;
        furi_thread_join(playback);
        furi_thread_free(playback);
    }

    // Close system services
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    // Clean up GUI
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);

    // Free application state
    furi_string_free(state->file_path);
    furi_string_free(state->status);
    free(state);

    FURI_LOG_I(TAG, "Guido Player exited cleanly");

    return 0;
}

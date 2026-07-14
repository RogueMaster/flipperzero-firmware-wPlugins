/*
 * ============================================================================
 * FLIPPER ZERO - NORTHERN HEMISPHERE STAR MAP SCROLLER
 * 
 * A minimal star map viewer for navigating a polar-projected map of the
 * Northern Hemisphere night sky. The map consists of 50 tiles (5x10 grid)
 * showing stars from magnitude 1-6.
 * 
 * Tile Numbering:
 * - Images named 00.png through 49.png
 * - Numbered left-to-right, top-to-bottom
 * - 5 columns × 10 rows
 * - Center tile: 27 (row 5, col 2) - Polaris location
 * 
 * Author: F Greil
 * ============================================================================
 */

/* ============================================================================
 * SYSTEM INCLUDES
 * ============================================================================ */

#include <furi.h> // Core Flipper OS functions (includes FuriString)
#include <furi_hal.h> // Hardware abstraction layer
#include <gui/gui.h> // GUI rendering and canvas
#include <gui/icon.h> // Icon/image loading
#include <input/input.h> // Input handling (buttons)
#include <storage/storage.h> // SD card file access

/* ============================================================================
 * CONFIGURATION CONSTANTS
 * ============================================================================ */
#define SCREEN_WIDTH  128 // Screen width in pixels
#define SCREEN_HEIGHT 64 // Screen height in pixels
#define TILE_WIDTH    128 // Each tile is 128px wide
#define TILE_HEIGHT   64 // Each tile is 64px tall

// Map configuration
#define TILE_COLS   5 // Number of tile columns
#define TILE_ROWS   10 // Number of tile rows
#define TOTAL_TILES (TILE_COLS * TILE_ROWS) // Total tiles: 50
#define MAP_WIDTH   (TILE_COLS * TILE_WIDTH) // Map width: 640px
#define MAP_HEIGHT  (TILE_ROWS * TILE_HEIGHT) // Map height: 640px

// Camera bounds (allow scrolling beyond edges to reach all map pixels)
#define CAMERA_MIN_X (-(SCREEN_WIDTH / 2)) // Can scroll 64px left of map
#define CAMERA_MAX_X (MAP_WIDTH - SCREEN_WIDTH / 2) // Can scroll 64px right of map
#define CAMERA_MIN_Y (-(SCREEN_HEIGHT / 2)) // Can scroll 32px above map
#define CAMERA_MAX_Y (MAP_HEIGHT - SCREEN_HEIGHT / 2) // Can scroll 32px below map

// Cursor settings
#define CURSOR_RADIUS 10 // Cursor circle radius (8px diameter)

// Memory limits
#define MAX_ANNOTATIONS       200 // Maximum number of star annotations
#define MAX_ANNOTATION_LENGTH 64 // Maximum length of star name
#define MAX_ERROR_MESSAGE     64 // Maximum length of error message

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * @brief Represents a star annotation at a specific location
 * 
 * Annotations mark the positions of stars on the map. Each annotation
 * references a tile by number (0-49) and has x,y coordinates within that tile.
 */
typedef struct {
    int tile_number; // Tile number (0-49)
    int x; // X coordinate within tile (0-127)
    int y; // Y coordinate within tile (0-63)
    char text[MAX_ANNOTATION_LENGTH]; // Star name (e.g., "Polaris (α UMi)")
} Annotation;

/**
 * @brief Main application state
 * 
 * Contains all the data needed for the star map viewer including:
 * - GUI components (viewport, event queue)
 * - Camera position for scrolling
 * - Star annotations
 * - Current selection state
 */
typedef struct {
    // GUI components
    ViewPort* view_port; // Flipper viewport for rendering
    FuriMessageQueue* event_queue; // Queue for input events

    // Camera/viewport position (in world coordinates)
    float camera_x; // Camera X position (0 to MAP_WIDTH-SCREEN_WIDTH)
    float camera_y; // Camera Y position (0 to MAP_HEIGHT-SCREEN_HEIGHT)

    // Star annotations
    Annotation annotations[MAX_ANNOTATIONS]; // Array of all star annotations
    int annotation_count; // Number of annotations loaded
    char current_annotation[MAX_ANNOTATION_LENGTH]; // Currently displayed star name
    bool has_annotation; // True if cursor is over a star

    int current_tile; // Tile number under cursor
    bool show_tile_name; // Toggle for tile name display
    bool has_error; // True if there's an error to display
    char error_message[MAX_ERROR_MESSAGE]; // Error message to show
} ScrollerState;

/* ============================================================================
 * HELPER FUNCTIONS 
 * ============================================================================ */

static void draw_simple_modal(Canvas* canvas, const char* text) {
    canvas_set_font(canvas, FontPrimary);
    const int box_w = canvas_string_width(canvas, text) + 6;
    const int box_h = 20;
    const int box_x = 2;
    const int box_y = 20;
    // White filled rectangle
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, box_x, box_y, box_w, box_h);
    // Black border
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, box_x, box_y, box_w, box_h);
    // Text inside
    canvas_draw_str(canvas, box_x + 3, box_y + 13, text);
    canvas_set_font(canvas, FontSecondary);
}

static int row_col_to_tile_num(int row, int col) {
    return row * TILE_COLS + col;
}

/**
 * @brief Load and draw a tile bitmap from file
 * 
 * Loads a 128x64 monochrome BMP file and draws it to the canvas.
 * 
 * @param canvas    Canvas to draw on
 * @param tile_num  Tile number (0-49)
 * @param x         X position to draw at
 * @param y         Y position to draw at
 * @return          true if loaded and drawn successfully
 */
static bool draw_tile_bmp(Canvas* canvas, int tile_num, int x, int y) {
    // Build file path: /ext/apps_assets/mitzi_scroller/XX.bmp
    FuriString* path = furi_string_alloc();
    furi_string_printf(path, EXT_PATH("apps_assets/mitzi_scroller/%02d.bmp"), tile_num);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    bool success = false;

    FURI_LOG_I("Scroller", "Attempting to load: %s", furi_string_get_cstr(path));

    if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("Scroller", "Failed to open file: %s", furi_string_get_cstr(path));
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(path);
        return false;
    }

    FURI_LOG_I("Scroller", "File opened successfully");

    // Read BMP header (first 54 bytes for standard BMP)
    uint8_t header[54];
    size_t bytes_read = storage_file_read(file, header, 54);
    FURI_LOG_I("Scroller", "Read %zu bytes of header", bytes_read);

    if(bytes_read == 54) {
        // Verify BMP signature
        if(header[0] == 'B' && header[1] == 'M') {
            FURI_LOG_I("Scroller", "Valid BMP signature");

            // Get image dimensions from header
            int32_t width = header[18] | (header[19] << 8) | (header[20] << 16) |
                            (header[21] << 24);
            int32_t height = header[22] | (header[23] << 8) | (header[24] << 16) |
                             (header[25] << 24);
            uint16_t bpp = header[28] | (header[29] << 8); // bits per pixel
            uint32_t data_offset = header[10] | (header[11] << 8) | (header[12] << 16) |
                                   (header[13] << 24);

            FURI_LOG_I(
                "Scroller",
                "BMP: %ldx%ld, %dbpp, data offset: %lu",
                width,
                height,
                bpp,
                data_offset);

            // We expect 128x64, 1-bit BMP
            if(width == TILE_WIDTH && height == TILE_HEIGHT && bpp == 1) {
                FURI_LOG_I("Scroller", "BMP format correct, loading pixels...");

                // Seek to pixel data
                storage_file_seek(file, data_offset, true);

                // Calculate row size (must be multiple of 4 bytes)
                int row_size = ((width + 31) / 32) * 4;
                FURI_LOG_I("Scroller", "Row size: %d bytes", row_size);

                // Read rows top-to-bottom (0 to height-1) to fix vertical flip
                uint8_t row_buffer[row_size];

                for(int row = 0; row < height; row++) {
                    if(storage_file_read(file, row_buffer, row_size) == (size_t)row_size) {
                        // Draw pixels for this row
                        for(int col = 0; col < width; col++) {
                            int byte_idx = col / 8;
                            int bit_idx = 7 - (col % 8);
                            bool pixel = (row_buffer[byte_idx] >> bit_idx) & 1;
                            if(!pixel) {
                                canvas_draw_dot(canvas, x + col, y + row);
                            }
                        }
                    } else {
                        FURI_LOG_E("Scroller", "Failed to read row %d", row);
                        break;
                    }
                }
                FURI_LOG_I("Scroller", "BMP loaded successfully!");
                success = true;
            } else {
                FURI_LOG_E(
                    "Scroller",
                    "Wrong BMP format: %ldx%ld, %dbpp (expected 128x64, 1bpp)",
                    width,
                    height,
                    bpp);
            }
        } else {
            FURI_LOG_E("Scroller", "Invalid BMP signature: 0x%02X 0x%02X", header[0], header[1]);
        }
    } else {
        FURI_LOG_E("Scroller", "Failed to read header (got %zu bytes)", bytes_read);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);

    return success;
}

/* ============================================================================
 * HELPER FUNCTIONS - FILE LOADING
 * ============================================================================ */
/**
 * @brief Load star annotations from CSV file
 * 
 * Reads assets/annotations.csv to get positions and names of stars.
 * Uses simple line-by-line reading without complex streaming.
 * 
 * @param state     Application state to populate with annotation data
 * @param storage   Flipper storage API handle
 * @return          true if successful, false on error
 */
static bool load_annotations(ScrollerState* state, Storage* storage) {
    File* file = storage_file_alloc(storage);

    state->annotation_count = 0;

    // Try to open annotations.csv
    if(!storage_file_open(
           file,
           EXT_PATH("apps_assets/mitzi_scroller/annotations.csv"),
           FSAM_READ,
           FSOM_OPEN_EXISTING)) {
        FURI_LOG_E("Scroller", "Failed to open annotations.csv");
        storage_file_free(file);
        state->has_error = true;
        snprintf(state->error_message, sizeof(state->error_message), "annotations.csv missing!");
        return false;
    }

    // Read file into buffer
    size_t file_size = storage_file_size(file);
    if(file_size == 0 || file_size > 16384) { // Max 16KB for safety
        FURI_LOG_E("Scroller", "Invalid file size: %zu", file_size);
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    char* buffer = malloc(file_size + 1);
    if(!buffer) {
        FURI_LOG_E("Scroller", "Failed to allocate buffer");
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    size_t bytes_read = storage_file_read(file, buffer, file_size);
    buffer[bytes_read] = '\0'; // Null terminate

    storage_file_close(file);
    storage_file_free(file);

    // Parse buffer line by line
    char* line = buffer;
    bool first_line = true; // Skip header

    while(line && *line && state->annotation_count < MAX_ANNOTATIONS) {
        // Find end of line
        char* next_line = strchr(line, '\n');
        if(next_line) {
            *next_line = '\0'; // Terminate current line
            next_line++; // Move to next line
        }

        // Remove carriage return if present
        char* cr = strchr(line, '\r');
        if(cr) *cr = '\0';

        // Skip header line
        if(first_line) {
            first_line = false;
            line = next_line;
            continue;
        }

        // Parse CSV line: tile_number,x,y,annotation
        int tile_num = 0;
        int x = 0;
        int y = 0;
        char text[MAX_ANNOTATION_LENGTH] = {0};

        if(sscanf(line, "%d,%d,%d,%63[^\r\n]", &tile_num, &x, &y, text) == 4) {
            if(tile_num >= 0 && tile_num < TOTAL_TILES) {
                Annotation* ann = &state->annotations[state->annotation_count];
                ann->tile_number = tile_num;
                ann->x = x;
                ann->y = y;
                strncpy(ann->text, text, sizeof(ann->text) - 1);
                ann->text[sizeof(ann->text) - 1] = '\0';
                state->annotation_count++;
            }
        }

        line = next_line;
    }

    free(buffer);

    FURI_LOG_I("Scroller", "Loaded %d annotations", state->annotation_count);
    return state->annotation_count > 0;
}

/* ============================================================================
 * HELPER FUNCTIONS - ANNOTATION DETECTION
 * ============================================================================ */

/**
 * @brief Check if the cursor is currently over any star annotation
 * 
 * This function performs collision detection between the cursor circle
 * (always at screen center) and star annotation points.
 * 
 * @param state     Application state containing camera position and annotations
 */
static void check_annotations(ScrollerState* state) {
    // Reset annotation state
    state->has_annotation = false;
    state->current_annotation[0] = '\0';

    // Calculate cursor position in world coordinates
    int cursor_world_x = (int)(state->camera_x + SCREEN_WIDTH / 2);
    int cursor_world_y = (int)(state->camera_y + SCREEN_HEIGHT / 2);

    // Determine which tile the cursor is on
    int cursor_tile_col = cursor_world_x / TILE_WIDTH;
    int cursor_tile_row = cursor_world_y / TILE_HEIGHT;

    // Calculate tile number
    int cursor_tile_num = row_col_to_tile_num(cursor_tile_row, cursor_tile_col);

    // Store current tile for preview
    if(cursor_tile_num >= 0 && cursor_tile_num < TOTAL_TILES) {
        state->current_tile = cursor_tile_num;
    } else {
        state->current_tile = -1;
    }

    // Bounds check
    if(cursor_tile_num < 0 || cursor_tile_num >= TOTAL_TILES) return;

    // Calculate cursor position within the tile
    int tile_local_x = cursor_world_x % TILE_WIDTH;
    int tile_local_y = cursor_world_y % TILE_HEIGHT;

    // Check all annotations for the current tile
    for(int i = 0; i < state->annotation_count; i++) {
        Annotation* ann = &state->annotations[i];

        if(ann->tile_number == cursor_tile_num) {
            // Calculate distance between cursor and annotation
            int dx = tile_local_x - ann->x;
            int dy = tile_local_y - ann->y;
            int dist_sq = dx * dx + dy * dy;

            // Check if cursor overlaps annotation
            if(dist_sq <= CURSOR_RADIUS * CURSOR_RADIUS) {
                state->has_annotation = true;
                strncpy(
                    state->current_annotation, ann->text, sizeof(state->current_annotation) - 1);
                state->current_annotation[sizeof(state->current_annotation) - 1] = '\0';
                break;
            }
        }
    }
}

/* ============================================================================
 * GUI CALLBACKS
 * ============================================================================ */

/**
 * @brief Canvas draw callback - renders the star map and UI
 * 
 * @param canvas    Flipper canvas API for drawing
 * @param ctx       Application state (ScrollerState*)
 */
static void scroller_draw_callback(Canvas* canvas, void* ctx) {
    ScrollerState* state = (ScrollerState*)ctx;

    canvas_clear(canvas);

    // Calculate visible tiles
    int start_tile_col = (int)(state->camera_x / TILE_WIDTH);
    int start_tile_row = (int)(state->camera_y / TILE_HEIGHT);
    int end_tile_col = (int)((state->camera_x + SCREEN_WIDTH) / TILE_WIDTH);
    int end_tile_row = (int)((state->camera_y + SCREEN_HEIGHT) / TILE_HEIGHT);

    // Clamp to valid range
    if(start_tile_col < 0) start_tile_col = 0;
    if(start_tile_row < 0) start_tile_row = 0;
    if(end_tile_col >= TILE_COLS) end_tile_col = TILE_COLS - 1;
    if(end_tile_row >= TILE_ROWS) end_tile_row = TILE_ROWS - 1;

    // Draw visible tiles
    canvas_set_color(canvas, ColorBlack);
    for(int row = start_tile_row; row <= end_tile_row; row++) {
        for(int col = start_tile_col; col <= end_tile_col; col++) {
            int tile_num = row_col_to_tile_num(row, col);

            int screen_x = (int)(col * TILE_WIDTH - state->camera_x);
            int screen_y = (int)(row * TILE_HEIGHT - state->camera_y);

            // Try to load and draw the BMP file
            if(!draw_tile_bmp(canvas, tile_num, screen_x, screen_y)) {
                // Fallback: draw tile border and number if BMP not found
                canvas_draw_frame(canvas, screen_x, screen_y, TILE_WIDTH, TILE_HEIGHT);
                canvas_set_font(canvas, FontSecondary);
                char tile_text[16];
                snprintf(tile_text, sizeof(tile_text), "%02d", tile_num);
                canvas_draw_str(canvas, screen_x + 2, screen_y + 8, tile_text);
            }
        }
    }

    // Draw cursor
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_circle(canvas, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, CURSOR_RADIUS);

    // Draw current tile filename in top right corner (if enabled)
    if(state->current_tile >= 0 && state->show_tile_name) {
        char tile_filename[16]; // Increased size to avoid truncation warning
        snprintf(tile_filename, sizeof(tile_filename), "%02d.bmp", state->current_tile);

        canvas_set_font(canvas, FontSecondary);
        int text_width = canvas_string_width(canvas, tile_filename);

        // Draw black background box
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, SCREEN_WIDTH - text_width - 4, 0, text_width + 4, 10);

        // Draw white text
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, SCREEN_WIDTH - text_width - 2, 8, tile_filename);
    }

    // Draw annotation if present
    if(state->has_annotation) {
        canvas_set_font(canvas, FontSecondary);
        int text_width = canvas_string_width(canvas, state->current_annotation);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 0, text_width + 4, 10);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 2, 8, state->current_annotation);
    }

    // Draw error modal if there's an error
    if(state->has_error) {
        draw_simple_modal(canvas, state->error_message);
    }
}

/**
 * @brief Input event callback
 * 
 * @param input_event   Button press/release event
 * @param ctx           Event queue (FuriMessageQueue*)
 */
static void scroller_input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

/* ============================================================================
 * MAIN APPLICATION ENTRY POINT
 * ============================================================================ */
int32_t scroller_main(void* p) {
    UNUSED(p);

    // Allocate state
    ScrollerState* state = malloc(sizeof(ScrollerState));
    memset(state, 0, sizeof(ScrollerState));

    // Initialize camera to center
    state->camera_x = (MAP_WIDTH - SCREEN_WIDTH) / 2.0f;
    state->camera_y = (MAP_HEIGHT - SCREEN_HEIGHT) / 2.0f;
    state->current_tile = -1;
    state->show_tile_name = false;

    // Load annotations
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!load_annotations(state, storage)) {
        FURI_LOG_E("Scroller", "Failed to load annotations");
    }
    furi_record_close(RECORD_STORAGE);

    FURI_LOG_I(
        "Scroller", "Map: %dx%d tiles, %dx%d pixels", TILE_COLS, TILE_ROWS, MAP_WIDTH, MAP_HEIGHT);

    // Setup GUI
    state->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    state->view_port = view_port_alloc();
    view_port_draw_callback_set(state->view_port, scroller_draw_callback, state);
    view_port_input_callback_set(state->view_port, scroller_input_callback, state->event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, state->view_port, GuiLayerFullscreen);

    // Main loop
    InputEvent event;
    bool running = true;
    const float move_speed = 4.0f;

    check_annotations(state);
    view_port_update(state->view_port);

    while(running) {
        if(furi_message_queue_get(state->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                switch(event.key) {
                case InputKeyUp:
                    if(event.type == InputTypePress) {
                        // Short press: smooth scroll
                        state->camera_y -= move_speed;
                        if(state->camera_y < CAMERA_MIN_Y) state->camera_y = CAMERA_MIN_Y;
                    } else {
                        // Long press: jump to next tile center up
                        int current_row =
                            (int)((state->camera_y + SCREEN_HEIGHT / 2) / TILE_HEIGHT);
                        if(current_row > 0) {
                            int target_row = current_row - 1;
                            state->camera_y =
                                target_row * TILE_HEIGHT + TILE_HEIGHT / 2 - SCREEN_HEIGHT / 2;
                            if(state->camera_y < CAMERA_MIN_Y) state->camera_y = CAMERA_MIN_Y;
                        }
                    }
                    break;

                case InputKeyDown:
                    if(event.type == InputTypePress) {
                        // Short press: smooth scroll
                        state->camera_y += move_speed;
                        if(state->camera_y > CAMERA_MAX_Y) {
                            state->camera_y = CAMERA_MAX_Y;
                        }
                    } else {
                        // Long press: jump to next tile center down
                        int current_row =
                            (int)((state->camera_y + SCREEN_HEIGHT / 2) / TILE_HEIGHT);
                        if(current_row < TILE_ROWS - 1) {
                            int target_row = current_row + 1;
                            state->camera_y =
                                target_row * TILE_HEIGHT + TILE_HEIGHT / 2 - SCREEN_HEIGHT / 2;
                            if(state->camera_y > CAMERA_MAX_Y) {
                                state->camera_y = CAMERA_MAX_Y;
                            }
                        }
                    }
                    break;

                case InputKeyLeft:
                    if(event.type == InputTypePress) {
                        // Short press: smooth scroll
                        state->camera_x -= move_speed;
                        if(state->camera_x < CAMERA_MIN_X) state->camera_x = CAMERA_MIN_X;
                    } else {
                        // Long press: jump to next tile center left
                        int current_col = (int)((state->camera_x + SCREEN_WIDTH / 2) / TILE_WIDTH);
                        if(current_col > 0) {
                            int target_col = current_col - 1;
                            state->camera_x =
                                target_col * TILE_WIDTH + TILE_WIDTH / 2 - SCREEN_WIDTH / 2;
                            if(state->camera_x < CAMERA_MIN_X) state->camera_x = CAMERA_MIN_X;
                        }
                    }
                    break;

                case InputKeyRight:
                    if(event.type == InputTypePress) {
                        // Short press: smooth scroll
                        state->camera_x += move_speed;
                        if(state->camera_x > CAMERA_MAX_X) {
                            state->camera_x = CAMERA_MAX_X;
                        }
                    } else {
                        // Long press: jump to next tile center right
                        int current_col = (int)((state->camera_x + SCREEN_WIDTH / 2) / TILE_WIDTH);
                        if(current_col < TILE_COLS - 1) {
                            int target_col = current_col + 1;
                            state->camera_x =
                                target_col * TILE_WIDTH + TILE_WIDTH / 2 - SCREEN_WIDTH / 2;
                            if(state->camera_x > CAMERA_MAX_X) {
                                state->camera_x = CAMERA_MAX_X;
                            }
                        }
                    }
                    break;

                case InputKeyBack:
                    running = false;
                    break;

                case InputKeyOk:
                    // Toggle tile name display
                    state->show_tile_name = !state->show_tile_name;

                    if(state->has_annotation) {
                        FURI_LOG_I("Scroller", "Selected: %s", state->current_annotation);
                    }
                    break;

                default:
                    break;
                }

                check_annotations(state);
                view_port_update(state->view_port);
            }
        }
    }

    // Cleanup
    gui_remove_view_port(gui, state->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(state->view_port);
    furi_message_queue_free(state->event_queue);
    free(state);

    return 0;
}

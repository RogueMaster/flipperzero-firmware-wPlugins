#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <stdint.h> // Standard integer types
#include <stdlib.h> // Standard library functions
#include <gui/elements.h> // to access button drawing functions
#include "tree_ident_icons.h" // Custom icon definitions

#define TAG "Tree Ident" // Tag for logging purposes

// Magic numbers
#define MENU_MAX_VISIBLE_ITEMS 4 // number of menu items visible at before scrolling is needed
#define MENU_OPTION_HEIGHT     10 // Vertical spacing between menu options in pixels
#define MENU_START_Y           21 // Y-coordinate where 1st menu option text starts within the frame
#define MENU_FRAME_X           1 // Left edge of menu frame
#define MENU_FRAME_Y           11 // Top of menu frame
#define MENU_FRAME_WIDTH       127 // Frame width
#define SELECTOR_X             3 // X position for ">" selection indicator
#define OPTION_TEXT_X          7 // X value where option text begins

typedef enum {
    ScreenSplash,
    ScreenL1Question,
    ScreenL1_A1,
    ScreenL1_A2,
    ScreenL1_A3,
    ScreenL2_A1_1, // Real needles
    ScreenL2_A1_2, // Scales
    ScreenL2_A2_1, // Compound leaves
    ScreenL2_A2_2, // Single leaf
    ScreenL3_A1_1_1, // Needles only on long shoots
    ScreenL3_A1_1_2, // Needles on short shoots
    ScreenL3_A1_1_3, // Needles on long and short shoots
    ScreenL3_A2_1_1, // Odd # of leaflets, terminal leaflet
    ScreenL3_A2_1_2, // Even # of leaflets, no terminal leaflet
    ScreenL3_A2_1_3, // Leaflets radiate from single point
    ScreenL3_A2_2_1, // Big rounded or pointed bumps
    ScreenL3_A2_2_2, // Leaf margin is smooth
    ScreenL3_A2_2_3, // Small teeth or cuts
    Screen_Fruit_Citrus,
    Screen_Fruit_Nut,
    Screen_Fruit_Samara,
    Screen_Fruit_Drupe,
    Screen_Fruit_Cone,
    Screen_Fruit_Berry,
    Screen_Fruit_Pome,
    Screen_Fruit_Other
} AppScreen;

static const AppScreen FRUIT_SCREENS[] = {
    Screen_Fruit_Citrus,
    Screen_Fruit_Nut,
    Screen_Fruit_Samara,
    Screen_Fruit_Drupe,
    Screen_Fruit_Cone,
    Screen_Fruit_Berry,
    Screen_Fruit_Pome,
    Screen_Fruit_Other};

// Menu configuration structure containing all state variable for a menu
typedef struct {
    const char** options; // Array of option strings to display
    int total_options; // Total number of options in the menu
    int selected_index; // Currently selected option (0-based index)
    int scroll_offset; // First visible item index (for scrolling menus)
} MenuState;

// Main application structure
typedef struct {
    FuriMessageQueue* input_queue; // Queue for handling input events
    ViewPort* view_port; // ViewPort for rendering UI
    Gui* gui; // GUI instance

    uint8_t current_screen; // 0 = first screen, 1 = second screen

    MenuState level1_menu;
    MenuState level2_needle_menu; // For ScreenL1_A1
    MenuState level2_leaf_menu; // For ScreenL1_A2
    MenuState level3_needle_location; // For ScreenL2_A1_1
    MenuState level3_leaflet_arrange; // For ScreenL2_A2_1
    MenuState level3_leaf_margin; // For ScreenL2_A2_2
    MenuState fruit_menu;
} AppState;

// STATIC MENU OPTION ARRAYS ==================================================
static const char* level1_options[] = {
    ".. needles.",
    ".. leaves.",
    "I don't know. Ask differently."};

static const char* needle_type_options[] = {
    "long and pointed needles",
    "are small and flat scales"};

static const char* leaf_type_options[] = {
    "compound leaves w. leaflets",
    "single, undivided leaves"};

static const char* needle_location_options[] = {
    "only on long shoots",
    "on short shoots",
    "both on long& short shoots"};

static const char* leaflet_arrangement_options[] = {
    "Odd #, single terminal leaflet",
    "Even #, no terminal leaflet",
    "Leaflets radiate from point"};

static const char* leaf_margin_options[] = {
    "Big rounded o.pointed bumps",
    "Leaf margin is smooth",
    "Small teeth or cuts"};

static const char* fruit_options[] = {
    "have segmented flesh & rind",
    "are hard-shelled, dry (nut)",
    "are winged (samara)",
    "are fleshy w. hard stone",
    "are woody cones",
    "are soft berries",
    "are fleshy with seeds inside",
    "are different or not present"};

// =============================================================================
// MENU HELPER FUNCTIONS
// =============================================================================
int get_menu_frame_height(int total_options) {
    int visible_items = (total_options < MENU_MAX_VISIBLE_ITEMS) ? total_options :
                                                                   MENU_MAX_VISIBLE_ITEMS;
    return 3 + (visible_items * MENU_OPTION_HEIGHT);
}

void handle_scrollable_menu_input(
    InputKey key,
    int* selected,
    int* scroll_offset,
    int total_items,
    int visible_items) {
    if(key == InputKeyUp && *selected > 0) {
        (*selected)--;
        if(*selected < *scroll_offset) {
            *scroll_offset = *selected;
        }
    } else if(
        key == InputKeyDown &&
        *selected < total_items - 1) { // Adjust scroll if selection goes below visible area
        (*selected)++;
        if(*selected >= *scroll_offset + visible_items) {
            *scroll_offset = *selected - visible_items + 1;
        }
    }
}

void handle_menu_input(MenuState* menu, InputKey key, InputType type) {
    if(type != InputTypePress) return;

    if(menu->total_options <= MENU_MAX_VISIBLE_ITEMS) {
        // No scrolling - wraparound navigation
        if(key == InputKeyUp) {
            menu->selected_index =
                (menu->selected_index - 1 + menu->total_options) % menu->total_options;
        } else if(key == InputKeyDown) {
            menu->selected_index = (menu->selected_index + 1) % menu->total_options;
        }
    } else {
        // Scrolling navigation
        handle_scrollable_menu_input(
            key,
            &menu->selected_index,
            &menu->scroll_offset,
            menu->total_options,
            MENU_MAX_VISIBLE_ITEMS);
    }
}

void setup_menu(MenuState* menu, const char** options, int count) {
    menu->options = options;
    menu->total_options = count;
    menu->selected_index = 0;
    menu->scroll_offset = 0;
}

// =============================================================================
// DRAWING FUNCTIONS
// =============================================================================
void draw_menu(Canvas* canvas, MenuState* menu) {
    // Calculate height of menu frame and draw it
    int frame_height = get_menu_frame_height(menu->total_options);
    canvas_draw_frame(canvas, MENU_FRAME_X, MENU_FRAME_Y, MENU_FRAME_WIDTH, frame_height);
    // If there are more MENU_MAX_VISIBLE_ITEMS, scrolling is needed
    int visible_items = (menu->total_options < MENU_MAX_VISIBLE_ITEMS) ? menu->total_options :
                                                                         MENU_MAX_VISIBLE_ITEMS;
    int display_start = menu->scroll_offset;
    int display_end = display_start + visible_items;
    // Write visible options onto canvas
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < visible_items && (display_start + i) < menu->total_options; i++) {
        int option_index = display_start + i;
        int y_pos = MENU_START_Y + (i * MENU_OPTION_HEIGHT);
        canvas_draw_str(canvas, OPTION_TEXT_X, y_pos, menu->options[option_index]);
    }
    // Draw selection indicator which is a greater-symbol
    if(menu->selected_index >= display_start && menu->selected_index < display_end) {
        int indicator_line = menu->selected_index - display_start;
        int y_pos = MENU_START_Y + (indicator_line * MENU_OPTION_HEIGHT);
        canvas_draw_str(canvas, SELECTOR_X, y_pos, ">");
    }
    // Draw navigation hints
    int nav_icon_y = MENU_FRAME_Y + frame_height - 11;
    canvas_draw_icon(canvas, 119, nav_icon_y, &I_ButtonUp_7x4);
    canvas_draw_icon(canvas, 119, nav_icon_y + 5, &I_ButtonDown_7x4);
}

void draw_result_screen(Canvas* canvas, const Icon* icon, const char* title, const char* content) {
    canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
    if(icon != NULL) {
        canvas_draw_icon(canvas, 118, 1, icon);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, title);
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(canvas, 1, 11, AlignLeft, AlignTop, content);
    elements_button_left(canvas, "Prev.?");
    elements_button_center(canvas, "OK");
}

// =============================================================================
// MAIN CALLBACK - called whenever the screen needs to be redrawn
// =============================================================================
void draw_callback(Canvas* canvas, void* context) {
    AppState* state = context; // Get app context to check current screen

    // Clear the canvas and set drawing color to black
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    switch(state->current_screen) {
    case ScreenSplash: // First screen ----------------------------
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 1, 5, AlignLeft, AlignTop, "Identify a tree");

        canvas_draw_icon(canvas, 75, 1, &I_icon_52x52); // 51 is a pixel above the buttons

        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 1, 16, AlignLeft, AlignTop, "Just answer a few");
        canvas_draw_str_aligned(canvas, 1, 25, AlignLeft, AlignTop, "questions.");

        canvas_set_font_direction(
            canvas, CanvasDirectionBottomToTop); // Set text rotation to 90 degrees
        canvas_draw_str(canvas, 128, 45, "2025-11");
        canvas_set_font_direction(
            canvas, CanvasDirectionLeftToRight); // Reset to normal text direction

        canvas_draw_str_aligned(canvas, 1, 49, AlignLeft, AlignTop, "Hold 'back'");
        canvas_draw_str_aligned(canvas, 1, 57, AlignLeft, AlignTop, "to exit.");

        canvas_draw_str_aligned(canvas, 100, 54, AlignLeft, AlignTop, "F. Greil");
        canvas_draw_str_aligned(canvas, 110, 1, AlignLeft, AlignTop, "v0.4");

        // Draw button hints at bottom using elements library
        elements_button_center(canvas, "OK"); // for the OK button
        break;
    case ScreenL1Question: // Second screen -------------------------------------------------
        canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
        // Level 1 Question
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "My tree has...");
        draw_menu(canvas, &state->level1_menu); // custom function, see definition above
        canvas_draw_str_aligned(canvas, 1, 49, AlignLeft, AlignTop, "Press 'back'");
        canvas_draw_str_aligned(canvas, 1, 56, AlignLeft, AlignTop, "for splash.");
        elements_button_center(canvas, "OK"); // for the OK button
        break;
    case ScreenL1_A1: // Level 1: Option 1: Needle Tree
        canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); // icon with App logo
        canvas_draw_icon(
            canvas, 118, 1, &I_NeedleTree_10x10); // icon with choosen option for level 1
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Needle-leaves are..");
        draw_menu(canvas, &state->level2_needle_menu);
        elements_button_left(canvas, "Prev. ?");
        elements_button_center(canvas, "OK"); // for the OK button
        break;
    case ScreenL1_A2: // Level 2: Option 2: Leave Tree
        canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); // icon with App logo
        canvas_draw_icon(
            canvas, 118, 1, &I_LeaveTree_10x10); // icon with choosen option for level 1
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Broad leaves are..");
        draw_menu(canvas, &state->level2_leaf_menu);
        elements_button_left(canvas, "Prev. ?");
        elements_button_center(canvas, "OK"); // for the OK button
        break;
    case ScreenL1_A3: // Level 2: Option 3: No leave type chosen. Fruit identification with scrolling
        canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); // icon with App logo
        // canvas_draw_icon(canvas, 118, 1, &I_IconNotReady_10x10);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "My trees fruit..");
        draw_menu(canvas, &state->fruit_menu);
        elements_button_left(canvas, "Prev. ?");
        elements_button_center(canvas, "OK"); // for the OK button
        break;
    case ScreenL2_A1_1: // Level 2: Option 1_1: Needles
        canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
        canvas_draw_icon(
            canvas, 118, 1, &I_NeedleTree_10x10); // icon with choosen option for level 1
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Where are needles?");
        draw_menu(canvas, &state->level3_needle_location);
        elements_button_left(canvas, "Prev.?");
        elements_button_center(canvas, "OK");
        break;
    case ScreenL2_A1_2: // Level 2: Option 1_2: Scales
        canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
        canvas_draw_icon(
            canvas, 118, 1, &I_NeedleTree_10x10); // icon with choosen option for level 1
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Scales");
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text_aligned(
            canvas,
            1,
            11,
            AlignLeft,
            AlignTop,
            "Cypress, Ccedar, Giant sequoia, Redwood, Juniper (most species), Arbor vitae, ..");
        elements_button_left(canvas, "Prev.?");
        break;
    case ScreenL2_A2_1: // Level 2: Option 2_1: Compound leave
        canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
        // canvas_draw_icon(canvas, 118, 1, &I_LeaveTree_10x10); // icon with choosen option for level 1
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Leaflets' arrangement?");
        draw_menu(canvas, &state->level3_needle_location);
        elements_button_left(canvas, "Prev.?");
        elements_button_center(canvas, "OK");
        break;
    case ScreenL2_A2_2: // Level 2: Option 2_2: Undivided leaf
        canvas_draw_icon(canvas, 1, 1, &I_icon_10x10);
        canvas_draw_icon(
            canvas, 118, 1, &I_LeaveTree_10x10); // icon with choosen option for level 1
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 12, 1, AlignLeft, AlignTop, "Type of leaf margin?");
        draw_menu(canvas, &state->level3_leaf_margin);
        elements_button_left(canvas, "Prev.?");
        elements_button_center(canvas, "OK");
        break;
    case ScreenL3_A1_1_1: // Level 3: Needles only on long shoots
        draw_result_screen(
            canvas, &I_NeedleTree_10x10, "On long shoots", "Fir, Spruce, Hemlock, Yew.");
        break;
    case ScreenL3_A1_1_2: // Level 3: Needles on short shoots
        draw_result_screen(canvas, &I_NeedleTree_10x10, "On short shoots", "Larch, Cedar.");
        break;
    case ScreenL3_A1_1_3: // Level 3: Needles on long and short shoots
        draw_result_screen(canvas, &I_NeedleTree_10x10, "On all shoots", "Pines.");
        break;
    case ScreenL3_A2_1_1: // Level 3: Odd-pinnate leaves / unpaarig gefiedert
        draw_result_screen(
            canvas,
            &I_Imparipinnate_10x10,
            "Odd-pinnate leaves",
            "European ash, Rowan, Common walnut, Elder, False acacia, Black walnut, Hickory, Sumac, Yellowwood, ..");
        break;
        break;
    case ScreenL3_A2_1_2: // Level 3: Even-pinnate leaves / paarig gefiedert
        draw_result_screen(
            canvas,
            &I_Paripinnate_10x10,
            "Even-pinnate leaves",
            "Locust, Carob tree, Laburnum, Mimosa, Judas tree, ..");
        break;
    case ScreenL3_A2_1_3: // Level 3: Palmate leaves / gefingert
        draw_result_screen(
            canvas,
            &I_Palmate_10x10,
            "Palmate leaves",
            "Horse chestnut, Maple, Sycamore, Vine, Fig, ..");
        break;
    case ScreenL3_A2_2_1: // Level 3: Lobed leaves / Gelappte Laubblätter
        draw_result_screen(
            canvas,
            &I_Lobed_10x10,
            "Lobed leaf",
            "Oak, Hawthorn, White polar, London plane, Mulberry, Ginkgo, Sassafras, ..");
        break;
    case ScreenL3_A2_2_2: // Level 3: Entire leaves / Ganzrandige Laubblätter
        draw_result_screen(
            canvas,
            &I_Entire_10x10,
            "Entire leaf",
            "European beech, Common box, Laurel, Holly, Magnolia, Olive, Pear, Dogwood, ..)");
        break;
    case ScreenL3_A2_2_3: // Level 3: Serrated or notched leaves
        draw_result_screen(
            canvas,
            &I_Notched_10x10,
            "Notched leaf",
            "Silver birch, European hornbeam, Com.hazel, Elm, Sweet chestnut, Alder, Aspen, Apple, Cherry, Plum, ..");
        break;
    // Fruit identification results
    case Screen_Fruit_Citrus:
        draw_result_screen(
            canvas,
            &I_Citrus_10x10,
            "Citrus",
            "Leathery rind and segmented flesh: oranges, lemons, ..");
        break;
    case Screen_Fruit_Nut:
        draw_result_screen(
            canvas,
            &I_Nut_10x10,
            "Nut",
            "A hard-shelled, dry fruit with a single seed that doesn't split open: acorns, hazelnuts, ..");
        break;
    case Screen_Fruit_Samara:
        draw_result_screen(
            canvas,
            &I_Samara_10x10,
            "Samara",
            "A winged, dry fruit that doesn't split open: maple, elm seeds ..");
        break;
    case Screen_Fruit_Drupe:
        draw_result_screen(
            canvas,
            &I_Drupe_10x10,
            "Drupe",
            "Fleshy fruit with a hard stone enclosing a single seed (e.g., peaches, cherries, olives)");
        break;
    case Screen_Fruit_Cone:
        draw_result_screen(
            canvas,
            &I_Cone_10x10,
            "Cone",
            "Seed-bearing structure with overlapping scales: pine/ spruce cones.");
        break;
    case Screen_Fruit_Berry:
        draw_result_screen(
            canvas,
            &I_Berry_10x10,
            "Berry",
            "Fleshy fruit from single ovary w. seeds embedded in the flesh: tomatoes, blueberries, ..");
        break;
    case Screen_Fruit_Pome:
        draw_result_screen(
            canvas,
            &I_Pome_10x10,
            "Pome",
            "Fleshy fruit where outer part comes from floral tube, not the ovary: apples, pears, ..");
        break;
    case Screen_Fruit_Other:
        draw_result_screen(
            canvas,
            &I_IDK_10x10,
            "Other fruit",
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
        break;
    }
}

void input_callback(InputEvent* event, void* context) {
    AppState* app = context;
    // Put the input event into the message queue for processing
    // Timeout of 0 means non-blocking
    furi_message_queue_put(app->input_queue, event, 0);
}

// =============================================================================
// MAIN APPLICATION
// =============================================================================
int32_t tree_ident_main(void* p) {
    UNUSED(p);

    AppState app; // Application state struct
    app.current_screen = ScreenSplash; // Start on first screen (0)

    // Initialize Menu States
    setup_menu(&app.level1_menu, level1_options, 3);
    setup_menu(&app.level2_needle_menu, needle_type_options, 2);
    setup_menu(&app.level2_leaf_menu, leaf_type_options, 2);
    setup_menu(&app.level3_needle_location, needle_location_options, 3);
    setup_menu(&app.level3_leaflet_arrange, leaflet_arrangement_options, 3);
    setup_menu(&app.level3_leaf_margin, leaf_margin_options, 3);
    setup_menu(&app.fruit_menu, fruit_options, 8);

    // Allocate resources for rendering and
    app.view_port = view_port_alloc(); // for rendering
    app.input_queue = furi_message_queue_alloc(1, sizeof(InputEvent)); // Only one element in queue

    // Callbacks
    view_port_draw_callback_set(app.view_port, draw_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, &app);

    // Initialize GUI
    app.gui = furi_record_open("gui");
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);

    // Input handling
    InputEvent input;
    uint8_t exit_loop = 0; // Flag to exit main loop

    FURI_LOG_I(TAG, "Start the main loop.");
    while(1) {
        furi_check(
            furi_message_queue_get(app.input_queue, &input, FuriWaitForever) == FuriStatusOk);
        // Handle button presses based on current screen
        switch(input.key) {
        case InputKeyUp:
            if(input.type == InputTypePress) {
                switch(app.current_screen) {
                case ScreenL1Question:
                    handle_menu_input(&app.level1_menu, InputKeyUp, input.type);
                    break;
                case ScreenL1_A1:
                    handle_menu_input(&app.level2_needle_menu, InputKeyUp, input.type);
                    break;
                case ScreenL1_A2:
                    handle_menu_input(&app.level2_leaf_menu, InputKeyUp, input.type);
                    break;
                case ScreenL1_A3:
                    handle_menu_input(&app.fruit_menu, InputKeyUp, input.type);
                    break;
                case ScreenL2_A1_1:
                    handle_menu_input(&app.level3_needle_location, InputKeyUp, input.type);
                    break;
                case ScreenL2_A2_1:
                    handle_menu_input(&app.level3_leaflet_arrange, InputKeyUp, input.type);
                    break;
                case ScreenL2_A2_2:
                    handle_menu_input(&app.level3_leaf_margin, InputKeyUp, input.type);
                    break;
                }
            }
            break;
        case InputKeyDown:
            if(input.type == InputTypePress) {
                switch(app.current_screen) {
                case ScreenL1Question:
                    handle_menu_input(&app.level1_menu, InputKeyDown, input.type);
                    break;
                case ScreenL1_A1:
                    handle_menu_input(&app.level2_needle_menu, InputKeyDown, input.type);
                    break;
                case ScreenL1_A2:
                    handle_menu_input(&app.level2_leaf_menu, InputKeyDown, input.type);
                    break;
                case ScreenL1_A3:
                    handle_menu_input(&app.fruit_menu, InputKeyDown, input.type);
                    break;
                case ScreenL2_A1_1:
                    handle_menu_input(&app.level3_needle_location, InputKeyDown, input.type);
                    break;
                case ScreenL2_A2_1:
                    handle_menu_input(&app.level3_leaflet_arrange, InputKeyDown, input.type);
                    break;
                case ScreenL2_A2_2:
                    handle_menu_input(&app.level3_leaf_margin, InputKeyDown, input.type);
                    break;
                }
            }
            break;
        case InputKeyOk:
            if(input.type == InputTypePress) {
                switch(app.current_screen) {
                case ScreenSplash:
                    app.current_screen = ScreenL1Question; // Move to second screen
                    break;
                case ScreenL1Question:
                    switch(app.level1_menu.selected_index) {
                    case 0:
                        app.current_screen = ScreenL1_A1;
                        break;
                    case 1:
                        app.current_screen = ScreenL1_A2;
                        break;
                    case 2:
                        app.current_screen = ScreenL1_A3;
                        break;
                    }
                    break;
                case ScreenL1_A1: // Needles
                    switch(app.level2_needle_menu.selected_index) {
                    case 0:
                        app.current_screen = ScreenL2_A1_1;
                        break;
                    case 1:
                        app.current_screen = ScreenL2_A1_2;
                        break;
                    }
                    break;
                case ScreenL1_A2: // Leaves
                    switch(app.level2_leaf_menu.selected_index) {
                    case 0:
                        app.current_screen = ScreenL2_A2_1;
                        break;
                    case 1:
                        app.current_screen = ScreenL2_A2_2;
                        break;
                    }
                    break;
                case ScreenL1_A3: // Fruit type selection
                    app.current_screen = FRUIT_SCREENS[app.fruit_menu.selected_index];
                    break;
                    break;
                case ScreenL2_A1_1: // Where are the needles?
                    switch(app.level3_needle_location.selected_index) {
                    case 0:
                        app.current_screen = ScreenL3_A1_1_1;
                        break;
                    case 1:
                        app.current_screen = ScreenL3_A1_1_2;
                        break;
                    case 2:
                        app.current_screen = ScreenL3_A1_1_3;
                        break;
                    }
                    break;
                case ScreenL2_A2_1: // How are leaflets arranged?
                    switch(app.level3_leaflet_arrange.selected_index) {
                    case 0:
                        app.current_screen = ScreenL3_A2_1_1;
                        break;
                    case 1:
                        app.current_screen = ScreenL3_A2_1_2;
                        break;
                    case 2:
                        app.current_screen = ScreenL3_A2_1_3;
                        break;
                    }
                    break;
                case ScreenL2_A2_2: // Type of leaf margin?
                    switch(app.level3_leaflet_arrange.selected_index) {
                    case 0:
                        app.current_screen = ScreenL3_A2_2_1;
                        break;
                    case 1:
                        app.current_screen = ScreenL3_A2_2_2;
                        break;
                    case 2:
                        app.current_screen = ScreenL3_A2_2_3;
                        break;
                    }
                    break;
                case Screen_Fruit_Citrus:
                case Screen_Fruit_Nut:
                case Screen_Fruit_Samara:
                case Screen_Fruit_Drupe:
                case Screen_Fruit_Cone:
                case Screen_Fruit_Berry:
                case Screen_Fruit_Pome:
                case Screen_Fruit_Other:
                    if(input.type == InputTypePress) {
                        app.current_screen = ScreenL1_A3;
                    }
                    break;
                default:
                    break;
                }
            }
            break;
        case InputKeyBack:
            switch(app.current_screen) {
            case ScreenSplash:
                if(input.type == InputTypeLong) {
                    exit_loop = 1; // Exit app after long press
                }
                break;
            case ScreenL2_A1_1:
            case ScreenL2_A2_1:
            case ScreenL2_A2_2:
            case ScreenL3_A1_1_1:
            case ScreenL3_A1_1_2:
            case ScreenL3_A1_1_3:
            case ScreenL3_A2_1_1:
            case ScreenL3_A2_1_2:
            case ScreenL3_A2_1_3:
            case ScreenL3_A2_2_2:
            case ScreenL3_A2_2_1:
            case ScreenL3_A2_2_3:
                if(input.type == InputTypePress) {
                    app.current_screen = ScreenL1Question;
                }
                break;
            case ScreenL1Question: // Back button: return to first screen
                app.current_screen = ScreenSplash;
                break;
            }
            break;
        case InputKeyLeft:
            if(input.type == InputTypePress) {
                switch(app.current_screen) {
                case ScreenL1_A1:
                case ScreenL1_A2:
                case ScreenL1_A3:
                    app.current_screen = ScreenL1Question;
                    break;
                case ScreenL2_A1_1:
                case ScreenL2_A1_2:
                    app.current_screen = ScreenL1_A1;
                    break;
                case ScreenL2_A2_1:
                case ScreenL2_A2_2:
                    app.current_screen = ScreenL1_A2;
                    break;
                case ScreenL3_A1_1_1:
                case ScreenL3_A1_1_2:
                case ScreenL3_A1_1_3:
                    app.current_screen = ScreenL2_A1_1;
                    break;
                case ScreenL3_A2_1_1:
                case ScreenL3_A2_1_2:
                case ScreenL3_A2_1_3:
                    app.current_screen = ScreenL2_A2_1;
                    break;
                case ScreenL3_A2_2_1:
                case ScreenL3_A2_2_2:
                case ScreenL3_A2_2_3:
                    app.current_screen = ScreenL2_A2_2;
                    break;
                    break;
                case Screen_Fruit_Citrus:
                case Screen_Fruit_Nut:
                case Screen_Fruit_Samara:
                case Screen_Fruit_Drupe:
                case Screen_Fruit_Cone:
                case Screen_Fruit_Berry:
                case Screen_Fruit_Pome:
                case Screen_Fruit_Other:
                    if(input.type == InputTypePress) {
                        app.current_screen = ScreenL1_A3;
                    }
                    break;
                default:
                    break;
                }
                break;
            }
            break;
        case InputKeyRight:
            switch(app.current_screen) {
            case ScreenL1Question:
                switch(app.level1_menu.selected_index) {
                case 0:
                    app.current_screen = ScreenL1_A1;
                    break;
                case 1:
                    app.current_screen = ScreenL1_A2;
                    break;
                case 2:
                    app.current_screen = ScreenL1_A3;
                    break;
                }
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }

        // Exit main app loop if exit flag is set
        if(exit_loop) break;
        // Trigger screen redraw
        view_port_update(app.view_port);
    }

    // Cleanup: Free all allocated resources
    view_port_enabled_set(app.view_port, false);
    gui_remove_view_port(app.gui, app.view_port);
    furi_record_close("gui");
    view_port_free(app.view_port);

    return 0;
}

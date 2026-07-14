#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include "combo_cracker_icons.h"

#define TAG "ComboLockCracker"

#define BACKLIGHT_ON 1

#define MAX_VALUES 10

// thank you derek jamison! ;)
typedef enum {
    ComboViewSubmenu,
    ComboViewCracker,
    ComboViewResults,
    ComboViewTutorial,
    ComboViewAbout,
} ComboView;

typedef enum {
    ComboEventIdRedrawScreen = 0,
    ComboEventIdCalculateCombo = 1,
} ComboEventId;

typedef enum {
    ComboSubmenuIndexCracker,
    ComboSubmenuIndexTutorial,
    ComboSubmenuIndexAbout,
} ComboSubmenuIndex;

typedef enum _ComboLockType {
    ComboLockTypeNumeric = 0, // zero-init == numeric as default
    ComboLockTypeAlphabetic
} ComboLockType;
#define COMBO_LOCK_TYPE_COUNT (2) // e.g., for modulo operations to iterate...

// store as array of fixed-length strings, so don't need to store 120x pointer values in RAM.
#define LOCK_INDEX_COUNT       (40u)
#define RESISTANCE_INDEX_COUNT (80u)
static const char gc_resistance_labels_numeric[RESISTANCE_INDEX_COUNT][5u] = {
    "0.0",  "0.5",  "1.0",  "1.5",  "2.0",  "2.5",  "3.0",  "3.5",  "4.0",  "4.5",  "5.0",  "5.5",
    "6.0",  "6.5",  "7.0",  "7.5",  "8.0",  "8.5",  "9.0",  "9.5",  "10.0", "10.5", "11.0", "11.5",
    "12.0", "12.5", "13.0", "13.5", "14.0", "14.5", "15.0", "15.5", "16.0", "16.5", "17.0", "17.5",
    "18.0", "18.5", "19.0", "19.5", "20.0", "20.5", "21.0", "21.5", "22.0", "22.5", "23.0", "23.5",
    "24.0", "24.5", "25.0", "25.5", "26.0", "26.5", "27.0", "27.5", "28.0", "28.5", "29.0", "29.5",
    "30.0", "30.5", "31.0", "31.5", "32.0", "32.5", "33.0", "33.5", "34.0", "34.5", "35.0", "35.5",
    "36.0", "36.5", "37.0", "37.5", "38.0", "38.5", "39.0", "39.5",
};
static const char gc_lock_labels_numeric[LOCK_INDEX_COUNT][3u] = {
    "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",  "10", "11", "12", "13",
    "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27",
    "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
};
static const char gc_resistance_labels_alpha[RESISTANCE_INDEX_COUNT][5u] = {
    // NOTE: For whatever reason, I chose for `Y` to be equivalent to `0`
    "Y.00", "Y.25", "Y.50", "Y.75", "A.00", "A.25", "A.50", "A.75", "B.00", "B.25", "B.50", "B.75",
    "C.00", "C.25", "C.50", "C.75", "D.00", "D.25", "D.50", "D.75", "E.00", "E.25", "E.50", "E.75",
    "F.00", "F.25", "F.50", "F.75", "G.00", "G.25", "G.50", "G.75", "H.00", "H.25", "H.50", "H.75",
    "I.00", "I.25", "I.50", "I.75", "L.00", "L.25", "L.50", "L.75", "M.00", "M.25", "M.50", "M.75",
    "N.00", "N.25", "N.50", "N.75", "O.00", "O.25", "O.50", "O.75", "P.00", "P.25", "P.50", "P.75",
    "R.00", "R.25", "R.50", "R.75", "S.00", "S.25", "S.50", "S.75", "T.00", "T.25", "T.50", "T.75",
    "U.00", "U.25", "U.50", "U.75", "W.00", "W.25", "W.50", "W.75",
};
static const char gc_lock_labels_alpha[LOCK_INDEX_COUNT][4u] = {
    // NOTE: For whatever reason, I chose for `Y` to be equivalent to `0`
    "Y", "Y.5", "A", "A.5", "B", "B.5", "C", "C.5", "D", "D.5", "E", "E.5", "F", "F.5",
    "G", "G.5", "H", "H.5", "I", "I.5", "L", "L.5", "M", "M.5", "N", "N.5", "O", "O.5",
    "P", "P.5", "R", "R.5", "S", "S.5", "T", "T.5", "U", "U.5", "W", "W.5",
};

#if 1 // pragma region // static const char gc_howto_numeric[]
static const char gc_instructions_numeric[] =
    "How to use:\n"
    "---\n"
    "First lock value:\n"
    "Set the lock's position to 0, then pull up firmly on the shackle and "
    "slowly rotate the dial counter-clockwise until it catches between two "
    "numbers. If the two locations where the lock gets caught between are "
    "whole numbers, reduce the tension on the shackle slightly so you can "
    "move out of the groove and find the next one. Once you find a groove "
    "between two half numbers, enter the number in the middle of that groove "
    "as the first lock position.\n\n"
    "Second lock value:\n"
    "Use the same process to find the next groove(s) after the first "
    "position. Remember that the locations where the lock gets caught "
    "between should be half numbers, so the middle location will be a "
    "whole number.\n\n"
    "Resistance value:\n"
    "Now apply about half as much tension on the shackle and rotate the dial "
    "CLOCKWISE until you feel resistance. You can repeat this step several "
    "times to make sure you have the correct position. Then enter this value "
    "as the resistance position. This can be a whole or half number.\n\n"
    "Discard one of the third options\n"
    "The solution provides two options for the third digit / letter.  Choose "
    "one, move dial to that location, pull hard on the shackle, and see how "
    "much give the dial has. Do the same for the other option.  The option "
    "with the greater give is the third digit / letter.\n\n"
    "Which of the second options\n"
    "The solution shows ten options for the second digit / letter.  Two of "
    "those options are actually invalid due to physical constraints in the "
    "lock.  This app does not exclude those two options for you (yet).\n\n"
    "This is a brief summary of the technique developed by Samy Kamkar. "
    "For full details and his original write-up, see:\n"
    "https://samy.pl/master/master.html\n";
#endif // 1 pragma endregion // static const char* gc_howto_numeric[]

typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    View* view_cracker;
    Widget* widget_results;
    Widget* widget_tutorial;
    Widget* widget_about;

    FuriTimer* timer;
} ComboLockCrackerApp;

typedef struct {
    ComboLockType lock_type;
    uint8_t first_lock_index; //  Index into gc_lock_string_*
    uint8_t second_lock_index; // Index into gc_lock_string_*
    uint8_t resistance_index; //  Index into gc_resistance_string_*

    int selected; // tracks which UI element is currently selected?
    char result[256]; // this is string buffer for UI display of the result ... not the result itself.
} ComboLockCrackerModel;

typedef struct {
    uint8_t second_pin_count; // variable number of pins for second digit [1..MAX_VALUES]
    uint8_t third_pin_count; // variable number of pins for third digit   [1..MAX_VALUES]
    uint8_t first_pin_index; // first pin is directly calculated (no guesswork)
    uint8_t second_pin_index[MAX_VALUES]; // up to MAX_VALUES entries
    uint8_t third_pin_index[MAX_VALUES]; //  up to MAX_VALUES entries
} ComboLockCombination;

/**
 * @brief      helper to get string for the resistance value
 * @param      model   the ComboLockCrackerModel to obtain resistance value for
 * @return     string representation of the float
 */

static const char* lock1_label_from_model(const ComboLockCrackerModel* model) {
    switch(model->lock_type) {
    case ComboLockTypeNumeric:
        return gc_lock_labels_numeric[model->first_lock_index];
    case ComboLockTypeAlphabetic:
        return gc_lock_labels_alpha[model->first_lock_index];
    default:
        return "?L1?";
    }
}
static const char* lock2_label_from_model(const ComboLockCrackerModel* model) {
    switch(model->lock_type) {
    case ComboLockTypeNumeric:
        return gc_lock_labels_numeric[model->second_lock_index];
    case ComboLockTypeAlphabetic:
        return gc_lock_labels_alpha[model->second_lock_index];
    default:
        return "?L2?";
    }
}
static const char* resistance_label_from_model(const ComboLockCrackerModel* model) {
    switch(model->lock_type) {
    case ComboLockTypeNumeric:
        return gc_resistance_labels_numeric[model->resistance_index];
    case ComboLockTypeAlphabetic:
        return gc_resistance_labels_alpha[model->resistance_index];
    default:
        return "?RR?";
    }
}
static const char* label_from_solution_index(const ComboLockCrackerModel* model, int index) {
    switch(model->lock_type) {
    case ComboLockTypeNumeric:
        return gc_lock_labels_numeric[index];
    case ComboLockTypeAlphabetic:
        return gc_lock_labels_alpha[index];
    default:
        return "?SS?";
    }
}
static const char* lock_type_label(const ComboLockCrackerModel* model) {
    switch(model->lock_type) {
    case ComboLockTypeAlphabetic:
        return "Alpha";
    case ComboLockTypeNumeric:
        return "Numeric";
    }
    return "????";
}

static int SolutionComparator(const ComboLockCombination* r1, const ComboLockCombination* r2) {
    if((r1 == NULL) && (r2 == NULL)) {
        // LOG A WARNING ... as this was likely unintentional
        return 0;
    }
    if(r1 == NULL) {
        return -1;
    }
    if(r2 == NULL) {
        return 1;
    }

    if(r1->second_pin_count != r2->second_pin_count) {
        return r1->second_pin_count - r2->second_pin_count;
    }
    if(r1->third_pin_count != r2->third_pin_count) {
        return r1->third_pin_count - r2->third_pin_count;
    }
    if(r1->first_pin_index != r2->first_pin_index) {
        return r1->first_pin_index - r2->first_pin_index;
    }
    for(unsigned int i = 0; i < r1->second_pin_count; ++i) {
        if(r1->second_pin_index[i] != r2->second_pin_index[i]) {
            return r1->second_pin_index[i] - r2->second_pin_index[i];
        }
    }
    for(unsigned int i = 0; i < r1->third_pin_count; ++i) {
        if(r1->third_pin_index[i] != r2->third_pin_index[i]) {
            return r1->third_pin_index[i] - r2->third_pin_index[i];
        }
    }
    return 0;
}
static void dump_state_and_combinations_to_model_result(
    ComboLockCrackerModel* model,
    const ComboLockCombination* r1,
    const ComboLockCombination* r2) {
    memset(model->result, 0, sizeof(model->result));
    int remaining_bytes = sizeof(model->result) - 1;
    char* b = model->result;
    int written;

    written = snprintf(b, remaining_bytes, "No Solution?\n");
    if((written < 0) || (written >= remaining_bytes)) {
        // can't add anything more, so return.
        return;
    }
    remaining_bytes -= written;
    b += written;

    // write indices
    written = snprintf(
        b,
        remaining_bytes,
        "M: %d, %d, %d",
        model->first_lock_index,
        model->second_lock_index,
        model->resistance_index);
    if((written < 0) || (written >= remaining_bytes)) {
        // can't add anything more, so return.
        return;
    }
    remaining_bytes -= written;
    b += written;

    // write result1 (if non-null)
    if(r1 != NULL) {
        const ComboLockCombination* tmp = r1;
        const char rnum = '1';

        written = snprintf(
            b,
            remaining_bytes,
            "\nR%c: %d  %d(",
            rnum,
            tmp->first_pin_index,
            tmp->second_pin_count);
        if((written < 0) || (written >= remaining_bytes)) {
            // can't add anything more, so return.
            return;
        }
        remaining_bytes -= written;
        b += written;

        // second pin indices
        for(uint8_t i = 0; i < tmp->second_pin_count; ++i) {
            written =
                snprintf(b, remaining_bytes, (i == 0) ? "%d" : ", %d", tmp->second_pin_index[i]);
            if((written < 0) || (written >= remaining_bytes)) {
                // can't add anything more, so return.
                return;
            }
            remaining_bytes -= written;
            b += written;
        }
        written = snprintf(b, remaining_bytes, ") %d(", tmp->third_pin_count);
        if((written < 0) || (written >= remaining_bytes)) {
            // can't add anything more, so return.
            return;
        }
        remaining_bytes -= written;
        b += written;

        // third pin indices
        for(uint8_t i = 0; i < tmp->third_pin_count; ++i) {
            written =
                snprintf(b, remaining_bytes, (i == 0) ? "%d" : ", %d", tmp->third_pin_index[i]);
            if((written < 0) || (written >= remaining_bytes)) {
                // can't add anything more, so return.
                return;
            }
            remaining_bytes -= written;
            b += written;
        }
        written = snprintf(b, remaining_bytes, ")");
        if((written < 0) || (written >= remaining_bytes)) {
            // can't add anything more, so return.
            return;
        }
        remaining_bytes -= written;
        b += written;
    }
    if(r2 != NULL) {
        const ComboLockCombination* tmp = r2;
        const char rnum = '2';

        written = snprintf(
            b,
            remaining_bytes,
            "\nR%c: %d  %d(",
            rnum,
            tmp->first_pin_index,
            tmp->second_pin_count);
        if((written < 0) || (written >= remaining_bytes)) {
            // can't add anything more, so return.
            return;
        }
        remaining_bytes -= written;
        b += written;

        // second pin indices
        for(uint8_t i = 0; i < tmp->second_pin_count; ++i) {
            written =
                snprintf(b, remaining_bytes, (i == 0) ? "%d" : ", %d", tmp->second_pin_index[i]);
            if((written < 0) || (written >= remaining_bytes)) {
                // can't add anything more, so return.
                return;
            }
            remaining_bytes -= written;
            b += written;
        }
        written = snprintf(b, remaining_bytes, ") %d(", tmp->third_pin_count);
        if((written < 0) || (written >= remaining_bytes)) {
            // can't add anything more, so return.
            return;
        }
        remaining_bytes -= written;
        b += written;

        // third pin indices
        for(uint8_t i = 0; i < tmp->third_pin_count; ++i) {
            written =
                snprintf(b, remaining_bytes, (i == 0) ? "%d" : ", %d", tmp->third_pin_index[i]);
            if((written < 0) || (written >= remaining_bytes)) {
                // can't add anything more, so return.
                return;
            }
            remaining_bytes -= written;
            b += written;
        }
        written = snprintf(b, remaining_bytes, ")");
        if((written < 0) || (written >= remaining_bytes)) {
            // can't add anything more, so return.
            return;
        }
        remaining_bytes -= written;
        b += written;
    }

    // that's it ... best effort completed
    return;
}

static void
    calculate_solution(const ComboLockCrackerModel* model, ComboLockCombination* solution) {
    // First things first... zero the solution structure
    memset(solution, 0, sizeof(ComboLockCombination));

    // calculate first pin (index == value for numeric combo locks)
    if(true) {
        // From old code:
        //     For numeric locks (0..39):
        //         If the resistance float value corresponds to a whole number:
        //             the first digit is: (int(number) + 5)
        //         Else:
        //             round up:           (int(number) + 5) + 1
        //
        // Converting to calculations using resistance indices 0..79:
        //     Get integer value:           (index / 2)
        //     Is index for a Whole number: (index % 2u == 0u)
        //
        // Thus, the first digit is calculated as:

        unsigned int pin0 = model->resistance_index / 2u;
        if(model->resistance_index % 2u != 0u) {
            pin0 += 6;
        } else {
            pin0 += 5;
        }
        pin0 %= LOCK_INDEX_COUNT;
        solution->first_pin_index = pin0;
    }

    uint8_t remainder = solution->first_pin_index % 4;

    // calculate the potential THIRD pins
    if(true) {
        uint8_t a = model->first_lock_index; //  index == value for numeric combo locks
        uint8_t b = model->second_lock_index; // index == value for numeric combo locks

        // Third digit:
        //    Check N, N+10, N+20, N+30 (for N is either of the two lock indices)
        // If any of those (value % 4) == (first pin % 4),
        // then it's a potential solution.
        //
        // Typically:
        // * lock1 and lock2 are offset by an odd value
        // * thus, only one of them would ever match the modulo by adding multiples of 10.
        // * 10 % 4 == 2, so only half the additions (+0, +10, +20, +30) will match the modulo.
        // * Thus, expect to get two values stored.
        // * NOTE: Invalid inputs might store 4 values.
        for(uint8_t i = 0u; i < 4u; i++) {
            if((a % 4u) == remainder) {
                solution->third_pin_index[solution->third_pin_count++] = a;
            }
            if((b % 4u) == remainder) {
                solution->third_pin_index[solution->third_pin_count++] = b;
            }
            a = (a + 10u) % 40u;
            b = (b + 10u) % 40u;
        }
    }

    // calculate the potential SECOND pins
    if(true) {
        // first two possibilities: remainder + 2, remainder + 6
        // Note that modulo here is redundant, as remainder is in range [0..3]
        uint8_t row_1 = (remainder + 2) % LOCK_INDEX_COUNT;
        uint8_t row_2 = (row_1 + 4) % LOCK_INDEX_COUNT;
        solution->second_pin_index[solution->second_pin_count++] = row_1;
        solution->second_pin_index[solution->second_pin_count++] = row_2;

        for(uint8_t i = 0u; i < 4u; i++) {
            row_1 = (row_1 + 8u) % LOCK_INDEX_COUNT;
            row_2 = (row_2 + 8u) % LOCK_INDEX_COUNT;
            solution->second_pin_index[solution->second_pin_count++] = row_1;
            solution->second_pin_index[solution->second_pin_count++] = row_2;
        }
    }
    // O(n^2) sorting of second pin numbers ... but as n is small (~8) it's good enough
    for(uint8_t i = 0; i < solution->second_pin_count - 1; i++) {
        // ensure smallest of all remaining values is in index i...
        for(uint8_t j = i + 1; j < solution->second_pin_count; j++) {
            // by comparing against all the remaining indices
            if(solution->second_pin_index[i] > solution->second_pin_index[j]) {
                // swap the values so smallest value comes first
                uint8_t temp = solution->second_pin_index[i];
                solution->second_pin_index[i] = solution->second_pin_index[j];
                solution->second_pin_index[j] = temp;
            }
        }
    }
}

static void
    fill_model_result_with_solution(ComboLockCrackerModel* model, ComboLockCombination* solution) {
    // TODO: consider a struct + helper function to store buffer + remaining_bytes
    //       and a helper variadic function that does the repeated bits here....
    //       (saves result of snprintf(), checks result, reduces remaining bytes available, etc.)
    //       this way, folks can just pass the buffer wrapper struct to the helper function,
    //       even if there's no space left, and nothing bad happens ... but the caller's code
    //       ends up MUCH easier to read.
    int pos = 0;
    if(true) {
        const char* s = label_from_solution_index(model, solution->first_pin_index);
        int written = snprintf(
            model->result + pos, sizeof(model->result) - pos, "First Pin: %s\nSecond Pin(s): ", s);
        if((written < 0) || (written >= (int)(sizeof(model->result) - pos))) {
            // LOG ERROR -- ran out of buffer space before full solution output
            return;
        }
        pos += written;
    }

    for(uint8_t i = 0; i < solution->second_pin_count; i++) {
        const char* s = label_from_solution_index(model, solution->second_pin_index[i]);
        int written = snprintf(model->result + pos, sizeof(model->result) - pos, "%s", s);
        if(written < 0 || written >= (int)(sizeof(model->result) - pos)) {
            // LOG ERROR -- ran out of buffer space before full solution output
            return;
        }
        pos += written;

        // append a comma if there are still more items (and `\n` after third result)
        if(i < solution->second_pin_count - 1) {
            const char* sep = (i == 3) ? ",\n -> " : ", ";
            written = snprintf(model->result + pos, sizeof(model->result) - pos, "%s", sep);
            if(written < 0 || written >= (int)(sizeof(model->result) - pos)) {
                // LOG ERROR -- ran out of buffer space before full solution output
                return;
            }
            pos += written;
        }
    }

    if(true) {
        int written =
            snprintf(model->result + pos, sizeof(model->result) - pos, "\nThird Pin(s): ");
        if((written < 0) || (written >= (int)(sizeof(model->result) - pos))) {
            // LOG ERROR -- ran out of buffer space before full solution output
            return;
        }
        pos += written;
    }

    for(uint8_t i = 0; i < solution->third_pin_count; i++) {
        const char* s = label_from_solution_index(model, solution->third_pin_index[i]);
        int written = snprintf(model->result + pos, sizeof(model->result) - pos, "%s", s);
        if((written < 0) || (written >= (int)(sizeof(model->result) - pos))) {
            // LOG ERROR -- ran out of buffer space before full solution output
            return;
        }
        pos += written;

        if(i < solution->third_pin_count - 1) {
            written = snprintf(model->result + pos, sizeof(model->result) - pos, ", ");
            if((written < 0) || (written >= (int)(sizeof(model->result) - pos))) {
                // LOG ERROR -- ran out of buffer space before full solution output
                return;
            }
            pos += written;
        }
    }
    return;
}
/**
 * @brief      calculate the combination based on inputs, AND displays the results
 * @param      model   the model containing input values
 */
static void calculate_combo(ComboLockCrackerModel* model) {
    // For numeric locks (0..39):
    //     If the resistance corresponds to a whole number,
    //     then the first digit is: (int(number) + 5)
    //     Otherwise round up:      (int(number) + 6)
    // Converting to using indices 0..79:
    //     Whole number from index: (index % 2u == 0u)
    //     Index increases by:      (index + 10 + ((index % 2u == 0u) ? 0u : 1u)) % RESISTANCE_INDEX_COUNT

    ComboLockCombination result = {};
    calculate_solution(model, &result);
    if((result.third_pin_count < 1) || (result.second_pin_count < 1)) {
        (void)SolutionComparator;
        dump_state_and_combinations_to_model_result(model, &result, NULL);
        return;
    }

    fill_model_result_with_solution(model, &result);
    return;
}

/**
 * @brief      callback for exiting the application.
 * @details    this function is called when user press back button.
 * @param      _context  the context - unused
 * @return     next view id
 */
static uint32_t combo_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

/**
 * @brief      callback for returning to submenu.
 * @details    this function is called when user press back button.
 * @param      _context  the context - unused
 * @return     next view id
 */
static uint32_t combo_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return ComboViewSubmenu;
}

/**
 * @brief      handle submenu item selection.
 * @details    this function is called when user selects an item from the submenu.
 * @param      context  the context - ComboLockCrackerApp object.
 * @param      index    the ComboSubmenuIndex item that was clicked.
 */
static void combo_submenu_callback(void* context, uint32_t index) {
    ComboLockCrackerApp* app = (ComboLockCrackerApp*)context;
    switch(index) {
    case ComboSubmenuIndexCracker:
        view_dispatcher_switch_to_view(app->view_dispatcher, ComboViewCracker);
        break;
    case ComboSubmenuIndexTutorial:
        view_dispatcher_switch_to_view(app->view_dispatcher, ComboViewTutorial);
        break;
    case ComboSubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, ComboViewAbout);
        break;
    default:
        break;
    }
}

/**
 * @brief      callback for drawing the cracker screen.
 * @details    this function is called when the screen needs to be redrawn.
 * @param      canvas  the canvas to draw on.
 * @param      model   the model - ComboLockCrackerModel object.
 */
static void combo_view_cracker_draw_callback(Canvas* canvas, void* model) {
    ComboLockCrackerModel* my_model = (ComboLockCrackerModel*)model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    char buf[16];
    int icon_width = 32;
    int icon_x = 128 - icon_width - 2; // moved 3 pixels to the right
    int icon_y = 2;
    int text_x = 2;
    int value_x = 75;
    int indicator_offset = -5;

    canvas_draw_str(canvas, text_x, 12, "First Lock:");
    snprintf(buf, sizeof(buf), "%s", lock1_label_from_model(my_model));
    canvas_draw_str(
        canvas,
        value_x + (my_model->selected == 0 ? indicator_offset : 0),
        12,
        (my_model->selected == 0 ? ">" : ""));
    canvas_draw_str(canvas, value_x, 12, buf);

    canvas_draw_str(canvas, text_x, 24, "Second Lock:");
    snprintf(buf, sizeof(buf), "%s", lock2_label_from_model(my_model));
    canvas_draw_str(
        canvas,
        value_x + (my_model->selected == 1 ? indicator_offset : 0),
        24,
        (my_model->selected == 1 ? ">" : ""));
    canvas_draw_str(canvas, value_x, 24, buf);

    canvas_draw_str(canvas, text_x, 36, "Resistance:");
    snprintf(buf, sizeof(buf), "%s", resistance_label_from_model(my_model));
    canvas_draw_str(
        canvas,
        value_x + (my_model->selected == 2 ? indicator_offset : 0),
        36,
        (my_model->selected == 2 ? ">" : ""));
    canvas_draw_str(canvas, value_x, 36, buf);

    canvas_draw_str(canvas, text_x, 48, "LockType: ");
    snprintf(buf, sizeof(buf), "%s", lock_type_label(my_model));
    canvas_draw_str(
        canvas,
        value_x + (my_model->selected == 3 ? indicator_offset : 0),
        48,
        (my_model->selected == 3 ? ">" : ""));
    canvas_draw_str(canvas, value_x, 48, buf);

    canvas_draw_str(canvas, text_x, 62, "OK to calculate");
    canvas_draw_icon(canvas, icon_x, icon_y, &I_lock32x32);
}

/**
 * @brief      callback for cracker screen input.
 * @details    this function is called when the user presses or holds a button while on the cracker screen.
 * @param      event    the event - InputEvent object.
 * @param      context  the context - ComboLockCrackerApp object.
 * @return     true if the event was handled, false otherwise.
 */
static bool combo_view_cracker_input_callback(InputEvent* event, void* context) {
    ComboLockCrackerApp* app = (ComboLockCrackerApp*)context;

    bool redraw = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                app->view_cracker,
                ComboLockCrackerModel * model,
                { model->selected = (model->selected > 0) ? model->selected - 1 : 3; },
                redraw);
            break;
        case InputKeyDown:
            with_view_model(
                app->view_cracker,
                ComboLockCrackerModel * model,
                { model->selected = (model->selected < 3) ? model->selected + 1 : 0; },
                redraw);
            break;
        case InputKeyLeft:
            with_view_model(
                app->view_cracker,
                ComboLockCrackerModel * model,
                {
                    if(model->selected == 0 && model->first_lock_index > 0) {
                        model->first_lock_index--;
                    }
                    if(model->selected == 1 && model->second_lock_index > 0) {
                        model->second_lock_index--;
                    }
                    if(model->selected == 2 && model->resistance_index > 0) {
                        model->resistance_index -= 1;
                    }
                    if(model->selected == 3) {
                        model->lock_type =
                            (model->lock_type + COMBO_LOCK_TYPE_COUNT - 1) % COMBO_LOCK_TYPE_COUNT;
                    }
                },
                redraw);
            break;
        case InputKeyRight:
            with_view_model(
                app->view_cracker,
                ComboLockCrackerModel * model,
                {
                    if(model->selected == 0 && model->first_lock_index < 39) {
                        model->first_lock_index++;
                    }
                    if(model->selected == 1 && model->first_lock_index < 39) {
                        model->second_lock_index++;
                    }
                    if(model->selected == 2 && model->resistance_index < 79) {
                        model->resistance_index++;
                    }
                    if(model->selected == 3) {
                        model->lock_type = (model->lock_type + 1) % COMBO_LOCK_TYPE_COUNT;
                    }
                },
                redraw);
            break;
        case InputKeyOk:
            view_dispatcher_send_custom_event(app->view_dispatcher, ComboEventIdCalculateCombo);
            return true;
        default:
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyLeft:
            with_view_model(
                app->view_cracker,
                ComboLockCrackerModel * model,
                {
                    if(model->selected == 0 && model->first_lock_index > 0) {
                        model->first_lock_index--;
                    }
                    if(model->selected == 1 && model->second_lock_index > 0) {
                        model->second_lock_index--;
                    }
                    if(model->selected == 2 && model->resistance_index > 0) {
                        model->resistance_index--;
                    }
                    if(model->selected == 3) {
                        model->lock_type =
                            (model->lock_type + COMBO_LOCK_TYPE_COUNT - 1) % COMBO_LOCK_TYPE_COUNT;
                    }
                },
                redraw);
            break;
        case InputKeyRight:
            with_view_model(
                app->view_cracker,
                ComboLockCrackerModel * model,
                {
                    if(model->selected == 0 && model->first_lock_index < 39) {
                        model->first_lock_index++;
                    }
                    if(model->selected == 1 && model->second_lock_index < 39) {
                        model->second_lock_index++;
                    }
                    if(model->selected == 2 && model->resistance_index < 79) {
                        model->resistance_index++;
                    }
                    if(model->selected == 3) {
                        model->lock_type = (model->lock_type + 1) % COMBO_LOCK_TYPE_COUNT;
                    }
                },
                redraw);
            break;
        default:
            break;
        }
    }

    return false;
}

/**
 * @brief      callback for custom events.
 * @details    this function is called when a custom event is sent to the view dispatcher.
 * @param      event    the event id - ComboEventId value.
 * @param      context  the context - ComboLockCrackerApp object.
 */
static bool combo_view_cracker_custom_event_callback(uint32_t event, void* context) {
    ComboLockCrackerApp* app = (ComboLockCrackerApp*)context;

    switch(event) {
    case ComboEventIdRedrawScreen: {
        bool redraw = true;
        with_view_model(
            app->view_cracker, ComboLockCrackerModel * _model, { UNUSED(_model); }, redraw);
        return true;
    }
    case ComboEventIdCalculateCombo: {
        bool redraw = false;
        with_view_model(
            app->view_cracker,
            ComboLockCrackerModel * model,
            {
                calculate_combo(model);
                widget_reset(app->widget_results);
                widget_add_text_scroll_element(app->widget_results, 2, 2, 124, 60, model->result);
            },
            redraw);

        view_dispatcher_switch_to_view(app->view_dispatcher, ComboViewResults);
        return true;
    }
    default:
        return false;
    }
}

static ComboLockCrackerApp* combo_app_alloc() {
    ComboLockCrackerApp* app = (ComboLockCrackerApp*)malloc(sizeof(ComboLockCrackerApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, "Crack Lock", ComboSubmenuIndexCracker, combo_submenu_callback, app);
    submenu_add_item(
        app->submenu, "Tutorial", ComboSubmenuIndexTutorial, combo_submenu_callback, app);
    submenu_add_item(app->submenu, "About", ComboSubmenuIndexAbout, combo_submenu_callback, app);
    view_set_previous_callback(submenu_get_view(app->submenu), combo_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ComboViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, ComboViewSubmenu);

    app->view_cracker = view_alloc();
    view_set_draw_callback(app->view_cracker, combo_view_cracker_draw_callback);
    view_set_input_callback(app->view_cracker, combo_view_cracker_input_callback);
    view_set_previous_callback(app->view_cracker, combo_navigation_submenu_callback);
    view_set_context(app->view_cracker, app);
    view_set_custom_callback(app->view_cracker, combo_view_cracker_custom_event_callback);
    view_allocate_model(app->view_cracker, ViewModelTypeLockFree, sizeof(ComboLockCrackerModel));

    ComboLockCrackerModel* model = view_get_model(app->view_cracker);
    model->first_lock_index = 1;
    model->second_lock_index = 8;
    model->resistance_index = 40;
    model->selected = 0;
    memset(model->result, 0, sizeof(model->result));

    view_dispatcher_add_view(app->view_dispatcher, ComboViewCracker, app->view_cracker);

    app->widget_results = widget_alloc();
    view_set_previous_callback(
        widget_get_view(app->widget_results), combo_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ComboViewResults, widget_get_view(app->widget_results));

    app->widget_tutorial = widget_alloc();
    widget_add_text_scroll_element(app->widget_tutorial, 0, 0, 128, 64, gc_instructions_numeric);
    view_set_previous_callback(
        widget_get_view(app->widget_tutorial), combo_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ComboViewTutorial, widget_get_view(app->widget_tutorial));

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Combo Lock Cracker\n"
        "---\n"
        "Based on Samy Kamkar's Master Lock research.\n"
        "Crack Combo Locks in 8 tries\n"
        "https://samy.pl/master/\n"
        "README at https://github.com/CharlesTheGreat77/ComboCracker-FZ\n");
    view_set_previous_callback(
        widget_get_view(app->widget_about), combo_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, ComboViewAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    return app;
}

/**
 * @brief      free the combo application.
 * @details    this function frees the application resources.
 * @param      app  the application object.
 */
static void combo_app_free(ComboLockCrackerApp* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, ComboViewAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, ComboViewTutorial);
    widget_free(app->widget_tutorial);
    view_dispatcher_remove_view(app->view_dispatcher, ComboViewResults);
    widget_free(app->widget_results);
    view_dispatcher_remove_view(app->view_dispatcher, ComboViewCracker);
    view_free(app->view_cracker);
    view_dispatcher_remove_view(app->view_dispatcher, ComboViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t combo_cracker_app(void* _p) {
    UNUSED(_p);

    ComboLockCrackerApp* app = combo_app_alloc();
    view_dispatcher_run(app->view_dispatcher);

    combo_app_free(app);
    return 0;
}

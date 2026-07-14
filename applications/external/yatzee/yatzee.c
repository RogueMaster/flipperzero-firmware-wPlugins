#include "yatzee_icons.h"

#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>

#include <stdlib.h>
#include <string.h>

#define BASE_X      18
#define BASE_Y      44
#define DICE_OFFSET 12
#define HOLD        "*"
#define MAX_DICE    5
#define NUM_SCORES  13

#define SCORE_1              0
#define SCORE_2              1
#define SCORE_3              2
#define SCORE_4              3
#define SCORE_5              4
#define SCORE_6              5
#define SCORE_3_OF_A_KIND    6
#define SCORE_4_OF_A_KIND    7
#define SCORE_FULL_HOUSE     8
#define SCORE_SMALL_STRAIGHT 9
#define SCORE_LARGE_STRAIGHT 10
#define SCORE_CHANCE         11
#define SCORE_YATZEE         12

bool new_game = true;
bool game_over = false;
bool bonus_added = false;
int8_t num_bonus_yatzees = 0;

// struct to hold image posistion for dice
typedef struct {
    //    +-----x
    //    |
    //    |
    //    y
    uint8_t x;
    uint8_t y;
} ImagePosition;

typedef struct {
    char* name;
    uint32_t value;
    bool used;
    int8_t row;
    int8_t col;
    uint8_t (*fn)(); // pointer to function that calculates score
} Score;

typedef struct {
    uint8_t index;
    uint8_t value;
    bool isHeld;
} Dice;

typedef struct {
    int index;
    char* symbol;
} Cursor;

// locations for the dice images
ImagePosition position[5] = {
    {.x = BASE_X - DICE_OFFSET, .y = BASE_Y},
    {.x = BASE_X * 2 - DICE_OFFSET, .y = BASE_Y},
    {.x = BASE_X * 3 - DICE_OFFSET, .y = BASE_Y},
    {.x = BASE_X * 4 - DICE_OFFSET, .y = BASE_Y},
    {.x = BASE_X * 5 - DICE_OFFSET, .y = BASE_Y},
};

// these are the positions that the score cursor will cycle through
ImagePosition score_positions[13] = {
    {.x = 15, .y = 0},
    {.x = 15, .y = 9},
    {.x = 15, .y = 18},
    {.x = 15, .y = 27},
    {.x = 44, .y = 0},
    {.x = 44, .y = 9},
    {.x = 44, .y = 18},
    {.x = 44, .y = 27},
    {.x = 77, .y = 0},
    {.x = 77, .y = 9},
    {.x = 77, .y = 18},
    {.x = 77, .y = 27},
    {.x = 91, .y = 21},
};

// cursor to select dice
Cursor cursor = {.index = 0, .symbol = "^"};

// cursor to select score
Cursor scoreCursor = {.index = -1, .symbol = "_"};

// setup array to store dice info
Dice dices[5] = {
    {.index = 0, .value = 1, .isHeld = false},
    {.index = 1, .value = 1, .isHeld = false},
    {.index = 2, .value = 1, .isHeld = false},
    {.index = 3, .value = 1, .isHeld = false},
    {.index = 4, .value = 1, .isHeld = false},
};

uint8_t upperScore = 0;
int32_t lowerScore = 0;
int32_t totalScore = 0;
uint8_t roll = 0;
uint8_t totalrolls = 0;

// #############################################
// # The following methods add the score for   #
// # whichever number is mentioned.            #
// #############################################
static uint8_t ones() {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 5; i++) {
        if(dices[i].value == 1) {
            sum++;
        }
    }
    return sum;
}

static uint8_t twos() {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 5; i++) {
        if(dices[i].value == 2) {
            sum = sum + 2;
        }
    }
    return sum;
}

static uint8_t threes() {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 5; i++) {
        if(dices[i].value == 3) {
            sum = sum + 3;
        }
    }
    return sum;
}

static uint8_t fours() {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 5; i++) {
        if(dices[i].value == 4) {
            sum = sum + 4;
        }
    }
    return sum;
}

static uint8_t fives() {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 5; i++) {
        if(dices[i].value == 5) {
            sum = sum + 5;
        }
    }
    return sum;
}

static uint8_t sixes() {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < 5; i++) {
        if(dices[i].value == 6) {
            sum = sum + 6;
        }
    }
    return sum;
}

// ####################################################
// # Helper methods for the special score types       #
// # defined before them so they can be used          #
// # since this whole thing is a linear mess          #
// # lol.                                             #
// # add_dice:                                        #
// #    inputs: none                                  #
// #    output: int8_t value of roll                  #
// # check_if_score_used:
// #    inputs: Score
// #    output: true if score.used = true
// # # # # # # # # # # # # # # # # # # # # # # # # # #
int8_t add_dice() {
    int8_t sum = 0;
    for(int8_t i = 0; i < MAX_DICE; i++) {
        sum += dices[i].value;
    }
    return sum;
}

bool check_if_score_used(Score score) {
    if(score.used == true) {
        return true;
    } else {
        return false;
    }
}

// #############################################
// # Methods to calculate scores for the fancy #
// # scoring types: 3 of a kind, 4 of a kind,  #
// # Full house, small straight, large straight#
// # chance & yatzee.                          #
// #############################################
static uint8_t threekind() {
    int8_t score = 0;
    for(int8_t num = 1; num < 7; num++) {
        int8_t sum = 0;

        for(int8_t i = 0; i < MAX_DICE; i++) {
            if(dices[i].value == num) {
                sum++;
            }
            if(sum > 2) {
                score = add_dice();
            }
        }
    }
    return score;
}

static uint8_t fourkind() {
    int8_t score = 0;
    for(int8_t num = 1; num < 7; num++) {
        int8_t sum = 0;

        for(int8_t i = 0; i < MAX_DICE; i++) {
            if(dices[i].value == num) {
                sum++;
            }
            if(sum > 3) {
                score = add_dice();
            }
        }
    }
    return score;
}

static uint8_t fullhouse() {
    int8_t counts[7] = {0};
    bool has_three = false;
    bool has_two = false;

    for(int8_t i = 0; i < MAX_DICE; i++) {
        counts[dices[i].value]++;
    }

    for(int8_t i = 1; i <= 6; i++) {
        if(counts[i] == 3)
            has_three = true;
        else if(counts[i] == 2)
            has_two = true;
    }

    return (has_three && has_two) ? 25 : 0;
}

// # # # # # # # # # # # # # # # # # # # # # # # # # # #
// # I'm dumb so I asked ChatGPT to write the          #
// # smallstraight function for me. Then I adapted it  #
// # fo the largestraight function.                    #
// # # # # # # # # # # # # # # # # # # # # # # # # # # #
static uint8_t smallstraight() {
    // Create a new array with the frequencies of the different die faces
    int8_t frequencies[6] = {0};

    for(int8_t i = 0; i < 5; i++) {
        int8_t face = dices[i].value;
        frequencies[face - 1]++;
    }

    // Check if there is a sequence of 4 consecutive die faces with at least one die
    bool found_small_straight = false;
    for(int i = 0; i < 3 && !found_small_straight; i++) {
        if(frequencies[i] > 0 && frequencies[i + 1] > 0 && frequencies[i + 2] > 0 &&
           frequencies[i + 3] > 0) {
            found_small_straight = true;
        }
    }

    if(found_small_straight) {
        return 30;
    } else {
        return 0;
    }
}

static uint8_t largestraight() {
    // Create a new array with the frequencies of the different die faces
    int8_t frequencies[6] = {0};

    for(int8_t i = 0; i < 5; i++) {
        int8_t face = dices[i].value;
        frequencies[face - 1]++;
    }

    // Check if there is a sequence of 4 consecutive die faces with at least one die
    bool found_large_straight = false;
    for(int i = 0; i < 3 && !found_large_straight; i++) {
        if(frequencies[i] > 0 && frequencies[i + 1] > 0 && frequencies[i + 2] > 0 &&
           frequencies[i + 3] > 0 && frequencies[i + 4] > 0) {
            found_large_straight = true;
        }
    }

    if(found_large_straight) {
        return 40;
    } else {
        return 0;
    }
}

static uint8_t chance() {
    // chance allows your roll to count for the raw number of pips showing
    int8_t sum = 0;
    for(int8_t i = 0; i < MAX_DICE; i++) {
        sum += dices[i].value;
    }
    return sum;
}

static uint8_t yatzee() {
    // checks if all die.values are equal to the first die
    int8_t val = dices[0].value;
    for(int8_t i = 1; i < MAX_DICE; i++) {
        // if value is the same as the first die, continue to next
        if(dices[i].value == val) {
            continue;
        } else {
            // if any value is not equal to the first die,
            // this is not a yatzee and we return 0
            return 0;
        }
    }
    return 50;
}

// # # # # # # # # # # # # # # # # # # # # # # # # # #
// # Method to return true if yatzee returns 50      #
// #                                                 #
// # # # # # # # # # # # # # # # # # # # # # # # # # #
// static bool check_for_bonus_yatzee() {
//   if (yatzee()==50){
//     return true;
//   } else {
//     return false;
//   }
// }

// Scorecard defined here so that we can use pointers to the functions
// defined above
Score scorecard[13] = {
    {.name = "1", .value = 0, .used = false, .row = 0, .col = 0, .fn = &ones},
    {.name = "2", .value = 0, .used = false, .row = 1, .col = 0, .fn = &twos},
    {.name = "3", .value = 0, .used = false, .row = 2, .col = 0, .fn = &threes},
    {.name = "4", .value = 0, .used = false, .row = 3, .col = 0, .fn = &fours},
    {.name = "5", .value = 0, .used = false, .row = 0, .col = 1, .fn = &fives},
    {.name = "6", .value = 0, .used = false, .row = 1, .col = 1, .fn = &sixes},
    {.name = "3k", .value = 0, .used = false, .row = 2, .col = 1, .fn = &threekind},
    {.name = "4k", .value = 0, .used = false, .row = 3, .col = 1, .fn = &fourkind},
    {.name = "Fh", .value = 0, .used = false, .row = 0, .col = 2, .fn = &fullhouse},
    {.name = "Sm", .value = 0, .used = false, .row = 1, .col = 2, .fn = &smallstraight},
    {.name = "Lg", .value = 0, .used = false, .row = 2, .col = 2, .fn = &largestraight},
    {.name = "Ch", .value = 0, .used = false, .row = 3, .col = 2, .fn = &chance},
    {.name = "Yz", .value = 0, .used = false, .row = 2, .col = 3, .fn = &yatzee},
};

// #############################################
// # begin draw callback                       #
// #                                           #
// #############################################
// define the callback for telling ViewPort how to update the screen
// not sure what ctx is but it seems important
static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_set_font(canvas, FontSecondary); //define a font so we can put letters on the screen
    int8_t selectorOffsetX = 8;
    int8_t selectorOffsetY = 16;
    char buffer[36];
    char bigbuffer[256];

    canvas_clear(canvas);

    // if new_game, show user instructions
    if(new_game) {
        canvas_set_font(canvas, FontPrimary);
        elements_multiline_text_aligned(canvas, 64, 0, AlignCenter, AlignTop, "Yatzee!");
        canvas_set_font(canvas, FontSecondary);
        snprintf(
            bigbuffer,
            sizeof(bigbuffer),
            "Up: Roll\nLeft/Right: Move cursor\nOK: Hold Die\nDown: Score");
        elements_multiline_text_aligned(canvas, 0, 8, AlignLeft, AlignTop, bigbuffer);
        elements_button_center(canvas, "Start!");
        return;

    } else {
        // draw border lines
        canvas_draw_line(canvas, 0, 37, 104, 37);
        canvas_draw_line(canvas, 104, 0, 104, 64);

        // iterate through dice and draw icon that correlates to dices[n].value, and the x,y position indicated by position[dices[i].index]
        for(int8_t i = 0; i < 5; i++) {
            if(dices[i].value == 1) {
                canvas_draw_icon(
                    canvas,
                    position[dices[i].index].x % 128,
                    position[dices[i].index].y % 64,
                    &I_die_1);
            } else if(dices[i].value == 2) {
                canvas_draw_icon(
                    canvas,
                    position[dices[i].index].x % 128,
                    position[dices[i].index].y % 64,
                    &I_die_2);
            } else if(dices[i].value == 3) {
                canvas_draw_icon(
                    canvas,
                    position[dices[i].index].x % 128,
                    position[dices[i].index].y % 64,
                    &I_die_3);
            } else if(dices[i].value == 4) {
                canvas_draw_icon(
                    canvas,
                    position[dices[i].index].x % 128,
                    position[dices[i].index].y % 64,
                    &I_die_4);
            } else if(dices[i].value == 5) {
                canvas_draw_icon(
                    canvas,
                    position[dices[i].index].x % 128,
                    position[dices[i].index].y % 64,
                    &I_die_5);
            } else if(dices[i].value == 6) {
                canvas_draw_icon(
                    canvas,
                    position[dices[i].index].x % 128,
                    position[dices[i].index].y % 64,
                    &I_die_6);
            }
        }

        // Puts an '*' above the die if hold is selected.
        int8_t holdOffsetX = 8;
        int8_t holdOffsetY = -5;
        for(int8_t i = 0; i < 5; i++) {
            if(dices[i].isHeld == 1) {
                elements_multiline_text_aligned(
                    canvas,
                    position[dices[i].index].x + holdOffsetX,
                    position[dices[i].index].y + holdOffsetY,
                    AlignCenter,
                    AlignTop,
                    HOLD);
            }
        }

        // Update die cursor location
        if(cursor.index != -1) {
            elements_multiline_text_aligned(
                canvas,
                position[cursor.index].x + selectorOffsetX,
                position[cursor.index].y + selectorOffsetY,
                AlignCenter,
                AlignTop,
                cursor.symbol);
        }

        // Update score cursor location
        if(scoreCursor.index != -1) {
            elements_multiline_text_aligned(
                canvas,
                score_positions[scoreCursor.index].x,
                score_positions[scoreCursor.index].y + 1,
                AlignLeft,
                AlignTop,
                scoreCursor.symbol);
        }

        // Update Roll

        // Scores are updated in groups on screen to help with formatting
        // first group is scorecard[0:7], second group is [8:12]
        // Cycle through first 8 scores, if cursor at score, update to show possible score
        // otherwise, show current scores value.
        for(int8_t i = 0; i < 8; i++) {
            if(scoreCursor.index == i && scorecard[i].used == false) {
                int possiblescore = (int)(*scorecard[i].fn)();

                snprintf(buffer, sizeof(buffer), "%s: %3u ", scorecard[i].name, possiblescore);
                canvas_draw_str_aligned(
                    canvas,
                    23 + 29 * scorecard[i].col,
                    9 * scorecard[i].row,
                    AlignRight,
                    AlignTop,
                    buffer);
            } else {
                uint8_t currentscore = scorecard[i].value;
                snprintf(buffer, sizeof(buffer), "%s: %3u ", scorecard[i].name, currentscore);
                canvas_draw_str_aligned(
                    canvas,
                    23 + 29 * scorecard[i].col,
                    9 * scorecard[i].row,
                    AlignRight,
                    AlignTop,
                    buffer);
            }
            if(scorecard[i].used) {
                canvas_draw_dot(canvas, 23 + 29 * scorecard[i].col, 3 + 9 * scorecard[i].row);
            }
        }

        // cycle through lower scores
        // NUM_SCORES minus one because the yatzee is 12 and is handled separately
        for(int8_t i = 8; i < NUM_SCORES - 1; i++) {
            if(scoreCursor.index == i && scorecard[i].used == false) {
                int possiblescore = (int)(*scorecard[i].fn)();

                snprintf(buffer, sizeof(buffer), " %s: %3u ", scorecard[i].name, possiblescore);
                canvas_draw_str_aligned(
                    canvas,
                    31 + 27 * scorecard[i].col,
                    9 * scorecard[i].row,
                    AlignRight,
                    AlignTop,
                    buffer);
            } else {
                uint8_t currentscore = scorecard[i].value;
                snprintf(buffer, sizeof(buffer), " %s: %3u ", scorecard[i].name, currentscore);
                canvas_draw_str_aligned(
                    canvas,
                    31 + 27 * scorecard[i].col,
                    9 * scorecard[i].row,
                    AlignRight,
                    AlignTop,
                    buffer);
            }
            if(scorecard[i].used) {
                canvas_draw_dot(canvas, 31 + 27 * scorecard[i].col, 3 + 9 * scorecard[i].row);
            }
        }

        // update yatzee score
        if(scoreCursor.index == SCORE_YATZEE && scorecard[SCORE_YATZEE].used == false) {
            int possiblescore = (int)(*scorecard[SCORE_YATZEE].fn)();

            snprintf(buffer, sizeof(buffer), "Yz\n%u", possiblescore);
            elements_multiline_text_aligned(canvas, 93, 10, AlignCenter, AlignTop, buffer);
        } else {
            snprintf(buffer, sizeof(buffer), "Yz\n%ld", scorecard[SCORE_YATZEE].value);
            elements_multiline_text_aligned(canvas, 93, 10, AlignCenter, AlignTop, buffer);
        }

        // Scores and roll number updated

        // sub score shows the 1-6 scores only. If this is >63 at the end of the game,
        // a 35 point bonus is added to the total score
        snprintf(buffer, sizeof(buffer), "Sub\n%u", upperScore);
        elements_multiline_text_aligned(canvas, 117, 0, AlignCenter, AlignTop, buffer);

        snprintf(buffer, sizeof(buffer), "Total\n%ld", totalScore);
        elements_multiline_text_aligned(canvas, 117, 22, AlignCenter, AlignTop, buffer);

        if(totalrolls == 0) {
            snprintf(buffer, sizeof(buffer), "Roll\n%s", " ");
            elements_multiline_text_aligned(canvas, 117, 64, AlignCenter, AlignBottom, buffer);
        } else {
            snprintf(buffer, sizeof(buffer), "Roll\n%u", totalrolls);
            elements_multiline_text_aligned(canvas, 117, 64, AlignCenter, AlignBottom, buffer);
        }

        // Check for then handle end of game

        // add num_bonus_yatzees to total rounds so that multiple
        // yatzees can be scored without impacting the number of rounds before
        // the game is over
        int8_t total_rounds = num_bonus_yatzees;
        // add up number of scores counted so far
        for(int8_t i = 0; i < NUM_SCORES; i++) {
            if(scorecard[i].used) {
                total_rounds++;
            }
        }

        // if total rounds is 13 + the number of bonus rounds,
        // thats it, game over.
        if(total_rounds == NUM_SCORES + num_bonus_yatzees) {
            // if scores of 1-6 add up to 63, a 35 point bonus is bonus_added
            // bonus_added = true keeps the game loop from
            // adding bonuses indefinetly
            if(upperScore >= 63 && bonus_added == false) {
                totalScore += 35;
                bonus_added = true;
            }
            // set game over to true and tell the user the game is over
            game_over = true;
            elements_button_center(canvas, "Game Over");
        }
    }
}

// define the callback for helping ViewPort get InputEvent and place it in the event_queue defined in the main method
static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// roll them diiiiceeee
static void roll_dice() {
    // increment roll count
    totalrolls++;
    for(uint8_t i = 0; i < MAX_DICE; i++) {
        // dont reroll if the dice is being held
        if(dices[i].isHeld == false) {
            dices[i].value = 1 + rand() % 6;
        }
    }
    // if 3 rolls have been used, force user to select a score.
    if(totalrolls == 3) {
        scoreCursor.index = 0;
    }
}

static void clear_board() {
    // reset board after adding score
    totalrolls = 0;
    for(int8_t i = 0; i < MAX_DICE; i++) {
        dices[i].isHeld = false;
    }
    scoreCursor.index = -1;
    cursor.index = 0;
}

static void add_score() {
    // return when scoring is not possible
    if(cursor.index != -1 || totalrolls == 0 ||
       (scoreCursor.index != SCORE_YATZEE && scorecard[scoreCursor.index].used)) {
        return;
    }

    // extra yatzee scores
    if(scoreCursor.index == SCORE_YATZEE && scorecard[scoreCursor.index].used) {
        uint8_t yatzee_score = (*scorecard[SCORE_YATZEE].fn)();
        scorecard[SCORE_YATZEE].value += 2 * yatzee_score;
        lowerScore += 100;
        num_bonus_yatzees++;
    }

    // upper score
    for(int8_t i = SCORE_1; i <= SCORE_6; i++) {
        if(scoreCursor.index == i && scorecard[scoreCursor.index].used == false) {
            scorecard[i].value = (*scorecard[i].fn)();
            upperScore += scorecard[i].value;
            scorecard[i].used = true;
        }
    }

    // lower score
    for(int8_t i = SCORE_3_OF_A_KIND; i <= SCORE_YATZEE; i++) {
        if(scoreCursor.index == i && scorecard[scoreCursor.index].used == false) {
            scorecard[i].value = (*scorecard[i].fn)();
            lowerScore += scorecard[i].value;
            scorecard[i].used = true;
        }
    }

    // recalculate total score
    totalScore = lowerScore + upperScore;
    clear_board();
}

// Entry Point
int32_t yatzee_main(void* p) {
    UNUSED(p);

    // Initialize event queue to handle incoming events like button presses
    // Use FuriMessageQueue as type as defined in furi api
    // InputEvents are supported by app_input_callback
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Initialize viewport
    ViewPort* view_port = view_port_alloc();

    // Set system callbacks
    view_port_draw_callback_set(view_port, app_draw_callback, view_port);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    // Open GUI & register viewport
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // hold input event
    InputEvent event;

    // Create a loop for the app to run in and handle InputEvents
    bool isRunning = true;

    while(isRunning) {
        if(totalrolls == 3) {
            cursor.index = -1;
        }
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if((event.type == InputTypePress) || event.type == InputTypeRepeat) {
                switch(event.key) {
                case InputKeyLeft:
                    if(cursor.index == -1) {
                        if(scoreCursor.index == 0 && totalrolls == 3) {
                            scoreCursor.index = NUM_SCORES - 1;
                        } else if(scoreCursor.index == 0) {
                            scoreCursor.index = -1;
                            cursor.index = 4;
                        } else {
                            scoreCursor.index--;
                        }
                    } else {
                        if(cursor.index == 0) {
                            cursor.index = -1;
                            scoreCursor.index = NUM_SCORES - 1;
                        } else {
                            cursor.index--;
                        }
                    }
                    break;
                case InputKeyRight:
                    // cursor.index == -1 means that scoreCursor is active
                    if(cursor.index == -1) {
                        if(scoreCursor.index == NUM_SCORES - 1 && totalrolls == 3) {
                            scoreCursor.index = 0;
                        } else if(scoreCursor.index == NUM_SCORES - 1) {
                            scoreCursor.index = -1;
                            cursor.index = 0;
                        } else {
                            scoreCursor.index++;
                        }
                        // if cursor.index is not -1, then dice cursor is active
                    } else {
                        if(cursor.index == 4) {
                            cursor.index = -1;
                            scoreCursor.index = 0;
                        } else {
                            cursor.index++;
                        }
                    }
                    break;
                case InputKeyUp:

                    if(totalrolls < 3) {
                        roll_dice();
                    }
                    // if (check_for_bonus_yatzee() && scorecard[13].used) {
                    //   num_bonus_yatzees++;
                    //   totalScore+=100;
                    //
                    //   clear_board();
                    // }
                    break;
                case InputKeyDown:
                    add_score();
                    break;
                case InputKeyOk:
                    if(new_game) {
                        new_game = false;
                        break;
                    }
                    if(game_over) {
                        isRunning = false;
                    }
                    if(cursor.index == -1 || totalrolls == 0) {
                        break;
                    }
                    if(dices[cursor.index].isHeld == false) {
                        dices[cursor.index].isHeld = true;
                    } else {
                        dices[cursor.index].isHeld = false;
                    }
                    break;
                default:
                    isRunning = false;
                    break;
                }
            }
        }
        // after every event, update view_port
        // uses app_draw_callback which is set before the game loop begins.
        view_port_update(view_port);
    }

    // cleanup
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}

#pragma once

#include <furi.h>
#include <vector>
#include "pinball0.h"
#include "objects.h"
#include "signals.h"

#define TABLE_SELECT       0
#define TABLE_ERROR        1
#define TABLE_SETTINGS     2
#define TABLE_INDEX_OFFSET 3

// Table display elements, rendered on the physical display coordinates,
// not the table's scaled coords
class DataDisplay {
public:
    enum Align {
        Horizontal,
        Vertical
    };
    DataDisplay(const Vec2& pos, int val, bool disp, Align align)
        : p(pos)
        , value(val)
        , display(disp)
        , alignment(align) {
    }
    Vec2 p;
    int value;
    bool display;
    Align alignment;
    virtual void draw(Canvas* canvas) = 0;
};
class Lives : public DataDisplay {
public:
    Lives()
        : DataDisplay(Vec2(), 3, false, Horizontal) {
    }
    void draw(Canvas* canvas);
};

class Score : public DataDisplay {
public:
    Score()
        : DataDisplay(Vec2(64 - 1, 1), 0, false, Horizontal) {
    }
    void draw(Canvas* canvas);
};

// Defines all of the elements on a pinball table:
// edges, bumpers, flipper locations, scoreboard
//
// Also used for other app "views", like the main menu (table select)
// and the Settings screen.
// TODO: make this better? eh, it works for now...
class Table {
public:
    Table();

    ~Table();

    std::vector<FixedObject*> objects;
    std::vector<Ball> balls; // current state of balls
    std::vector<Ball> balls_initial; // original positions, before release
    std::vector<Flipper> flippers;

    bool game_over;
    bool balls_released; // is ball in play?
    Lives lives;
    Score score;

    Plunger* plunger;

    // table bump / tilt tracking
    bool tilt_detect_enabled;
    uint32_t last_bump;
    uint32_t bump_count;

    SignalManager sm;

    void draw(Canvas* canvas);
};

// Read the list tables from the data folder and store in the state
void table_table_list_init(void* ctx);

// Reads the table file and creates the new table.
Table* table_load_table_from_file(PinballApp* ctx, size_t index);

// Loads the index'th table from the list
bool table_load_table(void* ctx, size_t index);

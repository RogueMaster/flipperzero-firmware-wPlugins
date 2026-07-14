#ifndef SAVE_H_
#define SAVE_H_

#include <stdint.h>
#include "game/Defines.h"

#define NUM_SAVE_SLOTS  3
#define NUM_HIGH_SCORES 3

struct SaveSlot {
    uint8_t hp;
    uint8_t level;
    uint8_t difficulty;
    uint8_t ammo;
    uint8_t inventoryFlags;
    uint8_t lives;
    int32_t score;
};

struct HighScore {
    char name[3];
    int32_t score;
};

struct SaveFile {
    SaveSlot slots[NUM_SAVE_SLOTS];
    HighScore scores[NUM_HIGH_SCORES];
};

class SaveSystem {
public:
    void init();

    void saveStateToActiveSlot();
    void restoreStateFromActiveSlot();
    void clearActiveSlot();

    bool trySubmitHighScore(int32_t score);

    void save();

    SaveFile saveFile;
    int8_t activeSlot;
};

#endif

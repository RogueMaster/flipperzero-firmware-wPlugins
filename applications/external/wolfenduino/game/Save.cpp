#include "game/Save.h"
#include "game/Engine.h"

void SaveSystem::init() {
    readSaveFile((uint8_t*)&saveFile, sizeof(SaveFile));

    if(saveFile.scores[0].name[0] == 0) {
        saveFile.scores[0].name[0] = 'J';
        saveFile.scores[0].name[1] = 'H';
        saveFile.scores[0].name[2] = 'H';
        saveFile.scores[0].score = 10000;

        saveFile.scores[1].name[0] = 'K';
        saveFile.scores[1].name[1] = 'E';
        saveFile.scores[1].name[2] = 'V';
        saveFile.scores[1].score = 9000;

        saveFile.scores[2].name[0] = 'M';
        saveFile.scores[2].name[1] = 'R';
        saveFile.scores[2].name[2] = 'B';
        saveFile.scores[2].score = 8000;
    }
}

void SaveSystem::save() {
    writeSaveFile((uint8_t*)&saveFile, sizeof(SaveFile));
}

void SaveSystem::saveStateToActiveSlot() {
    SaveSlot* slot = &saveFile.slots[activeSlot];
    slot->hp = engine.player.hp;
    slot->level = engine.map.currentLevel;
    slot->difficulty = engine.difficulty;
    slot->ammo = engine.player.weapon.ammo;
    slot->inventoryFlags = engine.player.inventoryFlags;
    slot->score = engine.player.score;
    slot->lives = engine.player.lives;
    save();
}

void SaveSystem::restoreStateFromActiveSlot() {
    SaveSlot* slot = &saveFile.slots[activeSlot];
    engine.player.hp = slot->hp;
    engine.map.currentLevel = slot->level;
    engine.difficulty = slot->difficulty;
    engine.player.weapon.ammo = slot->ammo;
    engine.player.inventoryFlags = slot->inventoryFlags;
    engine.player.score = slot->score;
    engine.player.lives = slot->lives;
}

void SaveSystem::clearActiveSlot() {
    saveFile.slots[activeSlot].hp = 0;
    save();
}

bool SaveSystem::trySubmitHighScore(int32_t score) {
    int insertIndex = -1;

    for(int n = 0; n < 3; n++) {
        if(score >= saveFile.scores[n].score) {
            insertIndex = n;
            break;
        }
    }

    if(insertIndex == -1) {
        return false;
    }

    for(int n = 2; n > insertIndex; n--) {
        saveFile.scores[n] = saveFile.scores[n - 1];
    }

    activeSlot = insertIndex;

    for(int n = 0; n < 3; n++) {
        saveFile.scores[insertIndex].name[n] = 'A';
    }
    saveFile.scores[insertIndex].score = score;

    return true;
}

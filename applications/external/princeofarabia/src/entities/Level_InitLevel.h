void loadItems(uint8_t level, Prince& prince) {
    FX::seekData(FX::readIndexedUInt24(Levels::Level_Items, level));
    FX::readBytes((uint8_t*)&this->items, Constants::Items_Count * sizeof(Item));
    FX::readEnd();

#ifdef DEBUG_LEVELS
    prince.setSword(level > 1);
#endif
}

void loadMap(GamePlay& gamePlay) {
    uint8_t offset = 0;

    if(this->xLoc) {
        offset = this->xLoc - 6;
    }

    // Background ..

    for(uint8_t x = 0; x < 5 * 22; x++) {
        *(&bg[0][0] + x) = TILE_NONE;
    }

    for(int8_t y = this->yLoc - 1; y < (int8_t)(this->yLoc + 4); y++) {
        if(y >= 0) {
            FX::seekDataArray(
                FX::readIndexedUInt24(Levels::Level_BG, gamePlay.level), y, offset, this->width);

            for(uint8_t x = 0; x < 22; x++) {
                if(x >= 6 || offset) {
                    bg[y - this->yLoc + 1][x] = static_cast<int8_t>(FX::readPendingUInt8());
                }
            }

            FX::readEnd();
        }
    }

    // Foreground ..

    for(uint8_t x = 0; x < 5 * 22; x++) {
        *(&fg[0][0] + x) = TILE_FG_WALL_1;
    }

    for(int8_t y = this->yLoc - 1; y < (int8_t)(this->yLoc + 4); y++) {
        if(y >= 0) {
            FX::seekDataArray(
                FX::readIndexedUInt24(Levels::level_FG, gamePlay.level), y, offset, this->width);

            for(uint8_t x = 0; x < 22; x++) {
                if(x >= 6 || offset) {
                    fg[y - this->yLoc + 1][x] = static_cast<int8_t>(FX::readPendingUInt8());
                }
            }

            FX::readEnd();
        }
    }

#if defined(DEBUG) && defined(DEBUG_LEVEL_LOAD_MAP)
    printMap();
#endif
}



#if defined(DEBUG)

void printMap() {
#if defined(DEBUG) && defined(DEBUG_LEVEL_LOAD_MAP)

    DEBUG_PRINTLN(F("Map ---------------"));
    DEBUG_PRINT(F("xLoc: "));
    DEBUG_PRINT(xLoc);
    DEBUG_PRINT(F(", yLoc: "));
    DEBUG_PRINTLN(yLoc);
    DEBUG_PRINTLN(F("BG ---------------"));

    for(uint8_t y = 0; y < 5; y++) {
        for(uint8_t x = 0; x < 22; x++) {
            DEBUG_PRINT(bg[y][x]);
            DEBUG_PRINT(" ");

            if(x == 2 || x == 12) {
                DEBUG_PRINT("| ");
            }
        }

        DEBUG_PRINTLN("");
    }

    DEBUG_PRINTLN(F("FG ---------------"));

    for(uint8_t y = 0; y < 5; y++) {
        for(uint8_t x = 0; x < 16; x++) {
            DEBUG_PRINT(fg[y][x]);
            DEBUG_PRINT(" ");

            if(x == 2 || x == 12) {
                DEBUG_PRINT("| ");
            }
        }

        DEBUG_PRINTLN("");
    }

#endif
}

void printAction(Action action) {
    switch(action) {
    case Action::SmallStep:
        DEBUG_PRINT(F("SmallStep"));
        break;

    case Action::CrouchHop:
        DEBUG_PRINT(F("CrouchHop"));
        break;

    case Action::Step:
        DEBUG_PRINT(F("Step"));
        break;

    case Action::RunStart:
        DEBUG_PRINT(F("RunStart"));
        break;

    case Action::RunRepeat:
        DEBUG_PRINT(F("RunRepeat"));
        break;

    case Action::RunJump_Normal:
        DEBUG_PRINT(F("RunJump_Normal"));
        break;

    case Action::RunJump_Level6Exit:
        DEBUG_PRINT(F("RunJump_Level6Exi"));
        break;

    case Action::StandingJump:
        DEBUG_PRINT(F("StandingJump"));
        break;

    case Action::RunningTurn:
        DEBUG_PRINT(F("RunningTurn"));
        break;
    }
}

void printTileInfo(int8_t bgTile, int8_t fgTile) {
    DEBUG_PRINT(F("isWallTile("));
    DEBUG_PRINT(bgTile);
    DEBUG_PRINT(F(","));
    DEBUG_PRINT(fgTile);
    DEBUG_PRINT(F(") "));
    DEBUG_PRINT((uint8_t)this->isWallTile(fgTile, Constants::CoordNone, Constants::CoordNone));
    DEBUG_PRINT(F(", isGroundTile() "));
    DEBUG_PRINT(this->isGroundTile(bgTile));
    DEBUG_PRINT(F(", canFall() "));
    DEBUG_PRINTLN((uint8_t)this->canFall(bgTile));
}

void printCoordToIndex(Point point, int tileXIdx, int8_t tileYIdx) {
    DEBUG_PRINT(F("coordToTileIndexX "));
    DEBUG_PRINT(point.x);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(", coordToTileIndexY "));
    DEBUG_PRINT(point.y);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINTLN(tileYIdx);
}
#endif

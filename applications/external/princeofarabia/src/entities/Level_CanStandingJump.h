StandingJumpResult canStandingJump(Prince& prince) {
    int8_t tileXIdx = this->coordToTileIndexX(prince.getPosition().x) - this->getXLocation();
    int8_t tileYIdx = this->coordToTileIndexY(prince.getPosition().y) - this->getYLocation();
    int8_t distToEdgeOfCurrentTile =
        distToEdgeOfTile(prince.getDirection(), prince.getPosition().x);
    int8_t offset = prince.getDirection() == Direction::Left ? -1 : 1;

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP)
    DEBUG_PRINTLN(F("------------------------------"));
    DEBUG_PRINT(F("StandingJump coordToTileIndexX "));
    DEBUG_PRINT(prince.getPosition().x);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(", coordToTileIndexY "));
    DEBUG_PRINT(prince.getPosition().y);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINTLN(tileYIdx);
#endif

    WallTileResults wallTile2_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (1 * offset), tileYIdx, prince.getDirection(), true, 8);
    WallTileResults wallTile3_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (2 * offset), tileYIdx, prince.getDirection(), true, 6);
    WallTileResults wallTile4_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (3 * offset), tileYIdx, prince.getDirection(), true, 4);
    WallTileResults wallTile5_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (4 * offset), tileYIdx, prince.getDirection(), true, 2);
    WallTileResults wallTile2_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (1 * offset), tileYIdx + 1, prince.getDirection(), true, 8);
    WallTileResults wallTile3_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (2 * offset), tileYIdx + 1, prince.getDirection(), true, 6);
    WallTileResults wallTile4_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (3 * offset), tileYIdx + 1, prince.getDirection(), true, 4);
    WallTileResults wallTile5_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (4 * offset), tileYIdx + 1, prince.getDirection(), true, 2);

    bool isGroundTile2_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (1 * offset), tileYIdx);
    bool isGroundTile3_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (2 * offset), tileYIdx);
    bool isGroundTile4_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (3 * offset), tileYIdx);
    bool isGroundTile5_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (4 * offset), tileYIdx);
    bool isGroundTile2_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (1 * offset), tileYIdx + 1);
    bool isGroundTile3_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (2 * offset), tileYIdx + 1);
    bool isGroundTile4_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (3 * offset), tileYIdx + 1);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP)

    WallTileResults wallTile1_CurrLvl =
        this->isWallTile_ByCoords(tileXIdx, tileYIdx, prince.getDirection());
    bool isGroundTile5_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (4 * offset), tileYIdx + 1);

    if(prince.getDirection() == Direction::Left) {
        DEBUG_PRINT(F("______5 4 3 2 1\nWT CL "));
        DEBUG_PRINT((uint8_t)wallTile5_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile4_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile3_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile2_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile1_CurrLvl);
        DEBUG_PRINT(F("\nWT NL "));
        DEBUG_PRINT((uint8_t)wallTile5_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile4_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile3_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile2_NextLvl);
        DEBUG_PRINT(F(" _\nGT CL "));
        DEBUG_PRINT((uint8_t)isGroundTile5_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile4_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile3_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile2_CurrLvl);
        DEBUG_PRINT(F(" _\nGT NL "));
        DEBUG_PRINT((uint8_t)isGroundTile5_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile4_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile3_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile2_NextLvl);
        DEBUG_PRINT(F(" _\nDist "));
        DEBUG_PRINTLN(distToEdgeOfCurrentTile);

    } else {
        DEBUG_PRINT(F("______1 2 3 4 5\nWT CL "));
        DEBUG_PRINT((uint8_t)wallTile1_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile2_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile3_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile4_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile5_CurrLvl);
        DEBUG_PRINT(F("\nWT NL _ "));
        DEBUG_PRINT((uint8_t)wallTile2_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile3_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile4_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile5_NextLvl);
        DEBUG_PRINT(F("\nGT CL _ "));
        DEBUG_PRINT((uint8_t)isGroundTile2_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile3_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile4_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile5_CurrLvl);
        DEBUG_PRINT(F("\nGT NL _ "));
        DEBUG_PRINT((uint8_t)isGroundTile2_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile3_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile4_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile5_NextLvl);
        DEBUG_PRINT("\nDist ");
        DEBUG_PRINTLN(distToEdgeOfCurrentTile);
    }

#endif

    /* ----------------------------------------------------------------------------------- */
    /*  Jump to 4th position / land normally or grab ledge ..
    
    Left                   Right
    _____ 6 5 4 3 2 1      _____ 1 2 3 4 5 6
    WT CL x x x 0 0 x      WT CL x 0 0 x x x
    WT NL x x 1 0 x x      WT CL x x 0 1 x x
    GT CL x x x x x _      GT CL _ x x x x x
    GT NL x x x x x _      GT NL _ x x x x x 
    */

    //if (wallTile1_CurrLvl == WallTileResults::None &&   removed as curent tile might be a gate
    if(wallTile2_CurrLvl == WallTileResults::None && wallTile3_CurrLvl == WallTileResults::None &&
       !isGroundTile3_CurrLvl && isGroundTile4_CurrLvl) {
        switch(distToEdgeOfCurrentTile) {
        case 2:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R1 Normal_36"));
#endif

            return StandingJumpResult::Normal_36;

        case 6:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R2 GrabLedge_28"));
#endif

            return StandingJumpResult::GrabLedge_28;

        case 10:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R3 GrabLedge_32"));
#endif

            return StandingJumpResult::GrabLedge_32;
        }
    }

    /* ----------------------------------------------------------------------------------- */
    /*  Jump to 5th position and grab wall tile ..
    
    Left                   Right
    _____ 6 5 4 3 2 1      _____ 1 2 3 4 5 6
    WT CL x 0 0 0 0 x      WT CL x 0 0 0 0 x
    WT NL x 1 0 0 0 x      WT CL x 0 0 0 1 x
    GT CL x x 0 0 x _      GT CL _ x 0 0 x x
    GT NL x x x x x _      GT NL _ x x x x x 
    */

    if((distToEdgeOfCurrentTile <= 6) && wallTile2_CurrLvl == WallTileResults::None &&
       wallTile3_CurrLvl == WallTileResults::None && wallTile4_CurrLvl == WallTileResults::None &&
       wallTile5_CurrLvl == WallTileResults::None && wallTile2_NextLvl == WallTileResults::None &&
       wallTile3_NextLvl == WallTileResults::None && wallTile4_NextLvl == WallTileResults::None &&
       wallTile5_NextLvl != WallTileResults::None && !isGroundTile3_CurrLvl &&
       !isGroundTile4_CurrLvl) {
        if(distToEdgeOfCurrentTile == 2) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R4 GrabLedge_36"));
#endif

            return StandingJumpResult::GrabLedge_36;
        } else {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R5 GrabLedge_40"));
#endif

            return StandingJumpResult::GrabLedge_40;
        }
    }

    /* ----------------------------------------------------------------------------------- */
    /*  Jump to 5th position and grab ground tile ..
    
    Left                   Right
    _____ 6 5 4 3 2 1      _____ 1 2 3 4 5 6
    WT CL x 0 0 0 0 x      WT CL x 0 0 0 0 x
    WT NL x 0 0 0 0 x      WT CL x 0 0 0 0 x
    GT CL x 1 0 0 x _      GT CL _ x 0 0 1 x
    GT NL x x x x x _      GT NL _ x x x x x 
    */

    if((distToEdgeOfCurrentTile <= 6) && wallTile2_CurrLvl == WallTileResults::None &&
       wallTile3_CurrLvl == WallTileResults::None && wallTile4_CurrLvl == WallTileResults::None &&
       wallTile5_CurrLvl == WallTileResults::None && wallTile2_NextLvl == WallTileResults::None &&
       wallTile3_NextLvl == WallTileResults::None && wallTile4_NextLvl == WallTileResults::None &&
       !isGroundTile3_CurrLvl && !isGroundTile4_CurrLvl && isGroundTile5_CurrLvl) {
        if(distToEdgeOfCurrentTile == 2) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R6 GrabLedge_36"));
#endif

            return StandingJumpResult::GrabLedge_36;
        } else {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R7 GrabLedge_40"));
#endif

            return StandingJumpResult::GrabLedge_40;
        }
    }

    /* ----------------------------------------------------------------------------------- */
    /*  Jump and drop a level?
    
    Left                   Right
    _____ 6 5 4 3 2 1      _____ 1 2 3 4 5 6
    WT CL x 1 0 0 0 x      WT CL x 0 0 0 1 x
    WT NL x 0 0 x x x      WT CL x x x 0 0 x
    GT CL x x 0 0 x _      GT CL _ x 0 0 x x
    GT NL x x x x x _      GT NL _ x x x x x 
    */

    if(distToEdgeOfCurrentTile == 2 && wallTile2_CurrLvl == WallTileResults::None &&
       wallTile3_CurrLvl == WallTileResults::None && wallTile4_CurrLvl == WallTileResults::None &&
       wallTile5_CurrLvl != WallTileResults::None && wallTile4_NextLvl == WallTileResults::None &&
       wallTile5_NextLvl == WallTileResults::None && !isGroundTile3_CurrLvl &&
       !isGroundTile4_CurrLvl) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
        DEBUG_PRINTLN(F("R8 DropLevel_40"));
#endif

        return StandingJumpResult::DropLevel_40;
    }

    /* ----------------------------------------------------------------------------------- */
    /*  Jump to Position 2?
    
    Left                   Right
    _____ 6 5 4 3 2 1      _____ 1 2 3 4 5 6
    WT CL x x x 0 0 x      WT CL x 0 0 x x x
    WT NL x x x 1 0 x      WT CL x 0 1 x x x
    GT CL x x x x x _      GT CL _ x x x x x
    GT NL x x x x x _      GT NL _ x x x x x 
    */

    if(wallTile2_CurrLvl == WallTileResults::None && wallTile3_CurrLvl == WallTileResults::None &&
       wallTile2_NextLvl == WallTileResults::None && wallTile3_NextLvl != WallTileResults::None) {
        switch(distToEdgeOfCurrentTile) {
        case 2:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R9 Normal_24"));
#endif

            return StandingJumpResult::Normal_24;

        case 6:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R10 Normal_28"));
#endif

            return StandingJumpResult::Normal_28; // Short_Pos6

        case 10:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("R11 Normal_28"));
#endif

            return StandingJumpResult::Normal_32;
        }
    }

    /* ----------------------------------------------------------------------------------- */
    /*  Jump to Position 2?
    
    Left                   Right
    _____ 6 5 4 3 2 1      _____ 1 2 3 4 5 6
    WT CL x x 1 0 0 x      WT CL x 0 0 1 x x
    WT NL x x x 1 0 x      WT CL x 0 1 x x x
    GT CL x x x x x _      GT CL _ x x x x x
    GT NL x x x x x _      GT NL _ x x x x x 
    */

    if(wallTile2_CurrLvl == WallTileResults::None && wallTile3_CurrLvl == WallTileResults::None &&
       wallTile4_CurrLvl != WallTileResults::None && isGroundTile3_CurrLvl) {
        switch(distToEdgeOfCurrentTile) {
        case 2:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("J1 Normal_24"));
#endif

            return StandingJumpResult::Normal_24;

        case 6:
        case 10:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("J3 Normal_28"));
#endif

            return StandingJumpResult::Normal_28;
        }
    }

    /* ----------------------------------------------------------------------------------- */
    /*  Jump to position 3 and drop a level?
    
    Left                   Right
    _____ 6 5 4 3 2 1      _____ 1 2 3 4 5 6
    WT CL x x x 0 0 x      WT CL x 0 0 x x x
    WT NL x x 0 0 0 x      WT CL x 0 0 0 x x
    GT CL x x 0 0 x _      GT CL _ x 0 0 x x
    GT NL x x 1 x x _      GT NL _ x x 1 x x 

    */

    if(wallTile2_CurrLvl == WallTileResults::None && wallTile3_CurrLvl == WallTileResults::None &&
       wallTile2_NextLvl == WallTileResults::None && wallTile3_NextLvl == WallTileResults::None &&
       wallTile4_NextLvl == WallTileResults::None && !isGroundTile2_CurrLvl &&
       !isGroundTile3_CurrLvl && isGroundTile4_NextLvl) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
        DEBUG_PRINTLN(F("L12 DropLevel_36"));
#endif

        return StandingJumpResult::DropLevel_36;

    } else {
        if(wallTile2_CurrLvl == WallTileResults::None) {
            switch(wallTile3_CurrLvl) {
            case WallTileResults::None:

                if(wallTile4_CurrLvl != WallTileResults::None) {
                    switch(distToEdgeOfCurrentTile) {
                    case 2:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
                        DEBUG_PRINTLN(F("L14 Normal_20"));
#endif

                        return StandingJumpResult::Normal_20;

                    case 6:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
                        DEBUG_PRINTLN(F("L15 Normal_24"));
#endif

                        return StandingJumpResult::Normal_24;

                    case 10:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
                        DEBUG_PRINTLN(F("L16 Normal_28"));
#endif

                        return StandingJumpResult::Normal_28;

                    default:
                        break;
                    }
                }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
                DEBUG_PRINTLN(F("L17 Normal_36"));
#endif

                return StandingJumpResult::Normal_36;

            case WallTileResults::SolidWall:
            case WallTileResults::GateClosed:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
                DEBUG_PRINTLN(F("L18 None"));
#endif

                return StandingJumpResult::None;
            }

        } else {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
            DEBUG_PRINTLN(F("L19 None"));
#endif

            return StandingJumpResult::None;
        }
    }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANSTANDINGJUMP) && \
    defined(DEBUG_ACTION_CANSTANDINGJUMP_DETAIL)
    DEBUG_PRINTLN(F("L20 Normal_36"));
#endif

    return StandingJumpResult::Normal_36;
}

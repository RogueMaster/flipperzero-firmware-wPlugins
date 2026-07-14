RunningJumpResult canRunningJump(Prince& prince, Action action) {
    int8_t tileXIdx = this->coordToTileIndexX(prince.getPosition().x) - this->getXLocation();
    int8_t tileYIdx = this->coordToTileIndexY(prince.getPosition().y) - this->getYLocation();
    int8_t offset = prince.getDirection() == Direction::Left ? -1 : 1;

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP)
    DEBUG_PRINTLN(F("------------------------------"));
    printAction(action);
    DEBUG_PRINT(F(" coordToTileIndexX "));
    DEBUG_PRINT(prince.getPosition().x);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINT(tileXIdx);
    DEBUG_PRINT(F(", coordToTileIndexY "));
    DEBUG_PRINT(prince.getPosition().y);
    DEBUG_PRINT(F(" = "));
    DEBUG_PRINTLN(tileYIdx);
#endif

    int8_t distToEdgeOfCurrentTile =
        distToEdgeOfTile(prince.getDirection(), prince.getPosition().x);

    WallTileResults wallTile2_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (1 * offset), tileYIdx, prince.getDirection(), true, 8);
    WallTileResults wallTile3_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (2 * offset), tileYIdx, prince.getDirection(), true, 6);
    WallTileResults wallTile4_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (3 * offset), tileYIdx, prince.getDirection(), true, 4);
    WallTileResults wallTile5_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (4 * offset), tileYIdx, prince.getDirection(), true, 2);
    WallTileResults wallTile6_CurrLvl = this->isWallTile_ByCoords(
        tileXIdx + (5 * offset), tileYIdx, prince.getDirection(), true, 0);

    WallTileResults wallTile2_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (1 * offset), tileYIdx + 1, prince.getDirection(), true, 8);
    WallTileResults wallTile3_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (2 * offset), tileYIdx + 1, prince.getDirection(), true, 6);
    WallTileResults wallTile4_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (3 * offset), tileYIdx + 1, prince.getDirection(), true, 4);
    WallTileResults wallTile5_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (4 * offset), tileYIdx + 1, prince.getDirection(), true, 2);
    WallTileResults wallTile6_NextLvl = this->isWallTile_ByCoords(
        tileXIdx + (5 * offset), tileYIdx + 1, prince.getDirection(), true, 0);

    bool isGroundTile2_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (1 * offset), tileYIdx);
    bool isGroundTile3_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (2 * offset), tileYIdx);
    bool isGroundTile4_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (3 * offset), tileYIdx);
    bool isGroundTile5_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (4 * offset), tileYIdx);
    bool isGroundTile6_CurrLvl = this->isGroundTile_ByCoords(tileXIdx + (5 * offset), tileYIdx);
    bool isGroundTile2_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (1 * offset), tileYIdx + 1);
    bool isGroundTile3_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (2 * offset), tileYIdx + 1);
    bool isGroundTile4_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (3 * offset), tileYIdx + 1);
    bool isGroundTile5_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (4 * offset), tileYIdx + 1);
    bool isGroundTile6_NextLvl =
        this->isGroundTile_ByCoords(tileXIdx + (5 * offset), tileYIdx + 1);

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP)

    WallTileResults wallTile1_CurrLvl =
        this->isWallTile_ByCoords(tileXIdx, tileYIdx, prince.getDirection());
    WallTileResults wallTile1_NextLvl =
        this->isWallTile_ByCoords(tileXIdx, tileYIdx + 1, prince.getDirection());
    bool isGroundTile1_CurrLvl = this->isGroundTile_ByCoords(tileXIdx, tileYIdx);
    bool isGroundTile1_NextLvl = this->isGroundTile_ByCoords(tileXIdx, tileYIdx + 1);

    printAction(action);

    if(prince.getDirection() == Direction::Left) {
        DEBUG_PRINT(F("\n______6 5 4 3 2 1\nWT CL "));
        DEBUG_PRINT((uint8_t)wallTile6_CurrLvl);
        DEBUG_PRINT(" ");
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
        DEBUG_PRINT((uint8_t)wallTile6_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile5_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile4_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile3_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile2_NextLvl);
        DEBUG_PRINT(F(" _\nGT CL "));
        DEBUG_PRINT((uint8_t)isGroundTile6_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile5_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile4_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile3_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile2_CurrLvl);
        DEBUG_PRINT(F(" _\nGT NL "));
        DEBUG_PRINT((uint8_t)isGroundTile6_NextLvl);
        DEBUG_PRINT(" ");
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
        DEBUG_PRINT(F("\n______1 2 3 4 5 6\nWT CL "));
        DEBUG_PRINT((uint8_t)wallTile1_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile2_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile3_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile4_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile5_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile6_CurrLvl);
        DEBUG_PRINT(F("\nWT NL _ "));
        DEBUG_PRINT((uint8_t)wallTile2_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile3_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile4_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile5_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)wallTile6_NextLvl);
        DEBUG_PRINT(F("\nGT CL _ "));
        DEBUG_PRINT((uint8_t)isGroundTile2_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile3_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile4_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile5_CurrLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile6_CurrLvl);
        DEBUG_PRINT(F("\nGT NL _ "));
        DEBUG_PRINT((uint8_t)isGroundTile2_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile3_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile4_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile5_NextLvl);
        DEBUG_PRINT(" ");
        DEBUG_PRINT((uint8_t)isGroundTile6_NextLvl);
        DEBUG_PRINT("\nDist ");
        DEBUG_PRINTLN(distToEdgeOfCurrentTile);
    }

#endif

    /* ----------------------------------------------------------------------------------- */
    /* 4. Can we jump four blanks to the same level? Results in a player hanging on.
    
    Positions 2, 6, 10.
    
    Left                  Right
    _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
    WT CL 0 0 0 0 0 0     WT CL 0 0 0 0 0 0
    WT NL x x x x x x     WT NL x x x x x x
    GT CL 1 0 0 0 x _     GT CL _ x 0 0 0 1
    GT NL x x x x x _     GT NL _ x x x x x
    */

    if(wallTile2_CurrLvl == WallTileResults::None && wallTile3_CurrLvl == WallTileResults::None &&
       wallTile4_CurrLvl == WallTileResults::None && wallTile5_CurrLvl == WallTileResults::None &&
       (wallTile6_CurrLvl == WallTileResults::None || action == Action::RunJump_Level6Exit) &&
       !isGroundTile3_CurrLvl && !isGroundTile4_CurrLvl && !isGroundTile5_CurrLvl &&
       isGroundTile6_CurrLvl) {
        switch(distToEdgeOfCurrentTile) {
        case 0 ... 3:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_4)
            DEBUG_PRINTLN(F("J4-1 Jump4_GrabLedge_Pos2"));
#endif

            return RunningJumpResult::Jump4_GrabLedge_Pos2;

        case 4 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_4)
            DEBUG_PRINTLN(F("J4-2 Jump4_GrabLedge_Pos6"));
#endif

            return RunningJumpResult::Jump4_GrabLedge_Pos6;

        default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_4)
            DEBUG_PRINTLN(F("J4-3 Jump4_GrabLedge_Pos10"));
#endif

            return RunningJumpResult::Jump4_GrabLedge_Pos10;
        }
    }

    if(action == Action::RunJump_Normal) {
        /* ----------------------------------------------------------------------------------- */
        /* 4.  Can we jump four levels to a lower level ?
        
        Positions 2, 6, 10.
        
        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL 1 0 0 0 0 0     WT CL 0 0 0 0 0 1
        WT NL 0 0 0 x x x     WT NL x x x 0 0 0
        GT CL 0 0 0 x x _     GT CL _ x x 0 0 0
        GT NL 1 0 0 x x _     GT NL _ x x 0 0 1
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl == WallTileResults::None &&
           wallTile6_CurrLvl != WallTileResults::None &&
           wallTile4_NextLvl == WallTileResults::None &&
           wallTile5_NextLvl == WallTileResults::None &&
           wallTile6_NextLvl == WallTileResults::None && !isGroundTile4_CurrLvl &&
           !isGroundTile5_CurrLvl && !isGroundTile6_CurrLvl && !isGroundTile4_NextLvl &&
           !isGroundTile5_NextLvl && isGroundTile6_NextLvl) {
            switch(distToEdgeOfCurrentTile) {
            case 0 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_4)
                DEBUG_PRINTLN(F("J4-4 Jump4_DropLevel"));
#endif

                return RunningJumpResult::Jump4_DropLevel;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_4)
                DEBUG_PRINTLN(F("J4-5 Jump4_DropLevel_Pos10"));
#endif

                return RunningJumpResult::Jump4_DropLevel_Pos10;
            }
        }

        /* ----------------------------------------------------------------------------------- */
        /*  3. Fix for #112.
        
        Positions 2, 6, 10.

        
        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL x 0 0 0 0 0     WT CL 0 0 0 0 0 x
        WT NL x 1 0 x x x     WT NL x x x 0 1 x
        GT CL x 1 0 x 1 _     GT CL _ 1 x 0 1 x
        GT NL x x x x x _     GT NL _ x x x x x
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl == WallTileResults::None &&
           wallTile2_NextLvl != WallTileResults::None &&
           wallTile4_NextLvl == WallTileResults::None &&
           wallTile5_NextLvl != WallTileResults::None && isGroundTile2_CurrLvl &&
           !isGroundTile4_CurrLvl && isGroundTile5_CurrLvl) {
            switch(distToEdgeOfCurrentTile) {
            case 0 ... 3:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-1 Jump3_GrabLedge_Pos2"));
#endif

                return RunningJumpResult::Jump3_GrabLedge_Pos2;

            case 4 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-2 Jump3_GrabLedge_Pos6"));
#endif

                return RunningJumpResult::Jump3_GrabLedge_Pos6;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-3 Normal_Pos10"));
#endif

                return RunningJumpResult::Jump3_GrabLedge_Pos10;
            }
        }

        /* ----------------------------------------------------------------------------------- */
        /*  3. Can we jump three blanks to the same level? Unique case where there first tile
               is a floor tile.
        
        Positions 2, 6, 10.

        
        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL 1 0 0 0 0 0     WT CL 0 0 0 0 0 1
        WT NL 1 1 0 0 x x     WT NL x x 0 0 1 1
        GT CL x 1 x x x _     GT CL _ x x x 1 x
        GT NL x x x x x _     GT NL _ x x x x x
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl == WallTileResults::None &&
           wallTile6_CurrLvl != WallTileResults::None &&
           wallTile3_NextLvl == WallTileResults::None &&
           wallTile4_NextLvl == WallTileResults::None &&
           wallTile5_NextLvl != WallTileResults::None &&
           wallTile6_NextLvl != WallTileResults::None && !isGroundTile3_CurrLvl &&
           !isGroundTile4_CurrLvl && isGroundTile5_CurrLvl && distToEdgeOfCurrentTile == 2) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
            DEBUG_PRINTLN(F("J3-3 Jump3_Pos6"));
#endif

            return RunningJumpResult::Jump3_Pos6;
        }

        /* ----------------------------------------------------------------------------------- */
        /*  3. Can we jump three blanks to the same level? Unique case where there first tile
               is a floor tile.
        
                Positions 2, 6, 10.

        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL x 0 0 0 0 0     WT CL 0 0 0 0 0 x
        WT NL x 1 0 0 1 x     WT NL x 1 0 0 1 x
        GT CL x 1 x x 1 _     GT CL _ 1 x x 1 x
        GT NL x x x x x _     GT NL _ x x x x x
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl == WallTileResults::None &&
           wallTile2_NextLvl != WallTileResults::None &&
           wallTile3_NextLvl == WallTileResults::None &&
           wallTile4_NextLvl == WallTileResults::None &&
           wallTile5_NextLvl != WallTileResults::None && isGroundTile2_CurrLvl &&
           !isGroundTile3_CurrLvl && !isGroundTile4_CurrLvl && isGroundTile5_CurrLvl &&
           distToEdgeOfCurrentTile == 2) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
            DEBUG_PRINTLN(F("J3-4 Jump3_Pos10"));
#endif

            return RunningJumpResult::Jump3_Pos10;
        }

        /* ----------------------------------------------------------------------------------- */
        /*  3. Can we jump three blanks to the same level? Level 8 issue.
        
                Positions 2, 6, 10.

        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL 1 0 0 0 0 0     WT CL 0 0 0 0 0 1
        WT NL x x x x x x     WT NL x x x x x x
        GT CL x 1 0 0 0 _     GT CL _ 0 0 0 1 x
        GT NL x x x x x _     GT NL _ x x x x x
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl == WallTileResults::None &&
           wallTile6_CurrLvl != WallTileResults::None && !isGroundTile2_CurrLvl &&
           !isGroundTile3_CurrLvl && !isGroundTile4_CurrLvl && isGroundTile5_CurrLvl &&
           distToEdgeOfCurrentTile == 2) {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
            DEBUG_PRINTLN(F("J3-5 Jump3_Pos6"));
#endif

            return RunningJumpResult::Jump3_Pos6;
        }

        /* ----------------------------------------------------------------------------------- */
        /*  3. Can we jump three blanks to the same level?
        
        Positions 2, 6, 10.
        
        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL x 0 0 0 0 0     WT CL 0 0 0 0 0 x
        WT NL x x x x x x     WT NL x x x x x x
        GT CL x 1 0 x x _     GT CL _ x x 0 1 x
        GT NL x x x x x _     GT NL _ x x x x x
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl == WallTileResults::None && isGroundTile5_CurrLvl &&
           !isGroundTile4_CurrLvl) {
            switch(distToEdgeOfCurrentTile) {
            case 0 ... 3:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-6 Jump3_Pos2"));
#endif

                return RunningJumpResult::Jump3_Pos2;

            case 4 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-7 Jump3_Pos6"));
#endif

                return RunningJumpResult::Jump3_Pos6;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-8 Jump3_Pos10"));
#endif

                return RunningJumpResult::Jump3_Pos10;
            }
        }

        /* ----------------------------------------------------------------------------------- */
        /*  3. Can we jump three blanks to the same level?
        
        Positions 2, 6, 10.
        
        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL x 0 0 0 0 0     WT CL 0 0 0 0 0 x
        WT NL x x x x x x     WT NL x x x x x x
        GT CL x 1 1 x x _     GT CL _ x x 1 1 x
        GT NL x x x x x _     GT NL _ x x x x x
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl == WallTileResults::None && isGroundTile5_CurrLvl &&
           isGroundTile4_CurrLvl) {
            switch(distToEdgeOfCurrentTile) {
            case 0 ... 3:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-9 Jump3_Pos2"));
#endif

                return RunningJumpResult::Jump3_Pos2;

            case 4 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-10 Jump3_Pos6"));
#endif

                return RunningJumpResult::Jump3_Pos6;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-11 Jump3_Pos10"));
#endif

                return RunningJumpResult::Jump3_Pos10;
            }
        }

        /* ----------------------------------------------------------------------------------- */
        /*  Can we jump three blanks to a lower level?  
        
        Positions 2, 6, 10.
        
        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL x 1 0 0 0 0     WT CL 0 0 0 0 1 x
        WT NL x 0 0 0 0 x     WT NL x 0 0 0 0 x
        GT CL x x x x x _     GT CL _ x x x x x
        GT NL x 1 x x x _     GT NL _ x x x 1 x
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl != WallTileResults::None &&
           wallTile2_NextLvl == WallTileResults::None &&
           wallTile3_NextLvl == WallTileResults::None &&
           wallTile4_NextLvl == WallTileResults::None &&
           wallTile5_NextLvl == WallTileResults::None && !isGroundTile2_CurrLvl &&
           !isGroundTile3_CurrLvl && !isGroundTile4_CurrLvl && !isGroundTile2_NextLvl &&
           !isGroundTile3_NextLvl && !isGroundTile4_NextLvl && isGroundTile5_NextLvl) {
            switch(distToEdgeOfCurrentTile) {
            case 0 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-12 Jump3_DropLevel"));
#endif

                return RunningJumpResult::Jump3_DropLevel;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-13 Jump3_DropLevel"));
#endif

                return RunningJumpResult::Jump3_DropLevel_Pos10;
            }
        }

        /* ----------------------------------------------------------------------------------- */
        /*  3. Three levels to same floor (might be nothing to land on) ..
        
        Positions 2, 6, 10.
        
        Left                  Right
        _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
        WT CL x 0 0 0 0 0     WT CL 0 0 0 0 0 x
        WT NL x x x x x x     WT NL x x x x x x
        GT CL x x x x x _     GT CL _ x x x x x
        GT NL x x x x x _     GT NL _ x x x x x
        */

        if(wallTile2_CurrLvl == WallTileResults::None &&
           wallTile3_CurrLvl == WallTileResults::None &&
           wallTile4_CurrLvl == WallTileResults::None &&
           wallTile5_CurrLvl == WallTileResults::None) {
            switch(distToEdgeOfCurrentTile) {
            case 0 ... 3:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-14 Normal_Pos2"));
#endif

                return RunningJumpResult::Normal_Pos2;

            default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_3)
                DEBUG_PRINTLN(F("J3-15 Normal_Pos6"));
#endif

                return RunningJumpResult::Normal_Pos6;
            }
        }
    }

    /* ----------------------------------------------------------------------------------- */
    /*  2. Basic jump ..
    
    Positions 2, 6, 10.
    
    Left                  Right
    _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
    WT CL x x 0 0 0 x     WT CL x 0 0 0 x x
    WT NL x x x x x x     WT NL x x x x x x
    GT CL x x 1 x x _     GT CL _ x x 1 x x
    GT NL x x x x x _     GT NL _ x x x x x
    */

    if(wallTile2_CurrLvl == WallTileResults::None && wallTile3_CurrLvl == WallTileResults::None &&
       wallTile4_CurrLvl == WallTileResults::None && isGroundTile4_CurrLvl) {
        switch(distToEdgeOfCurrentTile) {
        case 0 ... 3:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J2-1 Jump2_Pos2"));
#endif

            return RunningJumpResult::Jump2_Pos2;

        case 4 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J2-2 Jump2_Pos6"));
#endif

            return RunningJumpResult::Jump2_Pos6;

        default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J2-3 Jump2_Pos10"));
#endif

            return RunningJumpResult::Jump2_Pos10;
        }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
        DEBUG_PRINTLN(F("J2-4 Jump6_Pos6"));
#endif

        return RunningJumpResult::Jump2_Pos6;
    }

    /* ----------------------------------------------------------------------------------- */
    /*  2. Basic jump and drop ..
    
    Positions 2, 6, 10.
    
    Left                  Right
    _____ 6 5 4 3 2 1     _____ 1 2 3 4 5 6
    WT CL x x 0 0 0 x     WT CL x 0 0 0 x x
    WT NL x x 0 0 x x     WT NL x x 0 0 x x
    GT CL x x 0 0 x _     GT CL _ x 0 0 x x
    GT NL x x 1 x x _     GT NL _ x x 1 x x
    */

    if(wallTile2_CurrLvl == WallTileResults::None && wallTile3_CurrLvl == WallTileResults::None &&
       wallTile4_CurrLvl == WallTileResults::None && wallTile3_NextLvl == WallTileResults::None &&
       wallTile4_NextLvl == WallTileResults::None && !isGroundTile4_CurrLvl &&
       isGroundTile4_NextLvl) {
        switch(distToEdgeOfCurrentTile) {
        case 0 ... 4:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J2-5 Jump2_DropLevel_Pos2"));
#endif

            return RunningJumpResult::Jump2_DropLevel_Pos2;

        case 5 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J2-6 Jump2_DropLevel_Pos6"));
#endif

            return RunningJumpResult::Jump2_DropLevel_Pos6;

        default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J2-7 Jump2_DropLevel_Pos10"));
#endif

            return RunningJumpResult::Jump2_DropLevel_Pos10;
        }
    }

    /* ----------------------------------------------------------------------------------- */
    /* 1. Can we jump straight ahead?
    
    Left                   Right
    _____ 6 5 4 3 2 1      _____ 1 2 3 4 5 6
    WT CL x x x 0 0 0      WT CL 0 0 0 x x x
    WT NL x x x x x x      WT CL x x x x x x
    GT CL x x x x x _      GT CL _ x x x x x
    GT NL x x x x x _      GT NL _ x x x x x 
    */

    if(wallTile2_CurrLvl == WallTileResults::None && wallTile3_CurrLvl == WallTileResults::None) {
        switch(distToEdgeOfCurrentTile) {
        case 0 ... 3:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J1-1 Jump1_Pos2"));
#endif

            return RunningJumpResult::Jump1_Pos2;

        case 4 ... 7:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J1-2 Jump1_Pos6"));
#endif

            return RunningJumpResult::Jump1_Pos6;

        default:

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_2)
            DEBUG_PRINTLN(F("J1-3 Jump1_Pos10"));
#endif

            return RunningJumpResult::Jump1_Pos10;
        }

#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_1)
        DEBUG_PRINTLN(F("J1-4 None"));
#endif

        return RunningJumpResult::None;

    } else {
#if defined(DEBUG) && defined(DEBUG_ACTION_CANRUNNINGJUMP) && \
    defined(DEBUG_ACTION_CANRUNNINGJUMP_DETAIL) && defined(DEBUG_ACTION_CANRUNNINGJUMP_1)
        DEBUG_PRINTLN(F("J1-5 None"));
#endif

        return RunningJumpResult::None;
    }
}


#pragma once

enum class ItemType : uint8_t {
    /* 00 */ InteractiveItemType_Start,
    /* 00 */ AppearingFloor = 0,
    /* 01 */ CollapsingFloor,
    /* 02 */ FloorButton1,
    /* 03 */ FloorButton2,
    /* 04 */ Spikes,
    /* 05 */ ExitDoor_Button,
    /* 06 */ ExitDoor_Button_Cropped,
    /* 07 */ FloorButton_NoEdgeTile,
    /* 08 */ FloorButton3_UpOnly, // << needed?
    /* 09 */ FloorButton3_DownOnly, // << needed?
    /* 10 */ Mirror_Button,
    /* 11 */ FloorButton4,
    /* __ */ InteractiveItemType_End = FloorButton4,
    /* 15 */ Skeleton = 15,
    /* 16 */ ExitDoor_SelfOpen,
    /* 17 */ Gate,
    /* 18 */ Gate_StayOpen,
    /* 19 */ CollpasedFloor,
    /* 20 */ Potion_Small,
    /* 21 */ Potion_Large,
    /* 22 */ Potion_Poison,
    /* 23 */ Potion_Float,
    /* 24 */ EntryDoor_Cropped,
    /* 25 */ Blade,
    /* 26 */ ExitDoor_ButtonOpen,
    /* 27 */ Sword,
    /* 28 */ EntryDoor_HalfTileLeft,
    /* 29 */ Mirror,
    /* 30 */ Gate_StayClosed,
    /* 31 */ EntryDoor,
    /* 32 */ DecorativeDoor,
    /* 50 */ General = 50,
    /* 51 */ Invader_1,
    /* 52 */ Invader_2,
    /* 53 */ Invader_3,
    /* 54 */ Player,
    /* 55 */ Barrier,
    /* 56 */ Bullet,
    /* 96 */ None = 96,
    /* 97 */ LoveHeart = 97,
    /* 98 */ Sign = 98,
    /* 99 */ Flash = 99,
    /* 99 */ AllItemTypes_End = Flash,
};

enum GameState : uint8_t {
    SplashScreen_Init,
    SplashScreen,
    Title_Init,
    Title,
    Game_Init,
    Game,
    Game_StartLevel,
#ifndef SAVE_MEMORY_OTHER
    Menu,
#endif
};

enum class Direction : uint8_t {
    None,
    Left,
    Right,
    Up,
    Down,
    Forward,
    Back,
};

enum class Layer : uint8_t {
    Background,
    Foreground,
};

enum class Action : uint8_t {
    Step = 0,
    SmallStep = 1,
    RunStart = 2,
    RunRepeat = 3,
    SwordStep = 4, // Do not move from 4
    RunJump_Normal = 5,
    RunJump_Level6Exit = 6,
    StandingJump = 7,
    SwordStep2 = 8, // Do not move from 8
    CrouchHop = 9,
    RunningTurn = 10,
};

enum class CanJumpUpResult : uint8_t {
    None,
    Jump,
    StepThenJump,
    JumpThenFall,
    JumpThenFall_HideHands,
    TurnThenJump,
    JumpDist10,
    JumpThenFall_CollapseFloor,
    StepThenJumpThenFall_CollapseFloor,
    JumpThenFall_CollapseFloorAbove,
};

enum class CanClimbDownResult : uint8_t {
    None,
    ClimbDown,
    StepThenClimbDown,
    TurnThenClimbDown,
    StepThenTurnThenClimbDown,
};

enum class CanClimbDownPart2Result : uint8_t {
    None,
    Level_1,
    Falling,
    Level_1_Under,
};

enum class StandingJumpResult : uint8_t {
    None,
    Normal_20,
    Normal_24,
    Normal_28,
    Normal_32,
    Normal_36,
    DropLevel_36,
    DropLevel_40,
    GrabLedge_28,
    GrabLedge_32,
    GrabLedge_36,
    GrabLedge_40,
};

enum class RunningJumpResult : uint8_t {
    None,
    Normal_Pos2,
    Normal_Pos6,
    Jump4_GrabLedge_Pos2,
    Jump4_GrabLedge_Pos6,
    Jump4_GrabLedge_Pos10,
    Jump4_DropLevel,
    Jump4_DropLevel_Pos10,
    Jump3_Pos2,
    Jump3_Pos6,
    Jump3_Pos10,
    Jump3_DropLevel,
    Jump3_DropLevel_Pos10,
    Jump2_Pos2,
    Jump2_Pos6,
    Jump2_DropLevel_Pos6,
    Jump2_DropLevel_Pos10,
    Jump2_Pos10,
    Jump1_Pos2,
    Jump1_Pos6,
    Jump1_Pos10,
    Jump3_GrabLedge_Pos2,
    Jump3_GrabLedge_Pos6,
    Jump3_GrabLedge_Pos10,
    Jump2_DropLevel_Pos2,
    KeepRunning,
};

enum class MenuOption : uint8_t {
    Resume = 0,
    Load = 1,
    Save = 2,
    Sound = 3,
    MainMenu = 4,
    Clear = 5,
    Sound_On = 6,
    Sound_Off = 7,
    Clear_Yes = 8,
    Clear_No = 9,
};

enum class TitleScreenOptions : uint8_t {
    Play,
    Resume,
    Credits,
    High,
};

enum class TitleScreenMode : uint8_t {
    Intro, // 0
    Main, // 1
    Credits, // 2
    High, // 3
    IntroGame_1A, // 4
    CutScene_1, // 5
    IntroGame_1B, // 6
    CutScene_2, // 7
    CutScene_3, // 8
    CutScene_4, // 9
    CutScene_5, // 10
    CutScene_6, // 11
    CutScene_8, // 12
    CutScene_7_Hint, // 13

#ifndef SAVE_MEMORY_INVADER
    CutScene_7_Transition, // 13
    CutScene_7_PlayGame, // 14
#endif

    CutScene_End, // 16
    IntroGame_End, // 17
    TimeOut, // 18
    MaxUniqueScenes = CutScene_End - 1
};

enum class WallTileResults : uint8_t {
    None,
    SolidWall,
    GateClosed,
};

enum class SignType : uint8_t {
    PressA,
    GameOver,
};

enum class FlashType : uint8_t {
    None,
    SwordFight,
    MirrorLevel12,
};

enum class LevelUpdate : uint8_t {
    NoAction,
    FloorCollapsedOnPrince,
};

enum class Status : uint8_t {
    Active,
    Safe,
    Dormant,
    Dormant_ActionReady,
    Dormant_ActionDone,
    Exploding1,
    Exploding2,
    Exploding3,
    Exploding4,
    Dead,
    EnemiesAppearing,
};

enum class EnemyType : uint8_t {
    Guard,
    Skeleton,
    Mirror,
    MirrorAttackingL12,
    MirrorAfterChallengeL12,
    None = 255
};

enum class DeathType : uint8_t {
    Falling,
    Blade,
    SwordFight,
    Spikes,
};

enum class CanFallResult : uint8_t {
    CanFall,
    CannotFall,
    CanFallToHangingPosition,
};

enum class GateType : uint8_t {
    Normal,
    Level6Exit,
};

enum class TitleFrameIndex : uint8_t {
    Credits_PoP,
    High_PoP_Frame,
    Main_PoP_Frame_NC,
    Main_PoP_Frame_NCH,
    Main_PoP_Frame_NRC,
    Main_PoP_Frame_NRCH,
    Intro_Last_PoP_Frame_NC,
    Intro_Last_PoP_Frame_NCH,
    Intro_Last_PoP_Frame_NRC,
    Intro_Last_PoP_Frame_NRCH,
    Main_Game_PoP_Frame_NC,
    Main_Game_PoP_Frame_NCH,
    Main_Game_PoP_Frame_NRC,
    Main_Game_PoP_Frame_NRCH,
    IntroGame_End_PoP_Frame,
    Intro_PoP_Frame_NC,
    Intro_PoP_Frame_NCH,
    Intro_PoP_Frame_NRC,
    Intro_PoP_Frame_NRCH,
    TimeOut_PoP_Frame,
    IntroGame_1A_Frame,
    CutScene_1_Frame,
    IntroGame_1B_Frame,
    CutScene_2_Frame,
    CutScene_3_Frame,
    CutScene_4_Frame,
    CutScene_5_Frame,
    CutScene_6_Frame,
    CutScene_2B_Frame,
    CutScene_7_Frame,
    CutScene_8_Frame,
    CutScene_End_Frame
};

enum class SoundIndex : uint8_t {
    Override_Start = 0,
    Dead = 0,
    Grab1 = 1,
    Grab2 = 2,
    Grab3 = 3,
    Grab4 = 4,
    Seque = 5,
    Tada = 6,
    Theme = 7,
    Triumph = 8,
    Victory = 9,
    Ending = 10,
    OutOfTime = 11,
    Invader_Wave_Start = 12,
    Invader_Wave_Success = 13,
    Invader_End_of_Game = 14,
    Invader_Player_Fires_Bullet = 15,
    Invader_Enemy_Fires_Bullet = 16,
    Invader_Player_Explosion = 17,
    Invader_Enemy_Explosion = 18,
    Invader_Hit_Barrier = 19,
    Overrride_End = 19,
    Suppress_Start = 20,
    GateGoingDown = 20,
    GateGoingUp = 21,
    ChopChop = 22,
    Thump = 23,
    Strike = 24,
    Step = 25,
    Suppress_End = 25,
};

enum class GateMovement : uint8_t {
    None = 0,
    GoingUp = 1,
    WaitingToFall = 2,
    GoingDown = 3,
    WaitingToRaise = 4,
    StayOpen = 5,
    StayClosed = 6,
};

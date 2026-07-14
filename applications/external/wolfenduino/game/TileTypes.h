#ifndef TILE_TYPES_H_
#define TILE_TYPES_H_

enum TileType {
    Tile_Empty = 0,

    // Walls
    Tile_FirstWall,
    Tile_Wall01 = Tile_FirstWall,
    Tile_Wall02,
    Tile_Wall03,
    Tile_Wall04,
    Tile_Wall05,
    Tile_Wall06,
    Tile_Wall07,
    Tile_Wall08,
    Tile_Wall09,
    Tile_Wall10,
    Tile_Wall11,
    Tile_Wall12,
    Tile_Wall13,
    Tile_Wall14,
    Tile_Wall15,
    Tile_Wall16,
    Tile_Wall17,
    Tile_Wall18,
    Tile_Wall19,
    Tile_Wall20,
    Tile_ExitSwitchWall,
    Tile_Wall22,
    Tile_LastWall = Tile_Wall22,

    // Doors
    Tile_FirstDoor,
    Tile_Door_Generic_Horizontal = Tile_FirstDoor,
    Tile_Door_Generic_Vertical,
    Tile_Door_Locked1_Horizontal,
    Tile_Door_Locked1_Vertical,
    Tile_Door_Locked2_Horizontal,
    Tile_Door_Locked2_Vertical,
    Tile_Door_Elevator_Horizontal,
    Tile_Door_Elevator_Vertical,
    Tile_LastDoor = Tile_Door_Elevator_Vertical,

    // Actors
    Tile_FirstActor,
    Tile_Actor_Guard_Easy = Tile_FirstActor,
    Tile_Actor_Guard_Medium,
    Tile_Actor_Guard_Hard,
    Tile_Actor_SS_Easy,
    Tile_Actor_SS_Medium,
    Tile_Actor_SS_Hard,
    Tile_Actor_Dog_Easy,
    Tile_Actor_Dog_Medium,
    Tile_Actor_Dog_Hard,
    Tile_Actor_Boss,
    Tile_LastActor = Tile_Actor_Boss,

    // Player starts
    Tile_PlayerStart_North,
    Tile_PlayerStart_East,
    Tile_PlayerStart_South,
    Tile_PlayerStart_West,

    // Items
    Tile_FirstItem,
    Tile_Item_Clip = Tile_FirstItem,
    Tile_Item_FirstAid,
    Tile_Item_BadFood,
    Tile_Item_Food,
    Tile_FirstTreasure,
    Tile_Item_Cross = Tile_FirstTreasure,
    Tile_Item_Chalice,
    Tile_Item_Bible,
    Tile_Item_Crown,
    Tile_Item_1UP,
    Tile_LastTreasure = Tile_Item_1UP,
    Tile_Item_MachineGun,
    Tile_Item_ChainGun,
    Tile_Item_Key1,
    Tile_Item_Key2,
    Tile_LastItem = Tile_Item_Key2,

    // Blocking decoration
    Tile_FirstBlockingDecoration,
    Tile_BlockingDecoration_Table = Tile_FirstBlockingDecoration,
    Tile_BlockingDecoration_TableChairs,
    Tile_BlockingDecoration_FloorLamp,
    Tile_BlockingDecoration_Barrel,
    Tile_BlockingDecoration_Pillar,
    Tile_BlockingDecoration_Vase,
    Tile_BlockingDecoration_Tree,
    Tile_BlockingDecoration_HangingSkeleton,
    Tile_BlockingDecoration_Plant,
    Tile_BlockingDecoration_Sink,
    //Tile_BlockingDecoration_Well,
    Tile_BlockingDecoration_SuitOfArmour,
    Tile_LastBlockingDecoration = Tile_BlockingDecoration_SuitOfArmour,

    // Non blocking decoration
    Tile_FirstDecoration,
    Tile_Decoration_OverheadLamp = Tile_FirstDecoration,
    Tile_Decoration_DeadGuard,
    Tile_Decoration_Skeleton,
    Tile_Decoration_KitchenStuff,
    Tile_Decoration_Chandelier,
    Tile_LastDecoration = Tile_Decoration_Chandelier,

    Tile_SecretPushWall,
    Tile_SecretExit,
    Tile_CastleExit,

    NUM_TILE_TYPES
};

#endif

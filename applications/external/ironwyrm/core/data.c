#include "data.h"

const Material materials[MaterialIdCount] = {
    [MaterialCardboard] = {"Cardboard", 1, 0, 0},
    [MaterialWood] = {"Wood", 2, 1, 1},
    [MaterialBone] = {"Bone", 1, 2, 1},
    [MaterialCopper] = {"Copper", 1, 3, 2},
    [MaterialAluminium] = {"Aluminium", 3, 2, 2},
    [MaterialCastIron] = {"Cast Iron", 4, 1, 3},
    [MaterialBrass] = {"Brass", 3, 3, 3},
    [MaterialBronze] = {"Bronze", 4, 3, 4},
    [MaterialSteel] = {"Steel", 4, 4, 4},
    [MaterialCeramic] = {"Ceramic", 1, 5, 4},
    [MaterialTitanium] = {"Titanium", 6, 4, 6},
    [MaterialTungsten] = {"Tungsten", 4, 6, 6},
};

const Modifier modifiers[ModifierIdCount] = {
    [ModifierCracked] = {"Cracked", -3, -2, -3},
    [ModifierBlunt] = {"Blunt", -2, -3, -3},
    [ModifierCorroded] = {"Corroded", -2, -1, -2},
    [ModifierFlawed] = {"Flawed", -1, -2, -2},
    [ModifierWorn] = {"Worn", -1, -1, -1},
    [ModifierStandard] = {"Standard", 0, 0, 0},
    [ModifierRefined] = {"Refined", 1, 1, 1},
    [ModifierTempered] = {"Tempered", 2, 1, 2},
    [ModifierHardened] = {"Hardened", 1, 2, 2},
    [ModifierReinforced] = {"Reinforced", 3, 0, 3},
    [ModifierSerrated] = {"Serrated", 0, 3, 3},
    [ModifierMasterwork] = {"Masterwork", 2, 2, 3},
};

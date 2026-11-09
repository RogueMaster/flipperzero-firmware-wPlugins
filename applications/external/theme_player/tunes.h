#pragma once

/* Built-in tunes are original compositions written for this app. Add your own
 * tunes as .rtttl/.txt (RTTTL) or .fmf files in
 * SD:/apps_data/theme_player/<Theme>/ ; they never need to be in the repo. */
typedef struct {
    const char* theme;
    const char* rtttl;
} BuiltinTune;

static const BuiltinTune builtin_tunes[] = {
    {"Wizard",
     "Moonlit Tower:d=8,o=5,b=140:4e,g,4a,b,4a,g,4e,d#,2e,4p,4e,g,4b,c6,4b,a,4f#,a,2b,4p,4c6,b,4a,g,4f#,g,4e,d#,4e,f#,4g,a,2e"},
    {"Wizard",
     "Owl Post:d=8,o=5,b=170:e,g,b,e6,d6,b,g,b,a,c6,e6,a6,g6,e6,c6,e6,d6,b,g,d,f#,a,d6,c6,b,a,g,f#,4e,4p"},
    {"Adventure",
     "Hero's Road:d=8,o=5,b=150:4c,g,4c6,p,c6,d6,e6,f6,4g6,4e6,4c6,g,4a,f,4c6,a,4g,2p,4c,g,4c6,e6,d6,c6,b,4c6,4g,4e,2c"},
    {"Adventure",
     "Forest Dungeon:d=8,o=4,b=130:a,c5,e5,a5,g5,e5,c5,e5,f,a,c5,f5,e5,c5,a,c5,g,b,d5,g5,f5,d5,b,d5,e,g#,b,e5,4g#5,4p"},
    {"Space",
     "Orbit:d=16,o=5,b=120:c,e,g,c6,e6,g6,c7,g6,e6,c6,g,e,a,c6,e6,a6,c7,a6,e6,c6,f,a,c6,f6,a6,f6,c6,a,g,b,d6,g6,b6,g6,d6,b"},
    {"Space", "Starfield:d=4,o=5,b=90:e6,8p,8b,g,8p,8d6,2c6,a,8p,8e6,d6,8p,8c6,2b"},
    {"Spooky",
     "Haunted Hall:d=8,o=4,b=110:a,c5,d#5,a,c5,d#5,4f#5,g#,b,d5,g#,b,d5,4f5,a,c5,d#5,f#5,4a5,4g#5,2a5"},
    {"Spooky", "Crypt Waltz:d=4,o=4,b=160:d,8f,8a,d5,c#5,8e5,8g5,a5,g5,8f5,8e5,d5,a,8a#,8a,g,2d"},
    {"Retro Arcade",
     "Coin Rush:d=16,o=6,b=180:c,e,g,c7,8p,b,d7,g7,8p,a,c7,f7,8p,g,b,d7,4g7,8p,c,e,g,c7,e7,c7,g,e,4c"},
    {"Retro Arcade", "Level Up:d=8,o=5,b=200:c,e,g,c6,4p,d,f#,a,d6,4p,e,g#,b,e6,f6,g6,2c7"},
};

#define BUILTIN_TUNE_COUNT (sizeof(builtin_tunes) / sizeof(builtin_tunes[0]))

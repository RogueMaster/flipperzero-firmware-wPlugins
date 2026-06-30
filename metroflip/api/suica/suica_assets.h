#include <datetime.h>
#include <stdbool.h>
#include <furi.h>

#include "suica_structs_i.h"

#define SUICA_RAILWAY_NUM 86 // Don't count Unknown

#define SUICA_RAILWAY_UNKNOWN_NAME "Unknown"
// Railway

static const Railway RailwaysList[] = {
    // ----------------Area Code = 0b00----------------
    // East Japan Railway Company SuicaJREast
    {0x01, {0, 0}, "Keihin Tohoku", 14, SuicaJREast, "JK", 0},
    {0x01, {0, 0}, "East Tokaido", 21, SuicaJREast, "JT", 0},
    {0x02, {0, 0}, "Keihin Tohoku", 22, SuicaJREast, "JK", 0},
    {0x02, {0, 0}, "Utsunomiya", 33, SuicaJREast, "JU", 0},
    {0x02, {0, 0}, "Mobile Suica", 1, SuicaMobile, "", 0},
    {0x03, {0, 0}, "Chuo Rapid", 32, SuicaJREast, "JC", 0},
    {0x03, {0, 0}, "Chuo Main", 29, SuicaJREast, "CO", 0},
    {0x03, {0, 0}, "Chuo-Sobu", 18, SuicaJREast, "JB", 0},
    {0x05, {0, 0}, "Joban", 71, SuicaJREast, "JJ", 0},
    {0x05, {0, 0}, "Joban Local", 14, SuicaJREast, "JL", 0},
    {0x1D, {0, 0}, "Negishi", 10, SuicaJREast, "JK", 0},
    {0x25, {0, 0}, "Yamanote", 16, SuicaJREast, "JY", 0},

    // Central Japan Railway Company SuicaJRCentral
    {0x01, {0, 0}, "Central Tokaido", 83, SuicaJRCentral, "CA", 0},
    {0x03, {0, 0}, "Central Chuo", 37, SuicaJRCentral, "CF", 0},
    {0x3D, {1, 0}, "Gotemba", 18, SuicaJRCentral, "CB", 0},
    {0x3E, {0, 0}, "Minobu", 12, SuicaJRCentral, "CC", 0},
    {0x43, {1, 0}, "Taketoyo", 10, SuicaJRCentral, "CD", 0},
    {0x44, {0, 0}, "Takayama", 23, SuicaJRCentral, "CG", 0},
    {0x58, {0, 0}, "Taita", 8, SuicaJRCentral, "CI", 0},
    {0x5F, {0, 0}, "Kansai Main", 17, SuicaJRCentral, "CJ", 0},

    // West Japan Railway Company SuicaJRWest
    {0x01, {0, 0}, "Biwako", 20, SuicaJRWest, "A", 0},
    {0x01, {0, 0}, "Kyoto", 17, SuicaJRWest, "A", 0},
    {0x01, {0, 0}, "Kobe", 16, SuicaJRWest, "A", 0},
    {0x09, {0, 0}, "Hokuriku Main", 11, SuicaJRWest, "A", 0},
    {0x0A, {0, 0}, "Kobe", 22, SuicaJRWest, "A", 0},
    {0x0A, {0, 0}, "San'yo Main", 7, SuicaJRWest, "A", 0},
    {0x0A, {-1, 0}, "San'yo MI-OY", 11, SuicaJRWest, "S", 0},
    {0x0A, {-1, 0}, "San'yo OY-FY", 13, SuicaJRWest, "W", 0},
    {0x0A, {0, 0}, "San'yo FY-MH", 6, SuicaJRWest, "X", 0},
    {0x0A, {-1, 0}, "San'yo IZ-HS", 17, SuicaJRWest, "G", 0},
    {0x0A, {0, 0}, "San'yo HS-IK", 14, SuicaJRWest, "R", 0},
    {0x0B, {0, 0}, "Sagano", 16, SuicaJRWest, "E", 0},
    {0x0C, {-1, 0}, "Osaka Loop", 11, SuicaJRWest, "O", 0},
    {0x1F, {-1, 0}, "Seto-Ohashi", 7, SuicaJRWest, "M", 0},
    {0x20, {0, 0}, "Kibi Momotaro", 9, SuicaJRWest, "U", 0},
    {0x23, {0, 0}, "JR Tozai", 8, SuicaJRWest, "H", 0},
    {0x23, {0, 0}, "Hakubi", 13, SuicaJRWest, "V", 0},
    {0x24, {0, 0}, "Geibi", 9, SuicaJRWest, "P", 0},
    {0x26, {0, 0}, "Kure", 27, SuicaJRWest, "Y", 0},
    {0x2D, {0, 0}, "Kabe", 13, SuicaJRWest, "B", 0},
    {0x35, {-1, 0}, "Seto-Ohashi", 4, SuicaJRWest, "M", 0},
    {0x5B, {-1, 0}, "Osaka Loop", 8, SuicaJRWest, "O", 0},
    {0x5C, {0, 0}, "Sakurajima", 4, SuicaJRWest, "P", 0},
    {0x5F, {0, 0}, "Yamatoji", 22, SuicaJRWest, "Q", 0},
    {0x60, {0, 0}, "Kyoto", 22, SuicaJRWest, "D", 0},
    {0x63, {0, 0}, "Hanwa", 34, SuicaJRWest, "R", 0},
    {0x6A, {0, 0}, "Osaka Higashi", 5, SuicaJRWest, "F", 0},
    {0x6D, {0, 0}, "Kosai", 19, SuicaJRWest, "B", 0},
    {0x70, {-1, 0}, "Kansai Airport", 2, SuicaJRWest, "S", 0},
    {0x73, {0, 0}, "Ako", 12, SuicaJRWest, "N", 0},

    // Tokyo Waterfront Area Rapid Transit Company SuicaTWR
    {0x82, {0, 0}, "Rinkai", 8, SuicaTWR, "R", &I_Suica_RinkaiR},
    {0x82, {0, 0}, "Yurikamome", 16, SuicaYurikamome, "U", &I_Suica_YurikamomeU},

    // Tokyu Corporation SuicaTokyu
    {0xCE, {0, 0}, "Toyoko", 23, SuicaTokyu, "TY", 0},
    {0xCF, {0, 0}, "Tamagawa", 7, SuicaTokyu, "TM", 0},
    {0xCF, {0, 0}, "Meguro", 8, SuicaTokyu, "MG", 0},
    {0xD0, {0, 0}, "Oimachi", 15, SuicaTokyu, "OM", 0},
    {0xD0, {0, 0}, "Den-en-toshi", 20, SuicaTokyu, "DT", 0},
    {0xD1, {0, 0}, "Ikegami", 15, SuicaTokyu, "IK", 0},
    {0xD2, {0, 0}, "Den-en-toshi", 7, SuicaTokyu, "DT", 0},
    {0xD3, {0, 0}, "Setagaya", 10, SuicaTokyu, "SG", 0},

    // Keikyu Corporation SuicaKeikyu
    {0xD5, {0, 0}, "Keikyu Main", 50, SuicaKeikyu, "KK", &I_Suica_KeikyuKK},
    {0xD6, {0, 0}, "Keikyu Airport", 6, SuicaKeikyu, "KK", &I_Suica_KeikyuKK},

    // Tokyo Metro Co. SuicaTokyoMetro
    {0xE3, {0, 0}, "Ginza", 19, SuicaTokyoMetro, "G", &I_Suica_GinzaG},
    {0xE3, {1, 0}, "Chiyoda", 20, SuicaTokyoMetro, "C", &I_Suica_ChiyodaC},
    {0xE3, {1, 1}, "Yurakucho", 24, SuicaTokyoMetro, "Y", &I_Suica_YurakuchoY},
    {0xE4, {1, 0}, "Hibiya", 21, SuicaTokyoMetro, "H", &I_Suica_HibiyaH},
    {0xE4, {2, 1}, "Tozai", 23, SuicaTokyoMetro, "T", &I_Suica_TozaiT},
    {0xE5, {0, 1}, "Marunouchi", 25, SuicaTokyoMetro, "M", &I_Suica_MarunouchiM},
    {0xE5, {-5, 1}, "M Honancho", 4, SuicaTokyoMetro, "Mb", &I_Suica_MarunouchiHonanchoMb},
    {0xE6, {2, 1}, "Hanzomon", 14, SuicaTokyoMetro, "Z", &I_Suica_HanzomonZ},
    {0xE7, {0, 1}, "Namboku", 19, SuicaTokyoMetro, "N", &I_Suica_NambokuN},

    // Tokyo Metropolitan Bureau of Transportation SuicaToei
    {0xEF, {0, 0}, "Asakusa", 20, SuicaToei, "A", &I_Suica_AsakusaA},
    {0xF0, {4, 0}, "Mita", 27, SuicaToei, "I", &I_Suica_MitaI},
    {0xF1, {2, 0}, "Shinjuku", 21, SuicaToei, "S", &I_Suica_ShinjukuS},
    {0xF2, {3, 0}, "Oedo", 26, SuicaToei, "E", &I_Suica_OedoE},
    {0xF3, {3, 0}, "Oedo", 14, SuicaToei, "E", &I_Suica_OedoE},

    // Tokyo Monorail SuicaTokyoMonorail
    {0xFA, {0, 0}, "Tokyo Monorail", 11, SuicaTokyoMonorail, "MO", 0},

    // ----------------Area Code = 0b10----------------
    // Osaka Metro Company SuicaOsakaMetro
    {0x81, {0, 0}, "#1 Midosuji", 20, SuicaOsakaMetro, "M", 0},
    {0x82, {0, 0}, "#2 Tanimachi", 26, SuicaOsakaMetro, "T", 0},
    {0x83, {0, 0}, "#3 Yotsubashi", 11, SuicaOsakaMetro, "Y", 0},
    {0x84, {0, 0}, "#4 Chuo", 14, SuicaOsakaMetro, "C", 0},
    {0x85, {0, 0}, "#5 Sennichimae", 14, SuicaOsakaMetro, "S", 0},
    {0x86, {0, 0}, "#6 Sakaisuji", 10, SuicaOsakaMetro, "K", 0},
    {0x87, {0, 0}, "#7 Nagahori T-R", 17, SuicaOsakaMetro, "N", 0},
    {0x88, {0, 0}, "#8 Imazatosuji", 11, SuicaOsakaMetro, "I", 0},
    {0x8C, {0, 0}, "Nanko PortTown", 10, SuicaOsakaMetro, "P", 0},

    // Unknown
    {0x00,
     {0, 0},
     SUICA_RAILWAY_UNKNOWN_NAME,
     1,
     SuicaRailwayTypeMax,
     "??",
     &I_Suica_QuestionMarkBig}};

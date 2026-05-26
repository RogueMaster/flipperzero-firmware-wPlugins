#include "magicband_plus_lights.h"
#include <gui/gui.h>
#include <furi_hal_bt.h>
#include <extra_beacon.h>
#include <gui/elements.h>

#include "protocols/_protocols.h"

// MagicBand + Lights
// OG code by WillyJL, Modified by haw8411 to fit & work with MagicBands.

static Attack
    attacks[] =
        {{
             .title = "MagicBand+ Library",
             .text = "Disney 0x0183 beacons",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "CC Ping",
             .text = "cc03000000",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_CC,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "CC 000100",
             .text = "cc03000100",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_CC,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "CC 132000",
             .text = "cc03132000",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_CC,
                             .index = 2,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "00 cyan",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "01 purple",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 1,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "02 blue",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 2,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "03 midnight blue",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 3,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "04 blue",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 4,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "05 bright purple",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 5,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "06 lavender",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 6,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "07 purple",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 7,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "08 pink",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 8,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "09 pink",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 9,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "10 pink",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 10,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "11 pink",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 11,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "12 pink",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 12,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "13 pink",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 13,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "14 pink",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 14,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "15 yellow orange",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 15,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "16 off yellow",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 16,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "17 yellow orange",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 17,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "18 lime",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 18,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "19 orange",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 19,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "20 red orange",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 20,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "21 red",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 21,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "22 cyan",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 22,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "23 cyan",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 23,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "24 cyan",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 24,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "25 green",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 25,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "26 lime green",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 26,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "27 white",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 27,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "28 white",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 28,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "29 off",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 29,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "30 unique",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 30,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "31 random",
             .text = "single color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E905_Single,
                             .index = 0,
                             .color5 = 31,
                             .vibe_on = true,
                         },
                 },
         },
         {
             .title = "E9-06 Dual (ex1)",
             .text = "E9-06 dual palette",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E906_Dual,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-07 (ex1)",
             .text = "unknown effect ex1",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E907_Unknown,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-07 (ex2)",
             .text = "unknown effect ex2",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E907_Unknown,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-08 (ex1)",
             .text = "6-bit color",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E908_6bit,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-09 5-Color (ex1)",
             .text = "5-color palette",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E909_5Palette,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0B Circle (ex1)",
             .text = "circle animation",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90B_Circle,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0C Blink White",
             .text = "blink white",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90C_Animations,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0C Orange Blink",
             .text = "orange blink",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90C_Animations,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0C 5-Color Cycle",
             .text = "palette cycle",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90C_Animations,
                             .index = 2,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0C Taste the Rainbow",
             .text = "rainbow cycle",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90C_Animations,
                             .index = 3,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0E (ex1)",
             .text = "example 1",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90E_Examples,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0E (ex2)",
             .text = "example 2",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90E_Examples,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0E (ex3)",
             .text = "example 3",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90E_Examples,
                             .index = 2,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0E (ex4)",
             .text = "example 4",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90E_Examples,
                             .index = 3,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0E (ex5)",
             .text = "example 5",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90E_Examples,
                             .index = 4,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0F (ex1)",
             .text = "example 1",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90F_Examples,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-0F (ex2)",
             .text = "example 2",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E90F_Examples,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-10 Alternating (ex1)",
             .text = "alternating colors",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E910_Alternating,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-11 Cross Fade (ex1)",
             .text = "cross fade",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E911_Crossfade,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-11 Cross Fade (ex2)",
             .text = "cross fade",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E911_Crossfade,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-11 Cross Fade (ex3)",
             .text = "cross fade",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E911_Crossfade,
                             .index = 2,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-11 Cross Fade (ex4)",
             .text = "cross fade",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E911_Crossfade,
                             .index = 3,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-11 Cross Fade (ex5)",
             .text = "cross fade",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E911_Crossfade,
                             .index = 4,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-11 Cross Fade (ex6)",
             .text = "cross fade",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E911_Crossfade,
                             .index = 5,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-11 Cross Fade (ex7)",
             .text = "cross fade",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E911_Crossfade,
                             .index = 6,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-12 Circle+Vibe (ex1)",
             .text = "circle + vibe",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E912_CircleVibe,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-12 Circle+Vibe (ex2)",
             .text = "circle + vibe",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E912_CircleVibe,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-12 Circle+Vibe (ex3)",
             .text = "circle + vibe",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E912_CircleVibe,
                             .index = 2,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-13 (ex1)",
             .text = "animation",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E913_Examples,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-13 (ex2)",
             .text = "animation",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E913_Examples,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-14 (ex1)",
             .text = "animation",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E914_Examples,
                             .index = 0,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-14 (ex2)",
             .text = "animation",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E914_Examples,
                             .index = 1,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         },
         {
             .title = "E9-14 (ex3)",
             .text = "animation",
             .protocol = &protocol_magicband,
             .payload =
                 {
                     .random_mac = true,
                     .cfg.magicband =
                         {
                             .category = MB_Cat_E914_Examples,
                             .index = 2,
                             .color5 = 0,
                             .vibe_on = false,
                         },
                 },
         }};

#define ATTACKS_COUNT ((signed)COUNT_OF(attacks))

static uint16_t delays[] = {20, 50, 100, 200, 500};

typedef struct {
    Ctx ctx;
    View* main_view;
    bool lock_warning;
    uint8_t lock_count;
    FuriTimer* lock_timer;

    bool advertising;
    uint8_t delay;
    GapExtraBeaconConfig config;
    FuriThread* thread;
    int8_t index;
    bool ignore_bruteforce;
} State;

const NotificationSequence solid_message = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_do_not_reset,
    &message_delay_10,
    NULL,
};
NotificationMessage blink_message = {
    .type = NotificationMessageTypeLedBlinkStart,
    .data.led_blink.color = LightBlue | LightGreen,
    .data.led_blink.on_time = 10,
    .data.led_blink.period = 100,
};
const NotificationSequence blink_sequence = {
    &blink_message,
    &message_do_not_reset,
    NULL,
};
static void start_blink(State* state) {
    if(!state->ctx.led_indicator) return;
    uint16_t period = delays[state->delay];
    if(period <= 100) period += 30;
    blink_message.data.led_blink.period = period;
    notification_message_block(state->ctx.notification, &blink_sequence);
}
static void stop_blink(State* state) {
    if(!state->ctx.led_indicator) return;
    notification_message_block(state->ctx.notification, &sequence_blink_stop);
}

static void randomize_mac(State* state) {
    furi_hal_random_fill_buf(state->config.address, sizeof(state->config.address));
}

static void start_extra_beacon(State* state) {
    uint8_t size;
    uint8_t* packet;
    uint16_t delay = delays[state->delay];
    GapExtraBeaconConfig* config = &state->config;
    Payload* payload = &attacks[state->index].payload;
    const Protocol* protocol = attacks[state->index].protocol;

    config->min_adv_interval_ms = delay;
    config->max_adv_interval_ms = delay * 1.5;
    if(payload->random_mac) randomize_mac(state);
    furi_check(furi_hal_bt_extra_beacon_set_config(config));

    if(protocol) {
        protocol->make_packet(&size, &packet, payload);
    } else {
        protocols[rand() % protocols_count]->make_packet(&size, &packet, NULL);
    }
    furi_check(furi_hal_bt_extra_beacon_set_data(packet, size));
    free(packet);

    furi_check(furi_hal_bt_extra_beacon_start());
}

static int32_t adv_thread(void* _ctx) {
    State* state = _ctx;
    Payload* payload = &attacks[state->index].payload;
    const Protocol* protocol = attacks[state->index].protocol;
    if(!payload->random_mac) randomize_mac(state);
    start_blink(state);
    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_check(furi_hal_bt_extra_beacon_stop());
    }

    while(state->advertising) {
        if(protocol && payload->mode == PayloadModeBruteforce &&
           payload->bruteforce.counter++ >= 10) {
            payload->bruteforce.counter = 0;
            payload->bruteforce.value =
                (payload->bruteforce.value + 1) % (1 << (payload->bruteforce.size * 8));
        }

        start_extra_beacon(state);

        furi_thread_flags_wait(true, FuriFlagWaitAny, delays[state->delay]);
        furi_check(furi_hal_bt_extra_beacon_stop());
    }

    stop_blink(state);
    return 0;
}

static void toggle_adv(State* state) {
    if(state->advertising) {
        state->advertising = false;
        furi_thread_flags_set(furi_thread_get_id(state->thread), true);
        furi_thread_join(state->thread);
    } else {
        state->advertising = true;
        furi_thread_start(state->thread);
    }
}

#define PAGE_MIN (-5)
#define PAGE_MAX ATTACKS_COUNT
enum {
    PageHelpBruteforce = PAGE_MIN,
    PageHelpApps,
    PageHelpDelay,
    PageHelpDistance,
    PageHelpInfoConfig,
    PageStart = 0,
    PageEnd = ATTACKS_COUNT - 1,
    PageAboutCredits = PAGE_MAX,
};

static void draw_callback(Canvas* canvas, void* _ctx) {
    State* state = *(State**)_ctx;
    const char* back = "Back";
    const char* next = "Next";
    if(state->index < 0) {
        back = "Next";
        next = "Back";
    }
    switch(state->index) {
    case PageStart - 1:
        next = "Spam";
        break;
    case PageStart:
        back = "Help";
        break;
    case PageEnd:
        next = "About";
        break;
    case PageEnd + 1:
        back = "Spam";
        break;
    }

    const Attack* attack =
        (state->index >= 0 && state->index <= ATTACKS_COUNT - 1) ? &attacks[state->index] : NULL;
    const Payload* payload = attack ? &attack->payload : NULL;
    const Protocol* protocol = attack ? attack->protocol : NULL;

    canvas_set_font(canvas, FontSecondary);
    const Icon* icon = protocol ? protocol->icon : &I_ble_spam;
    canvas_draw_icon(canvas, 4 - (icon == &I_ble_spam), 3, icon);
    canvas_draw_str(canvas, 14, 12, "MagicBand +");

    switch(state->index) {
    case PageHelpBruteforce:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 124, 12, AlignRight, AlignBottom, "Help");
        elements_text_box(
            canvas,
            4,
            16,
            120,
            48,
            AlignLeft,
            AlignTop,
            "\e#Bruteforce\e# cycles codes\n"
            "to find popups, hold left and\n"
            "right to send manually and\n"
            "change delay",
            false);
        break;
    case PageHelpApps:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 124, 12, AlignRight, AlignBottom, "Help");
        elements_text_box(
            canvas,
            4,
            16,
            120,
            48,
            AlignLeft,
            AlignTop,
            "\e#Some Apps\e# interfere\n"
            "with the attacks, stay on\n"
            "homescreen for best results",
            false);
        break;
    case PageHelpDelay:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 124, 12, AlignRight, AlignBottom, "Help");
        elements_text_box(
            canvas,
            4,
            16,
            120,
            48,
            AlignLeft,
            AlignTop,
            "\e#Delay\e# is time between\n"
            "attack attempts (top right),\n"
            "keep 20ms for best results",
            false);
        break;
    case PageHelpDistance:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 124, 12, AlignRight, AlignBottom, "Help");
        elements_text_box(
            canvas,
            4,
            16,
            120,
            48,
            AlignLeft,
            AlignTop,
            "\e#Distance\e# varies greatly:\n"
            "some are long range (>30 m)\n"
            "others are close range (<1 m)",
            false);
        break;
    case PageHelpInfoConfig:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 124, 12, AlignRight, AlignBottom, "Help");
        elements_text_box(
            canvas,
            4,
            16,
            120,
            48,
            AlignLeft,
            AlignTop,
            "See \e#more info\e# and change\n"
            "attack \e#options\e# by holding\n"
            "Ok on each attack page",
            false);
        break;
    case PageAboutCredits:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 124, 12, AlignRight, AlignBottom, "Credits");
        elements_text_box(
            canvas,
            4,
            16,
            122,
            48,
            AlignLeft,
            AlignTop,
            "Original App/Code: \e#WillyJL\e#\n"
            "Modified By: \e#haw8411\e#\n"
            "                                   Version \e#" FAP_VERSION "\e#",
            false);
        break;
    default: {
        if(!attack) break;
        if(state->ctx.lock_keyboard && !state->advertising) {
            // Forgive me Lord for I have sinned by handling state in draw
            toggle_adv(state);
        }
        char str[32];

        canvas_set_font(canvas, FontPrimary);
        if(payload->mode == PayloadModeBruteforce) {
            snprintf(
                str,
                sizeof(str),
                "0x%0*lX",
                payload->bruteforce.size * 2,
                payload->bruteforce.value);
        } else {
            snprintf(str, sizeof(str), "%ims", delays[state->delay]);
        }
        canvas_draw_str_aligned(canvas, 116, 12, AlignRight, AlignBottom, str);
        canvas_draw_icon(canvas, 119, 6, &I_SmallArrowUp_3x5);
        canvas_draw_icon(canvas, 119, 10, &I_SmallArrowDown_3x5);

        canvas_set_font(canvas, FontPrimary);
        if(payload->mode == PayloadModeBruteforce) {
            canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignBottom, "Bruteforce");
            if(delays[state->delay] < 100) {
                snprintf(str, sizeof(str), "%ims>", delays[state->delay]);
            } else {
                snprintf(str, sizeof(str), "%.1fs>", (double)delays[state->delay] / 1000);
            }
            uint16_t w = canvas_string_width(canvas, str);
            elements_slightly_rounded_box(canvas, 3, 14, 30, 10);
            elements_slightly_rounded_box(canvas, 119 - w, 14, 6 + w, 10);
            canvas_invert_color(canvas);
            canvas_draw_str_aligned(canvas, 5, 22, AlignLeft, AlignBottom, "<Send");
            canvas_draw_str_aligned(canvas, 122, 22, AlignRight, AlignBottom, str);
            canvas_invert_color(canvas);
        } else {
            snprintf(
                str,
                sizeof(str),
                "%02i/%02i: %s",
                state->index + 1,
                ATTACKS_COUNT,
                protocol ? protocol->get_name(payload) : "Everything AND");
            canvas_draw_str(canvas, 4 - (state->index < 19 ? 1 : 0), 22, str);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 33, attack->title);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 4, 46, attack->text);

        elements_button_center(canvas, state->advertising ? "Stop" : "Start");
        break;
    }
    }

    if(state->index > PAGE_MIN) {
        elements_button_left(canvas, back);
    }
    if(state->index < PAGE_MAX) {
        elements_button_right(canvas, next);
    }

    if(state->lock_warning) {
        canvas_set_font(canvas, FontSecondary);
        elements_bold_rounded_frame(canvas, 14, 8, 99, 48);
        elements_multiline_text(canvas, 65, 26, "To unlock\npress:");
        canvas_draw_icon(canvas, 65, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 80, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 95, 42, &I_Pin_back_arrow_10x8);
        canvas_draw_icon(canvas, 16, 13, &I_WarningDolphin_45x42);
        canvas_draw_dot(canvas, 17, 61);
    }
}

static bool input_callback(InputEvent* input, void* _ctx) {
    View* view = _ctx;
    State* state = *(State**)view_get_model(view);
    bool consumed = false;

    if(state->ctx.lock_keyboard) {
        consumed = true;
        state->lock_warning = true;
        if(state->lock_count == 0) {
            furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
            furi_timer_start(state->lock_timer, 1000);
        }
        if(input->type == InputTypeShort && input->key == InputKeyBack) {
            state->lock_count++;
        }
        if(state->lock_count >= 3) {
            furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
            furi_timer_start(state->lock_timer, 1);
        }
    } else if(
        input->type == InputTypeShort || input->type == InputTypeLong ||
        input->type == InputTypeRepeat) {
        consumed = true;

        bool is_attack = state->index >= 0 && state->index <= ATTACKS_COUNT - 1;
        Payload* payload = is_attack ? &attacks[state->index].payload : NULL;
        bool advertising = state->advertising;

        switch(input->key) {
        case InputKeyOk:
            if(is_attack) {
                if(input->type == InputTypeLong) {
                    if(advertising) toggle_adv(state);
                    state->ctx.attack = &attacks[state->index];
                    scene_manager_set_scene_state(state->ctx.scene_manager, SceneConfig, 0);
                    view_commit_model(view, consumed);
                    scene_manager_next_scene(state->ctx.scene_manager, SceneConfig);
                    return consumed;
                } else if(input->type == InputTypeShort) {
                    toggle_adv(state);
                }
            }
            break;
        case InputKeyUp:
            if(is_attack) {
                if(payload->mode == PayloadModeBruteforce) {
                    payload->bruteforce.counter = 0;
                    payload->bruteforce.value =
                        (payload->bruteforce.value + 1) % (1 << (payload->bruteforce.size * 8));
                } else if(state->delay < COUNT_OF(delays) - 1) {
                    state->delay++;
                    if(advertising) start_blink(state);
                }
            }
            break;
        case InputKeyDown:
            if(is_attack) {
                if(payload->mode == PayloadModeBruteforce) {
                    payload->bruteforce.counter = 0;
                    payload->bruteforce.value =
                        (payload->bruteforce.value - 1) % (1 << (payload->bruteforce.size * 8));
                } else if(state->delay > 0) {
                    state->delay--;
                    if(advertising) start_blink(state);
                }
            }
            break;
        case InputKeyLeft:
            if(input->type == InputTypeLong) {
                state->ignore_bruteforce = payload ? (payload->mode != PayloadModeBruteforce) :
                                                     true;
            }
            if(input->type == InputTypeShort || !is_attack || state->ignore_bruteforce ||
               payload->mode != PayloadModeBruteforce) {
                if(state->index > PAGE_MIN) {
                    if(advertising) toggle_adv(state);
                    state->index--;
                }
            } else {
                if(!advertising) {
                    Payload* payload = &attacks[state->index].payload;
                    if(input->type == InputTypeLong && !payload->random_mac) randomize_mac(state);
                    if(furi_hal_bt_extra_beacon_is_active()) {
                        furi_check(furi_hal_bt_extra_beacon_stop());
                    }

                    start_extra_beacon(state);

                    if(state->ctx.led_indicator)
                        notification_message(state->ctx.notification, &solid_message);
                    furi_delay_ms(10);
                    furi_check(furi_hal_bt_extra_beacon_stop());

                    if(state->ctx.led_indicator)
                        notification_message_block(state->ctx.notification, &sequence_reset_rgb);
                }
            }
            break;
        case InputKeyRight:
            if(input->type == InputTypeLong) {
                state->ignore_bruteforce = payload ? (payload->mode != PayloadModeBruteforce) :
                                                     true;
            }
            if(input->type == InputTypeShort || !is_attack || state->ignore_bruteforce ||
               payload->mode != PayloadModeBruteforce) {
                if(state->index < PAGE_MAX) {
                    if(advertising) toggle_adv(state);
                    state->index++;
                }
            } else if(input->type == InputTypeLong) {
                state->delay = (state->delay + 1) % COUNT_OF(delays);
                if(advertising) start_blink(state);
            }
            break;
        case InputKeyBack:
            if(advertising) toggle_adv(state);
            consumed = false;
            break;
        default:
            break;
        }
    }

    view_commit_model(view, consumed);
    return consumed;
}

static void lock_timer_callback(void* _ctx) {
    State* state = _ctx;
    if(state->lock_count < 3) {
        notification_message_block(state->ctx.notification, &sequence_display_backlight_off);
    } else {
        state->ctx.lock_keyboard = false;
    }
    with_view_model(state->main_view, State * *model, { (*model)->lock_warning = false; }, true);
    state->lock_count = 0;
    furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
}

static bool custom_event_callback(void* _ctx, uint32_t event) {
    State* state = _ctx;
    return scene_manager_handle_custom_event(state->ctx.scene_manager, event);
}

static void tick_event_callback(void* _ctx) {
    State* state = _ctx;
    bool advertising;
    with_view_model(
        state->main_view, State * *model, { advertising = (*model)->advertising; }, advertising);
    scene_manager_handle_tick_event(state->ctx.scene_manager);
}

static bool back_event_callback(void* _ctx) {
    State* state = _ctx;
    return scene_manager_handle_back_event(state->ctx.scene_manager);
}

int32_t magicband_plus_lights_main(void* p) {
    UNUSED(p);
    GapExtraBeaconConfig prev_cfg;
    const GapExtraBeaconConfig* prev_cfg_ptr = furi_hal_bt_extra_beacon_get_config();
    if(prev_cfg_ptr) {
        memcpy(&prev_cfg, prev_cfg_ptr, sizeof(prev_cfg));
    }
    uint8_t prev_data[EXTRA_BEACON_MAX_DATA_SIZE];
    uint8_t prev_data_len = furi_hal_bt_extra_beacon_get_data(prev_data);
    bool prev_active = furi_hal_bt_extra_beacon_is_active();

    State* state = malloc(sizeof(State));
    state->config.adv_channel_map = GapAdvChannelMapAll;
    state->config.adv_power_level = GapAdvPowerLevel_6dBm;
    state->config.address_type = GapAddressTypePublic;
    state->thread = furi_thread_alloc();
    furi_thread_set_callback(state->thread, adv_thread);
    furi_thread_set_context(state->thread, state);
    furi_thread_set_stack_size(state->thread, 2048);
    state->ctx.led_indicator = true;
    state->lock_timer = furi_timer_alloc(lock_timer_callback, FuriTimerTypeOnce, state);

    state->ctx.notification = furi_record_open(RECORD_NOTIFICATION);
    Gui* gui = furi_record_open(RECORD_GUI);
    state->ctx.view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(state->ctx.view_dispatcher);

    view_dispatcher_set_event_callback_context(state->ctx.view_dispatcher, state);
    view_dispatcher_set_custom_event_callback(state->ctx.view_dispatcher, custom_event_callback);
    view_dispatcher_set_tick_event_callback(state->ctx.view_dispatcher, tick_event_callback, 100);
    view_dispatcher_set_navigation_event_callback(state->ctx.view_dispatcher, back_event_callback);
    state->ctx.scene_manager = scene_manager_alloc(&scene_handlers, &state->ctx);

    state->main_view = view_alloc();
    view_allocate_model(state->main_view, ViewModelTypeLocking, sizeof(State*));
    with_view_model(state->main_view, State * *model, { *model = state; }, false);
    view_set_context(state->main_view, state->main_view);
    view_set_draw_callback(state->main_view, draw_callback);
    view_set_input_callback(state->main_view, input_callback);
    view_dispatcher_add_view(state->ctx.view_dispatcher, ViewMain, state->main_view);

    state->ctx.byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        state->ctx.view_dispatcher, ViewByteInput, byte_input_get_view(state->ctx.byte_input));

    state->ctx.submenu = submenu_alloc();
    view_dispatcher_add_view(
        state->ctx.view_dispatcher, ViewSubmenu, submenu_get_view(state->ctx.submenu));

    state->ctx.text_input = text_input_alloc();
    view_dispatcher_add_view(
        state->ctx.view_dispatcher, ViewTextInput, text_input_get_view(state->ctx.text_input));

    state->ctx.variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        state->ctx.view_dispatcher,
        ViewVariableItemList,
        variable_item_list_get_view(state->ctx.variable_item_list));

    view_dispatcher_attach_to_gui(state->ctx.view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(state->ctx.scene_manager, SceneMain);
    view_dispatcher_run(state->ctx.view_dispatcher);

    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewByteInput);
    byte_input_free(state->ctx.byte_input);

    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewSubmenu);
    submenu_free(state->ctx.submenu);

    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewTextInput);
    text_input_free(state->ctx.text_input);

    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewVariableItemList);
    variable_item_list_free(state->ctx.variable_item_list);

    view_dispatcher_remove_view(state->ctx.view_dispatcher, ViewMain);
    view_free(state->main_view);

    scene_manager_free(state->ctx.scene_manager);
    view_dispatcher_free(state->ctx.view_dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    furi_timer_free(state->lock_timer);
    furi_thread_free(state->thread);
    free(state);

    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_check(furi_hal_bt_extra_beacon_stop());
    }
    if(prev_cfg_ptr) {
        furi_check(furi_hal_bt_extra_beacon_set_config(&prev_cfg));
    }
    furi_check(furi_hal_bt_extra_beacon_set_data(prev_data, prev_data_len));
    if(prev_active) {
        furi_check(furi_hal_bt_extra_beacon_start());
    }
    return 0;
}

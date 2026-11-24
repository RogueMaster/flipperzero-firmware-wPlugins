#include "url_links.h"
#include <stddef.h>
#include <string.h>

/* Source links are curated by stable collection ID. Interactive maps and category
 * indexes are source references, not direct map images. Populate map_image only
 * with an actual map image URL. */
static const char url_0[] = "https://finalboss.io/gta-6-cocaine-flamingo-locations";
static const char url_1[] = "https://games.gg/grand-theft-auto-vi/guides/grand-theft-auto-6-waxman-island-safe-code-purple-drank-emerus/";
static const char url_2[] = "https://getvicekeys.com/guides/gta-6-mariana-county-pink-flamingo-route";
static const char url_3[] = "https://gta6-japan.com/en/guides/gun-nut-house-key-lento-loot/";
static const char url_4[] = "https://gta6-japan.com/en/guides/la-polilla-negra-kz-59-palmer-fishing-warehouse/";
static const char url_5[] = "https://gta6-japan.com/en/guides/royal-ruby-emerus-vespucci-mansion/";
static const char url_6[] = "https://gta6bugs.com/easter-eggs/burg-industries-underwater-facilities-meteorite";
static const char url_7[] = "https://gta6gps.com/?p=gamersky-917805";
static const char url_8[] = "https://gta6gps.com/?p=pin-918017";
static const char url_9[] = "https://gta6gps.com/?p=pin-918042";
static const char url_10[] = "https://gta6gps.com/?p=pin-918184";
static const char url_11[] = "https://gta6gps.com/?p=pin-918185";
static const char url_12[] = "https://gta6gps.com/?p=pin-918457";
static const char url_13[] = "https://gta6gps.com/?p=pin-918460";
static const char url_14[] = "https://gta6gps.com/?p=pin-918551";
static const char url_15[] = "https://gta6gps.com/?p=pin-918560";
static const char url_16[] = "https://gta6gps.com/?p=pin-918626";
static const char url_17[] = "https://gta6gps.com/?p=pin-918628";
static const char url_18[] = "https://gta6gps.com/?p=pin-918629";
static const char url_19[] = "https://gta6gps.com/map/game/kenton-sets";
static const char url_20[] = "https://gta6realmap.com/cocaine-flamingos-and-cards";
static const char url_21[] = "https://gtadb.net/articles/gta-6-cocaine-flamingos-locations/";
static const char url_22[] = "https://gtadb.net/gta-6-map/interactive/?category=collectibles&marker=IG6_000842&set=cocaine-flamingos&type=cocaine-flamingos";
static const char url_23[] = "https://gtadb.net/gta-6-map/interactive/?category=collectibles&marker=IG6_000843&set=cocaine-flamingos&type=cocaine-flamingos";
static const char url_24[] = "https://gtascoop.com/guides/gta-6-ghost-ship-lighthouse/";
static const char url_25[] = "https://gtascoop.com/guides/gta-6-meteorite-locations/";
static const char url_26[] = "https://gtascoop.com/guides/gta-6-sos-island/";
static const char url_27[] = "https://insider-gaming.com/how-to-get-the-fastest-car-in-gta-6/";
static const char url_28[] = "https://leonidaplayers.com/guides/gta-6-best-silenced-weapons-11-legendary-suppressed-guns-locations";
static const char url_29[] = "https://libertycity.net/map/gta6/1603-cocaine-flamingo-dorado-pawn-shop.html";
static const char url_30[] = "https://linkleung310.github.io/gtavi-atlas/en/guides/collectibles/meteorites-field.html";
static const char url_31[] = "https://lootmapped.com/gta-6/map/";
static const char url_32[] = "https://metaforge.app/gta-6/gta-6-cocaine-flamingos-locations";
static const char url_33[] = "https://metaforge.app/gta-6/map/leonida?markerID=ab9ab221-bfe0-7f57-cb06-f351c9405a07";
static const char url_34[] = "https://metaforge.app/gta-6/map/leonida?markerID=f7a03abe-cd3d-8f21-fb4a-a3cd50d2fe05";
static const char url_35[] = "https://www.dexerto.com/wikis/gta-6/vehicles/standout/blinged-up/gold-scarab-windsor/";
static const char url_36[] = "https://www.gtabase.com/gta-6/map/?feature=hidden-packages";
static const char url_37[] = "https://www.gtabase.com/gta-6/map/?feature=meteorite";
static const char url_38[] = "https://www.gtabase.com/gta-6/map/?feature=standout-vehicle";
static const char url_39[] = "https://www.gtabase.com/gta-6/map/?feature=unique-weapon";
static const char url_40[] = "https://www.gtabase.com/gta-6/weapons/candy-suxxx-bat";
static const char url_41[] = "https://www.gtabase.com/gta-6/weapons/pocketblast-dsg";
static const char url_42[] = "https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/kentons-treasures";
static const char url_43[] = "https://www.powerpyx.com/gta-6-help-kirk-find-mr-mangrove-walkthrough";
static const char url_44[] = "https://www.powerpyx.com/gta-6-how-to-get-ultimate-edition-rewards/";
static const char url_45[] = "https://www.powerpyx.com/gta-6-hyde-island-kidnapping-easter-egg/";
static const char url_46[] = "https://www.pushsquare.com/guides/gta-6-underwater-discoveries-locations-and-kentons-treasures";

static const char flamingo_pin_3793[] = "https://gta6gps.com/?p=pin-918472";
static const char flamingo_pin_3794[] = "https://gta6gps.com/?p=pin-918220";
static const char flamingo_pin_3795[] = "https://gta6gps.com/?p=pin-918625";
static const char flamingo_pin_3796[] = "https://gta6gps.com/?p=pin-918627";

static const struct { uint16_t id; GtaUrlLinks links; } gta_links[] = {
    {140, {"https://gta6gps.com/?p=pin-916989", NULL, NULL, false, false}},
    {151, {"https://gta6gps.com/?p=pin-916977", NULL, NULL, false, false}},
    {163, {"https://gta6gps.com/?p=pin-916912", NULL, NULL, false, false}},
    {197, {"https://gta6gps.com/?p=pin-916957", NULL, NULL, false, false}},
    {201, {"https://gta6gps.com/?p=pin-916943", NULL, NULL, false, false}},
    {203, {"https://gta6gps.com/?p=pin-916983", url_1, NULL, false, false}},
    {204, {"https://gta6gps.com/?p=pin-916984", url_5, NULL, false, false}},
    {212, {"https://gta6gps.com/?p=pin-916968", url_27, NULL, false, false}},
    {215, {"https://gta6gps.com/?p=pin-916932", NULL, NULL, false, false}},
    {219, {"https://gta6gps.com/?p=pin-916993", NULL, NULL, false, false}},
    {400, {url_39, NULL, NULL, true, false}},
    {401, {url_39, NULL, NULL, true, false}},
    {402, {url_39, NULL, NULL, true, false}},
    {403, {url_39, NULL, NULL, true, false}},
    {404, {url_28, NULL, NULL, false, false}},
    {405, {url_39, NULL, NULL, true, false}},
    {406, {url_39, NULL, NULL, true, false}},
    {407, {url_39, NULL, NULL, true, false}},
    {408, {url_39, NULL, NULL, true, false}},
    {409, {url_39, NULL, NULL, true, false}},
    {410, {url_4, NULL, NULL, false, false}},
    {411, {url_39, NULL, NULL, true, false}},
    {412, {url_28, NULL, NULL, false, false}},
    {413, {url_39, NULL, NULL, true, false}},
    {414, {url_28, NULL, NULL, false, false}},
    {415, {url_39, NULL, NULL, true, false}},
    {416, {url_39, NULL, NULL, true, false}},
    {417, {url_28, NULL, NULL, false, false}},
    {418, {url_39, NULL, NULL, true, false}},
    {419, {url_39, NULL, NULL, true, false}},
    {420, {url_28, NULL, NULL, false, false}},
    {421, {url_39, NULL, NULL, true, false}},
    {422, {url_39, NULL, NULL, true, false}},
    {423, {url_39, NULL, NULL, true, false}},
    {424, {url_39, NULL, NULL, true, false}},
    {425, {url_39, NULL, NULL, true, false}},
    {426, {url_28, NULL, NULL, false, false}},
    {427, {url_39, NULL, NULL, true, false}},
    {428, {url_39, NULL, NULL, true, false}},
    {429, {url_39, NULL, NULL, true, false}},
    {430, {url_39, NULL, NULL, true, false}},
    {431, {url_39, NULL, NULL, true, false}},
    {432, {url_28, NULL, NULL, false, false}},
    {433, {url_39, NULL, NULL, true, false}},
    {434, {url_28, NULL, NULL, false, false}},
    {436, {url_39, NULL, NULL, true, false}},
    {437, {url_39, NULL, NULL, true, false}},
    {438, {url_39, NULL, NULL, true, false}},
    {439, {url_39, NULL, NULL, true, false}},
    {440, {url_39, NULL, NULL, true, false}},
    {441, {url_39, NULL, NULL, true, false}},
    {1400, {url_42, url_19, NULL, false, true}},
    {1401, {"https://gta6gps.com/?p=kenton-sets-2", url_42, NULL, false, false}},
    {1402, {url_42, url_19, NULL, false, true}},
    {1403, {url_42, url_19, NULL, false, true}},
    {1404, {url_42, url_19, NULL, false, true}},
    {1405, {url_42, url_19, NULL, false, true}},
    {1406, {url_42, url_19, NULL, false, true}},
    {1416, {url_46, NULL, NULL, false, false}},
    {1417, {url_46, NULL, NULL, false, false}},
    {1418, {"https://gta6gps.com/?p=kenton-sets-2", url_46, NULL, false, false}},
    {1419, {url_46, NULL, NULL, false, false}},
    {1420, {url_46, NULL, NULL, false, false}},
    {1421, {url_46, NULL, NULL, false, false}},
    {1536, {url_2, url_36, NULL, false, true}},
    {1537, {url_2, url_36, NULL, false, true}},
    {1538, {url_2, url_36, NULL, false, true}},
    {1539, {"https://gta6gps.com/?p=pin-917973", url_2, NULL, false, false}},
    {1540, {"https://gta6gps.com/?p=pin-917974", url_2, NULL, false, false}},
    {1541, {"https://gta6gps.com/?p=pin-917975", url_2, NULL, false, false}},
    {1542, {"https://gta6gps.com/?p=pin-917976", NULL, NULL, false, false}},
    {1543, {url_31, url_11, NULL, false, false}},
    {1544, {"https://gta6gps.com/?p=pin-918672", url_31, NULL, false, false}},
    {1545, {url_36, NULL, NULL, true, false}},
    {1546, {url_36, NULL, NULL, true, false}},
    {1547, {url_36, NULL, NULL, true, false}},
    {1548, {url_0, url_36, NULL, false, true}},
    {1549, {"https://gta6gps.com/?p=pin-917977", NULL, NULL, false, false}},
    {1550, {url_0, url_8, NULL, false, false}},
    {1551, {url_0, url_36, NULL, false, true}},
    {1552, {url_36, NULL, NULL, true, false}},
    {1553, {url_36, NULL, NULL, true, false}},
    {1554, {url_36, NULL, NULL, true, false}},
    {1555, {url_36, NULL, NULL, true, false}},
    {1556, {url_36, NULL, NULL, true, false}},
    {1557, {url_36, NULL, NULL, true, false}},
    {1558, {url_31, url_36, NULL, false, true}},
    {1559, {url_36, NULL, NULL, true, false}},
    {1560, {url_36, NULL, NULL, true, false}},
    {1561, {url_36, NULL, NULL, true, false}},
    {1562, {"https://gta6gps.com/?p=pin-918145", NULL, NULL, false, false}},
    {1563, {url_31, url_36, NULL, false, true}},
    {1564, {url_36, NULL, NULL, true, false}},
    {1565, {url_36, NULL, NULL, true, false}},
    {2048, {url_37, NULL, NULL, true, false}},
    {2049, {url_37, NULL, NULL, true, false}},
    {2050, {url_30, url_37, NULL, false, true}},
    {2051, {url_30, url_37, NULL, false, true}},
    {2052, {url_30, url_37, NULL, false, true}},
    {2053, {url_30, url_37, NULL, false, true}},
    {2054, {url_30, url_37, NULL, false, true}},
    {2055, {url_37, NULL, NULL, true, false}},
    {2056, {"https://gta6gps.com/?p=pin-918035", url_26, NULL, false, false}},
    {2057, {url_26, url_37, NULL, false, true}},
    {2058, {url_37, NULL, NULL, true, false}},
    {2059, {url_37, NULL, NULL, true, false}},
    {2060, {url_37, NULL, NULL, true, false}},
    {2061, {url_37, NULL, NULL, true, false}},
    {2062, {url_37, NULL, NULL, true, false}},
    {2063, {url_37, NULL, NULL, true, false}},
    {2064, {url_37, NULL, NULL, true, false}},
    {2065, {url_37, NULL, NULL, true, false}},
    {2066, {url_37, NULL, NULL, true, false}},
    {2067, {url_37, NULL, NULL, true, false}},
    {2068, {url_37, NULL, NULL, true, false}},
    {2069, {url_37, NULL, NULL, true, false}},
    {2070, {url_37, NULL, NULL, true, false}},
    {2071, {url_37, NULL, NULL, true, false}},
    {2072, {url_37, NULL, NULL, true, false}},
    {2073, {url_37, NULL, NULL, true, false}},
    {2412, {url_31, url_36, NULL, false, true}},
    {2413, {url_36, NULL, NULL, true, false}},
    {2414, {url_31, url_36, NULL, false, true}},
    {2415, {url_29, url_36, NULL, false, true}},
    {2416, {url_31, url_36, NULL, false, true}},
    {2417, {url_37, NULL, NULL, true, false}},
    {2418, {url_37, NULL, NULL, true, false}},
    {2419, {url_37, NULL, NULL, true, false}},
    {2420, {url_37, NULL, NULL, true, false}},
    {2421, {url_37, NULL, NULL, true, false}},
    {2493, {url_35, url_38, NULL, false, true}},
    {2512, {url_25, url_37, NULL, false, true}},
    {2513, {url_37, NULL, NULL, true, false}},
    {2514, {url_37, NULL, NULL, true, false}},
    {2515, {url_37, NULL, NULL, true, false}},
    {2516, {url_37, NULL, NULL, true, false}},
    {2517, {url_37, NULL, NULL, true, false}},
    {2518, {url_37, NULL, NULL, true, false}},
    {2519, {url_37, NULL, NULL, true, false}},
    {2520, {url_37, NULL, NULL, true, false}},
    {2521, {url_37, NULL, NULL, true, false}},
    {2522, {url_37, NULL, NULL, true, false}},
    {2523, {url_37, NULL, NULL, true, false}},
    {2524, {url_36, NULL, NULL, true, false}},
    {2525, {url_36, NULL, NULL, true, false}},
    {2526, {"https://gta6gps.com/?p=pin-918458", NULL, NULL, false, false}},
    {2527, {url_36, NULL, NULL, true, false}},
    {2528, {url_9, NULL, NULL, false, false}},
    {2546, {url_7, NULL, NULL, false, false}},
    {3587, {url_37, NULL, NULL, true, false}},
    {3588, {url_37, NULL, NULL, true, false}},
    {3589, {url_37, NULL, NULL, true, false}},
    {3590, {url_37, NULL, NULL, true, false}},
    {3591, {url_37, NULL, NULL, true, false}},
    {3592, {url_37, NULL, NULL, true, false}},
    {3593, {url_37, NULL, NULL, true, false}},
    {3594, {url_37, NULL, NULL, true, false}},
    {3595, {url_37, NULL, NULL, true, false}},
    {3596, {url_37, NULL, NULL, true, false}},
    {3597, {url_37, NULL, NULL, true, false}},
    {3598, {url_37, NULL, NULL, true, false}},
    {3599, {url_37, NULL, NULL, true, false}},
    {3600, {url_37, NULL, NULL, true, false}},
    {3601, {url_37, NULL, NULL, true, false}},
    {3602, {url_0, url_36, NULL, false, true}},
    {3622, {url_39, NULL, NULL, true, false}},
    {3623, {url_39, NULL, NULL, true, false}},
    {3624, {url_39, NULL, NULL, true, false}},
    {3625, {url_39, NULL, NULL, true, false}},
    {3626, {url_39, NULL, NULL, true, false}},
    {3627, {url_39, NULL, NULL, true, false}},
    {3628, {url_39, NULL, NULL, true, false}},
    {3629, {url_39, NULL, NULL, true, false}},
    {3630, {url_39, NULL, NULL, true, false}},
    {3631, {url_39, NULL, NULL, true, false}},
    {3632, {url_39, NULL, NULL, true, false}},
    {3633, {url_39, NULL, NULL, true, false}},
    {3634, {url_39, NULL, NULL, true, false}},
    {3635, {url_28, NULL, NULL, false, false}},
    {3636, {url_39, NULL, NULL, true, false}},
    {3637, {url_39, NULL, NULL, true, false}},
    {3638, {url_39, NULL, NULL, true, false}},
    {3639, {url_39, NULL, NULL, true, false}},
    {3641, {url_6, NULL, NULL, false, false}},
    {3642, {url_6, NULL, NULL, false, false}},
    {3643, {url_6, url_37, NULL, false, true}},
    {3644, {url_6, url_37, NULL, false, true}},
    {3645, {"https://gta6.house/library/items/flamingo-statue", url_36, NULL, false, true}},
    {3646, {url_24, NULL, NULL, false, false}},
    {3647, {url_31, url_36, NULL, false, true}},
    {3648, {url_0, url_36, NULL, false, true}},
    {3649, {"https://cutscenewire.com/games/gta-6/collectibles/gta-6-all-cocaine-flamingo-locations/", url_36, NULL, false, true}},
    {3650, {"https://cutscenewire.com/games/gta-6/collectibles/gta-6-all-cocaine-flamingo-locations/", url_36, NULL, false, true}},
    {3651, {url_37, NULL, NULL, true, false}},
    {3652, {url_37, NULL, NULL, true, false}},
    {3653, {url_37, NULL, NULL, true, false}},
    {3654, {url_37, NULL, NULL, true, false}},
    {3655, {url_37, NULL, NULL, true, false}},
    {3656, {url_37, NULL, NULL, true, false}},
    {3657, {url_37, NULL, NULL, true, false}},
    {3658, {url_37, NULL, NULL, true, false}},
    {3659, {url_37, NULL, NULL, true, false}},
    {3660, {url_37, NULL, NULL, true, false}},
    {3662, {url_21, url_36, NULL, false, true}},
    {3663, {url_31, url_36, NULL, false, true}},
    {3664, {url_36, NULL, NULL, true, false}},
    {3665, {url_36, NULL, NULL, true, false}},
    {3666, {url_36, NULL, NULL, true, false}},
    {3667, {url_36, NULL, NULL, true, false}},
    {3668, {url_36, NULL, NULL, true, false}},
    {3669, {url_36, NULL, NULL, true, false}},
    {3670, {url_36, NULL, NULL, true, false}},
    {3671, {url_37, NULL, NULL, true, false}},
    {3672, {url_37, NULL, NULL, true, false}},
    {3673, {url_45, NULL, NULL, false, false}},
    {3689, {url_28, NULL, NULL, false, false}},
    {3700, {url_39, NULL, NULL, true, false}},
    {3701, {url_39, NULL, NULL, true, false}},
    {3702, {url_39, NULL, NULL, true, false}},
    {3703, {url_39, NULL, NULL, true, false}},
    {3704, {url_39, NULL, NULL, true, false}},
    {3705, {url_39, NULL, NULL, true, false}},
    {3706, {url_39, NULL, NULL, true, false}},
    {3707, {url_39, NULL, NULL, true, false}},
    {3708, {url_39, NULL, NULL, true, false}},
    {3709, {url_39, NULL, NULL, true, false}},
    {3710, {url_39, NULL, NULL, true, false}},
    {3711, {url_39, NULL, NULL, true, false}},
    {3712, {url_39, NULL, NULL, true, false}},
    {3713, {url_39, NULL, NULL, true, false}},
    {3727, {url_39, NULL, NULL, true, false}},
    {3728, {url_3, NULL, NULL, false, false}},
    {3733, {url_41, NULL, NULL, false, false}},
    {3734, {url_37, NULL, NULL, true, false}},
    {3735, {url_37, NULL, NULL, true, false}},
    {3736, {url_37, NULL, NULL, true, false}},
    {3737, {"https://gta6gps.com/?p=pin-918456", url_36, NULL, false, true}},
    {3738, {url_17, NULL, NULL, false, false}},
    {3739, {url_13, NULL, NULL, false, false}},
    {3740, {url_10, NULL, NULL, false, false}},
    {3741, {url_12, NULL, NULL, false, false}},
    {3742, {url_12, NULL, NULL, false, false}},
    {3743, {url_37, NULL, NULL, true, false}},
    {3744, {url_37, NULL, NULL, true, false}},
    {3745, {url_37, NULL, NULL, true, false}},
    {3746, {url_37, NULL, NULL, true, false}},
    {3747, {url_39, NULL, NULL, true, false}},
    {3749, {url_39, NULL, NULL, true, false}},
    {3750, {url_40, NULL, NULL, false, false}},
    {3751, {url_39, NULL, NULL, true, false}},
    {3752, {url_39, NULL, NULL, true, false}},
    {3755, {url_39, NULL, NULL, true, false}},
    {3756, {url_39, NULL, NULL, true, false}},
    {3760, {url_39, NULL, NULL, true, false}},
    {3763, {url_39, NULL, NULL, true, false}},
    {3766, {url_39, NULL, NULL, true, false}},
    {3767, {url_43, NULL, NULL, false, false}},
    {3770, {url_44, NULL, NULL, false, false}},
    {3771, {url_37, NULL, NULL, true, false}},
    {3772, {url_37, NULL, NULL, true, false}},
    {3773, {url_15, NULL, NULL, false, false}},
    {3774, {url_16, NULL, NULL, false, false}},
    {3775, {url_20, url_36, NULL, false, true}},
    {3776, {url_36, NULL, NULL, true, false}},
    {3777, {url_36, NULL, NULL, true, false}},
    {3778, {url_36, NULL, NULL, true, false}},
    {3779, {url_36, NULL, NULL, true, false}},
    {3780, {url_14, NULL, NULL, false, false}},
    {3781, {url_18, NULL, NULL, false, false}},
    {3782, {"https://www.reddit.com/r/grandtheftauto/comments/1y1f2w0/almost_all_gta6_collectibles_discovered_100_maps/", url_36, NULL, false, true}},
    {3783, {url_32, url_34, NULL, false, false}},
    {3784, {url_32, url_33, NULL, false, false}},
    {3785, {url_31, NULL, NULL, true, false}},
    {3786, {url_31, NULL, NULL, true, false}},
    {3787, {url_31, NULL, NULL, true, false}},
    {3788, {url_31, NULL, NULL, true, false}},
    {3789, {url_31, NULL, NULL, true, false}},
    {3790, {url_31, NULL, NULL, true, false}},
    {3791, {url_21, url_22, NULL, false, false}},
    {3792, {url_21, url_23, NULL, false, false}},
    {3793, {flamingo_pin_3793, NULL, NULL, false, false}},
    {3794, {flamingo_pin_3794, NULL, NULL, false, false}},
    {3795, {flamingo_pin_3795, NULL, NULL, false, false}},
    {3796, {flamingo_pin_3796, NULL, NULL, false, false}},
    {3907, {"https://gtamap.tildrix.dev/en/gta-6/hidden-packages", url_36, NULL, true, true}},
    {3908, {"https://gtamap.tildrix.dev/en/gta-6/hidden-packages", url_36, NULL, true, true}},
};

const GtaUrlLinks* gta_url_links_find(uint16_t id) {
    size_t lo = 0, hi = sizeof(gta_links) / sizeof(gta_links[0]);
    while(lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if(gta_links[mid].id < id) lo = mid + 1;
        else hi = mid;
    }
    return lo < sizeof(gta_links) / sizeof(gta_links[0]) && gta_links[lo].id == id ?
               &gta_links[lo].links : NULL;
}

/* Shared guides and indexes for entries without an individual source. */
static const GtaUrlLinks category_ref_0 = {"https://allgtaworld.com/guides/gta-6-legendary-animals", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_1 = {"https://cutscenewire.com/games/gta-6/collectibles/gta-6-all-book-swap-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_2 = {"https://game-checklists.com/gta6/card-sets-checklist/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_3 = {"https://game-checklists.com/gta6/stick-ups-checklist/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_4 = {"https://game-checklists.com/gta6/styles-checklist/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_5 = {"https://gameta.gg/gta6/guides/unique-weapons/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_6 = {"https://gta6-life.com/en/guides/gta-6-graffiti-tags/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_7 = {"https://gta6-life.com/en/guides/gta-6-under-the-bridge-knife-flights/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_8 = {"https://gta6gps.com/map/game/flamingos", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_9 = {"https://gta6gps.com/map/game/murals", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_10 = {"https://gta6gps.com/map/game/playlists", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_11 = {"https://gtagameintel.com/en/interactive-map/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_12 = {"https://itemlevel.net/gta-6-all-6-messages-in-bottles-locations-in-the-shoeless-keys-sunken-boat-mystery/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_13 = {"https://linkleung310.github.io/gtavi-atlas/en/guides/collectibles/meditation-spots.html", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_14 = {"https://metaforge.app/gta-6/gta-6-standout-vehicles-locations", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_15 = {"https://outputlag.com/guides/gta-6-safehouses/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_16 = {"https://gta6gps.com/map/game/wyman-cars", "https://thegameswiki.com/gta6/wiki/wymans-cars", NULL, true, true};
static const GtaUrlLinks category_ref_17 = {"https://www.gtabase.com/gta-6/articles/gta-6-endings-explained", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_18 = {"https://www.gtabase.com/gta-6/articles/how-to-unlock-garages-in-gta-6", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_19 = {"https://www.gtabase.com/gta-6/map/?feature=ahol-street-art", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_20 = {"https://www.gtabase.com/gta-6/map/?feature=luxury-watch", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_21 = {"https://www.gtabase.com/gta-6/map/?feature=meteorite", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_22 = {"https://www.gtabase.com/gta-6/map/?feature=stunt-jumps", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_23 = {"https://www.gtabase.com/gta-6/map/?feature=waink-landmark", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_24 = {"https://www.gtasixmap.com/guides/waink-curiosities", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_25 = {"https://www.gtasixmap.com/guides/waink-sculptures", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_26 = {"https://www.nexzyapp.com/games/grand-theft-auto-vi/guide/kentons-treasures", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_27 = {"https://www.powerpyx.com/gta-6-100-completion-checklist-guide/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_28 = {"https://www.powerpyx.com/gta-6-all-bank-jobs-locations-bank-vaults/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_29 = {"https://www.powerpyx.com/gta-6-all-base-jump-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_30 = {"https://www.powerpyx.com/gta-6-all-challenges-guide/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_31 = {"https://www.powerpyx.com/gta-6-all-golf-course-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_32 = {"https://www.powerpyx.com/gta-6-all-illegal-gang-operations-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_33 = {"https://www.powerpyx.com/gta-6-all-knife-flight-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_34 = {"https://www.powerpyx.com/gta-6-all-magazines-books-comics-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_35 = {"https://www.powerpyx.com/gta-6-all-payday-loans-jobs-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_36 = {"https://www.powerpyx.com/gta-6-all-pool-opponent-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_37 = {"https://www.powerpyx.com/gta-6-all-race-challenge-locations-gold-medals/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_38 = {"https://www.powerpyx.com/gta-6-all-shooting-range-gold-medal-challenges/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_39 = {"https://www.powerpyx.com/gta-6-all-smash-n-grabs-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_40 = {"https://www.powerpyx.com/gta-6-all-strangers-freaks-side-missions/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_41 = {"https://www.powerpyx.com/gta-6-all-top-fun-drone-race-time-trial-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_42 = {"https://www.powerpyx.com/gta-6-all-unique-race-locations-racing-activities/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_43 = {"https://www.powerpyx.com/gta-6-all-video-game-locations/", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_44 = {"https://www.pushsquare.com/guides/gta-6-basketball-hoop-locations", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_45 = {"https://www.pushsquare.com/guides/gta-6-brets-hunting-target-deck-locations", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_46 = {"https://www.pushsquare.com/guides/gta-6-moses-vehicle-drop-off-locations", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_47 = {"https://www.pushsquare.com/guides/gta-6-peewees-billboard-locations-and-grand-prize-puzzle-solution", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_48 = {"https://www.pushsquare.com/guides/gta-6-randys-wish-list-vehicle-locations", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_49 = {"https://www.pushsquare.com/guides/gta-6-underwater-discoveries-locations-and-kentons-treasures", NULL, NULL, true, false};
static const GtaUrlLinks category_ref_50 = {"https://www.videogameschronicle.com/guide/gta-6-serial-killer-missing-persons-locations/", NULL, NULL, true, false};

static const GtaUrlLinks category_fences = {"https://www.gtabase.com/gta-6/articles/gta-6-fence-locations-all-fences-what-they-buy", NULL, NULL, true, false};

static const struct { const char* key; const GtaUrlLinks* links; } gta_category_links[] = {
    {"Garages", &category_ref_18},
    {"Houses / Cribs", &category_ref_15},
    {"Moses Drop-Offs", &category_ref_46},
    {"Randy's Wish List", &category_ref_48},
    {"Standout Vehicles", &category_ref_14},
    {"Wyman's Classic Cars", &category_ref_16},
    {"ahol.tsv", &category_ref_19},
    {"ahol_pieces.tsv", &category_ref_19},
    {"arcade_games.tsv", &category_ref_43},
    {"bank_vaults.tsv", &category_ref_28},
    {"base_jumps.tsv", &category_ref_29},
    {"basketball_hoop_leads.tsv", &category_ref_44},
    {"basketball_hoops.tsv", &category_ref_44},
    {"book_swaps.tsv", &category_ref_1},
    {"bret_target_decks.tsv", &category_ref_45},
    {"bridges_kelly.tsv", &category_ref_7},
    {"bridges_leonard.tsv", &category_ref_7},
    {"bridges_lummox.tsv", &category_ref_7},
    {"bridges_mariana.tsv", &category_ref_7},
    {"bridges_vice_dale.tsv", &category_ref_7},
    {"cards_01.tsv", &category_ref_2},
    {"cards_02.tsv", &category_ref_2},
    {"cards_03.tsv", &category_ref_2},
    {"cards_04.tsv", &category_ref_2},
    {"cards_05.tsv", &category_ref_2},
    {"cards_06.tsv", &category_ref_2},
    {"cards_07.tsv", &category_ref_2},
    {"cards_08.tsv", &category_ref_2},
    {"cards_09.tsv", &category_ref_2},
    {"cards_10.tsv", &category_ref_2},
    {"cards_11.tsv", &category_ref_2},
    {"cards_12.tsv", &category_ref_2},
    {"cards_13.tsv", &category_ref_2},
    {"cards_14.tsv", &category_ref_2},
    {"cards_15.tsv", &category_ref_2},
    {"cards_16.tsv", &category_ref_2},
    {"cards_17.tsv", &category_ref_2},
    {"cards_18.tsv", &category_ref_2},
    {"challenge_milestones.tsv", &category_ref_30},
    {"challenge_objectives.tsv", &category_ref_30},
    {"curiosities.tsv", &category_ref_24},
    {"drone_trials.tsv", &category_ref_41},
    {"endings.tsv", &category_ref_17},
    {"fence_goals.tsv", &category_ref_27},
    {"fences.tsv", &category_fences},
    {"flamingos.tsv", &category_ref_8},
    {"flamingos_inland.tsv", &category_ref_8},
    {"flamingos_north.tsv", &category_ref_8},
    {"gang_operations.tsv", &category_ref_32},
    {"golf_courses.tsv", &category_ref_31},
    {"jumps_kelly.tsv", &category_ref_22},
    {"jumps_leonard.tsv", &category_ref_22},
    {"jumps_lummox.tsv", &category_ref_22},
    {"jumps_mariana.tsv", &category_ref_22},
    {"jumps_vice_dale.tsv", &category_ref_22},
    {"kenton.tsv", &category_ref_26},
    {"knife_flights.tsv", &category_ref_33},
    {"landmarks.tsv", &category_ref_23},
    {"legendary_animals.tsv", &category_ref_0},
    {"location_leads.tsv", &category_ref_11},
    {"meditations.tsv", &category_ref_13},
    {"messages_bottles.tsv", &category_ref_12},
    {"meteorites.tsv", &category_ref_21},
    {"meteorites_mainland.tsv", &category_ref_21},
    {"missing_persons.tsv", &category_ref_50},
    {"murals.tsv", &category_ref_9},
    {"no_flusher.tsv", &category_ref_6},
    {"optional_missions.tsv", &category_ref_40},
    {"payday_loans.tsv", &category_ref_35},
    {"peewees.tsv", &category_ref_47},
    {"pool_opponents.tsv", &category_ref_36},
    {"publications.tsv", &category_ref_34},
    {"race_challenges.tsv", &category_ref_37},
    {"sculptures.tsv", &category_ref_25},
    {"shooting_gold.tsv", &category_ref_38},
    {"small_banks.tsv", &category_ref_28},
    {"smash_grabs.tsv", &category_ref_39},
    {"sniper_posters.tsv", &category_ref_10},
    {"special_weapons.tsv", &category_ref_5},
    {"special_weapons_operations.tsv", &category_ref_32},
    {"special_weapons_rewards.tsv", &category_ref_5},
    {"stickup_leads.tsv", &category_ref_3},
    {"styles_jason.tsv", &category_ref_4},
    {"styles_lucia.tsv", &category_ref_4},
    {"tattoos.tsv", &category_ref_4},
    {"underwater.tsv", &category_ref_49},
    {"unique_race_hubs.tsv", &category_ref_42},
    {"vault_poster.tsv", &category_ref_10},
    {"watches.tsv", &category_ref_20},
};

const GtaUrlLinks* gta_url_links_resolve(uint16_t id, const char* asset, const char* title) {
    const GtaUrlLinks* specific = gta_url_links_find(id);
    if(specific) return specific;
    const char* key = asset ? asset : title;
    if(!key) return NULL;
    for(size_t i = 0; i < sizeof(gta_category_links) / sizeof(gta_category_links[0]); ++i) {
        if(strcmp(key, gta_category_links[i].key) == 0) return gta_category_links[i].links;
    }
    return NULL;
}

GtaUrlChoice gta_url_links_choose(const GtaUrlLinks* links, bool right) {
    if(!links) return (GtaUrlChoice){0};
    if(!right) {
        if(links->source)
            return (GtaUrlChoice){links->source, "Source", GtaUrlSource, links->source_index};
    } else if(links->map_image) {
        return (GtaUrlChoice){links->map_image, "Map", GtaUrlMapImage, false};
    } else if(links->source2) {
        return (GtaUrlChoice){links->source2, "Source 2", GtaUrlSource2, links->source2_index};
    }
    return (GtaUrlChoice){0};
}

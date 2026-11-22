#include "exploration_catalog.h"

#define GTA_COUNT(items) (sizeof(items) / sizeof((items)[0]))
#define GTA_LEAF(title, file, total, checked_label) \
    {title, file ".tsv", checked_label, total, NULL, 0}
#define GTA_GROUP(title, items) {title, NULL, NULL, 0, items, GTA_COUNT(items)}

static const GtaExploreSection card_sets[] = {
    GTA_LEAF("Vice City Flamingos", "cards_01", 24, "Collected"),
    GTA_LEAF("Vice City Narcos", "cards_02", 24, "Collected"),
    GTA_LEAF("Vice City Manatees", "cards_03", 24, "Collected"),
    GTA_LEAF("Vice City Club De Futbol", "cards_04", 30, "Collected"),
    GTA_LEAF("Liberty City Swingers", "cards_05", 24, "Collected"),
    GTA_LEAF("Liberty City Penetrators", "cards_06", 24, "Collected"),
    GTA_LEAF("Liberty City Wrath", "cards_07", 24, "Collected"),
    GTA_LEAF("Los Santos Corkers", "cards_08", 24, "Collected"),
    GTA_LEAF("Los Santos Panic", "cards_09", 24, "Collected"),
    GTA_LEAF("Los Santos Pounders", "cards_10", 24, "Collected"),
    GTA_LEAF("Backyard Wrestling", "cards_11", 12, "Collected"),
    GTA_LEAF("America's Next Top Hooker", "cards_12", 24, "Collected"),
    GTA_LEAF("Delights Dancers", "cards_13", 18, "Collected"),
    GTA_LEAF("Jack of Hearts Dancers", "cards_14", 36, "Collected"),
    GTA_LEAF("Nine1Nine Dancers", "cards_15", 36, "Collected"),
    GTA_LEAF("The Legends of Ratface", "cards_16", 36, "Collected"),
    GTA_LEAF("Right Wing: America's Hero", "cards_17", 36, "Collected"),
    GTA_LEAF("ED Comics Universe", "cards_18", 36, "Collected"),
};

static const GtaExploreSection waink[] = {
    GTA_LEAF("Landmarks", "landmarks", 31, "Scanned"),
    GTA_LEAF("Curiosities", "curiosities", 35, "Scanned"),
    GTA_LEAF("Sculptures", "sculptures", 23, "Scanned"),
    GTA_LEAF("No Flusher Tags", "no_flusher", 0, "Scanned"),
    GTA_LEAF("Peewees Billboards", "peewees", 4, "Scanned"),
};

static const GtaExploreSection ahol[] = {
    GTA_LEAF("All 112 Pieces", "ahol_pieces", 112, "Scanned"),
    GTA_LEAF("Type Progress", "ahol", 11, "Type complete"),
};

static const GtaExploreSection playlists[] = {
    GTA_LEAF("The Sniper of La Perle", "sniper_posters", 4, "Scanned"),
    GTA_LEAF("The Vault", "vault_poster", 1, "Scanned"),
};

static const GtaExploreSection styles[] = {
    GTA_LEAF("Jason's Magazine Styles", "styles_jason", 13, "Discovered"),
    GTA_LEAF("Lucia's Magazine Styles", "styles_lucia", 15, "Discovered"),
    GTA_LEAF("Magazine Tattoos", "tattoos", 17, "Discovered"),
    GTA_LEAF("Luxury Watches", "watches", 34, "Collected"),
};

static const GtaExploreSection stunt_jumps[] = {
    GTA_LEAF("Vice-Dale County", "jumps_vice_dale", 54, "Completed"),
    GTA_LEAF("Mariana County", "jumps_mariana", 17, "Completed"),
    GTA_LEAF("Kelly County", "jumps_kelly", 31, "Completed"),
    GTA_LEAF("Leonard County", "jumps_leonard", 12, "Completed"),
    GTA_LEAF("Lummox County", "jumps_lummox", 16, "Completed"),
};

static const GtaExploreSection bridges[] = {
    GTA_LEAF("Vice-Dale County", "bridges_vice_dale", 59, "Completed"),
    GTA_LEAF("Mariana County", "bridges_mariana", 39, "Completed"),
    GTA_LEAF("Kelly County", "bridges_kelly", 52, "Completed"),
    GTA_LEAF("Leonard County", "bridges_leonard", 35, "Completed"),
    GTA_LEAF("Lummox County", "bridges_lummox", 17, "Completed"),
};

const GtaExploreSection gta_explore_sections[] = {
    GTA_GROUP("Collectible Cards", card_sets),
    GTA_GROUP("WAiNK Locations", waink),
    GTA_GROUP("AHOL Street Art", ahol),
    GTA_LEAF("1010 Murals", "murals", 5, "Scanned"),
    GTA_LEAF("Kenton's Treasures", "kenton", 7, "Set complete"),
    GTA_LEAF("Underwater Discoveries", "underwater", 33, "Discovered"),
    GTA_LEAF("Magazines, Books & Comics", "publications", 27, "Read"),
    GTA_LEAF("Book Swaps", "book_swaps", 7, "Swapped"),
    GTA_LEAF("Meditations", "meditations", 18, "Completed"),
    GTA_LEAF("Cocaine Flamingos", "flamingos", 125, "Destroyed"),
    GTA_GROUP("Unique Playlists", playlists),
    GTA_GROUP("Discovered Styles", styles),
    GTA_LEAF("Special Weapons", "special_weapons", 25, "Acquired"),
    GTA_GROUP("Stunt Jumps", stunt_jumps),
    GTA_GROUP("Under the Bridge", bridges),
    GTA_LEAF("Knife Flights", "knife_flights", 40, "Completed"),
    GTA_LEAF("Base Jumps", "base_jumps", 25, "Completed"),
};

const size_t gta_explore_section_count = GTA_COUNT(gta_explore_sections);

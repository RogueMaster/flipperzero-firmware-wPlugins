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
    GTA_LEAF("Random Card Pickups", "card_pickups", 0, "Checked"),
};
static const GtaExploreSection waink[] = {
    GTA_LEAF("Landmarks", "landmarks", 31, "Scanned"),
    GTA_LEAF("Curiosities", "curiosities", 35, "Scanned"),
    GTA_LEAF("Sculptures", "sculptures", 23, "Scanned"),
    GTA_LEAF("No Flusher Tags", "no_flusher", 15, "Scanned"),
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
static const GtaExploreSection activity_robberies[] = {
    GTA_LEAF("Gang Operations", "gang_operations", 39, "Cleared"),
    GTA_LEAF("Small Bank Jobs", "small_banks", 14, "Robbed"),
    GTA_LEAF("Large Bank Vaults", "bank_vaults", 8, "Hit"),
    GTA_LEAF("Payday Loan Jobs", "payday_loans", 7, "Robbed"),
    GTA_LEAF("Fence Sale Milestones", "fence_goals", 4, "Reached"),
    GTA_LEAF("Challenge Milestones", "challenge_milestones", 19, "Reached"),
    GTA_LEAF("Challenge Objectives", "challenge_objectives", 80, "Completed"),
    GTA_LEAF("Bret's Target Decks", "bret_target_decks", 15, "Completed"),
    GTA_LEAF("Legendary Animals", "legendary_animals", 11, "Logged"),
    GTA_LEAF("Smash and Grabs", "smash_grabs", 35, "Robbed"),
    GTA_LEAF("Stick-Up Store Leads", "stickup_leads", 0, "Checked"),
    GTA_LEAF("Kitty Litter Labs", "kitty_litter_labs", 0, "Closed"),
    GTA_LEAF("Drone Time Trials", "drone_trials", 42, "Completed"),
    GTA_LEAF("Shooting Gold Medals", "shooting_gold", 42, "Gold"),
    GTA_LEAF("Race Challenges", "race_challenges", 13, "Gold"),
    GTA_LEAF("Arcade Video Games", "arcade_games", 4, "Played"),
    GTA_LEAF("Pool Opponents", "pool_opponents", 7, "Beaten"),
    GTA_LEAF("Golf Courses", "golf_courses", 5, "Played"),
    GTA_LEAF("Unique Race Venues", "unique_race_hubs", 13, "Visited"),
    GTA_LEAF("Basketball Hoops", "basketball_gps", 100, "Scored"),
};
static const GtaExploreSection special_weapons[] = {
    GTA_LEAF("World Finds & Leads", "special_weapons", 49, "Acquired"),
    GTA_LEAF("Operation Weapons", "special_weapons_operations", 37, "Acquired"),
    GTA_LEAF("Shops & Rewards", "special_weapons_rewards", 51, "Acquired"),
};
static const GtaExploreSection meteorite_chunks[] = {
    GTA_LEAF("Keys, Coast & Islands", "meteorites", 0, "Found"),
    GTA_LEAF("Mainland & Caves", "meteorites_mainland", 0, "Found"),
};
static const GtaExploreSection cocaine_flamingos[] = {
    GTA_LEAF("Keys & South Coast", "flamingos", 0, "Destroyed"),
    GTA_LEAF("Vice City & Inland", "flamingos_inland", 0, "Destroyed"),
    GTA_LEAF("Northern Leonida", "flamingos_north", 0, "Destroyed"),
};
const GtaExploreSection gta_explore_sections[] = {
    GTA_GROUP("Special Weapons", special_weapons),
    GTA_LEAF("1010 Murals", "murals", 5, "Scanned"),
    GTA_GROUP("Activities / Robberies", activity_robberies),
    GTA_GROUP("AHOL Street Art", ahol),
    GTA_LEAF("Base Jumps", "base_jumps", 25, "Completed"),
    GTA_LEAF("Book Swaps", "book_swaps", 7, "Swapped"),
    GTA_GROUP("Cocaine Flamingos", cocaine_flamingos),
    GTA_GROUP("Collectible Cards", card_sets),
    GTA_GROUP("Discovered Styles", styles),
    GTA_LEAF("Easter Eggs", "easter_eggs", 0, "Visited"),
    GTA_LEAF("Endings", "endings", 12, "Seen"),
    GTA_LEAF("Fences", "fences", 7, "Unlocked"),
    GTA_LEAF("Kenton's Treasures", "kenton", 7, "Set complete"),
    GTA_LEAF("Knife Flights", "knife_flights", 40, "Completed"),
    GTA_LEAF("Location Search Leads", "location_leads", 0, "Checked"),
    GTA_LEAF("Magazines, Books & Comics", "publications", 27, "Read"),
    GTA_LEAF("Meditations", "meditations", 18, "Completed"),
    GTA_LEAF("Messages in Bottles", "messages_bottles", 6, "Read"),
    GTA_GROUP("Meteorite Chunks (known)", meteorite_chunks),
    GTA_LEAF("Missing Persons (5 steps)", "missing_persons", 5, "Investigated"),
    GTA_LEAF("Optional Missions", "optional_missions", 43, "Completed"),
    GTA_GROUP("Stunt Jumps", stunt_jumps),
    GTA_GROUP("Under the Bridge", bridges),
    GTA_LEAF("Underwater Discoveries", "underwater", 33, "Discovered"),
    GTA_LEAF("Vice City Portal Route", "vice_city_portal", 14, "Completed"),
    GTA_GROUP("Unique Playlists", playlists),
    GTA_GROUP("WAiNK Locations", waink),
};
const size_t gta_explore_section_count = GTA_COUNT(gta_explore_sections);

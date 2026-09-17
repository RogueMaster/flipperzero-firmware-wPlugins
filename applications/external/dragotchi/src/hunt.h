#ifndef __hunt_h__
#define __hunt_h__
#include <stddef.h>
#include "game_structs.h"
/* Cooldown gate. */
bool forage_ready(const struct GameState *, uint32_t now);
/* Seconds left on the forage cooldown (0 when ready). */
uint32_t forage_cooldown_remaining(const struct GameState *, uint32_t now);
/* Roll a catch given RF activity (0-100) and the hottest band. Uses global RNG. */
struct Catch catch_roll(uint8_t activity, uint8_t band);
/* Apply a catch's effects and stamp the forage cooldown. */
void apply_catch(struct GameState *, struct Catch, uint32_t now);
const char *band_name(uint8_t band);           /* "315".."915" */
int catch_describe(struct Catch, char *buf, size_t n);
/* WiFi devboard enrichment: nearby AP count raises effective activity (a
 * "signal storm"). Returns base_activity + capped bonus, clamped 0..100. */
uint8_t signal_storm_activity(uint8_t base_activity, uint8_t wifi_count);
/* True when the AP count is dense enough to flag as a signal storm. */
bool signal_storm_active(uint8_t wifi_count);
/* Board-assisted catch: applies the WiFi activity boost, a guaranteed floor
 * (a strong storm never yields a small-prey dud), and a chance at a
 * board-exclusive "storm egg" (Catch.tier == 2 for eggs). Uses global RNG. */
struct Catch storm_catch_roll(uint8_t base_activity, uint8_t band, uint8_t wifi_count);
/* Describe a storm catch with WiFi-themed flavour (for the Signal Storm screen). */
int catch_describe_storm(struct Catch, char *buf, size_t n);
#endif

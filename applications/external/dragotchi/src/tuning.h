#ifndef __tuning_h__
#define __tuning_h__
/* All game-balance constants. Host-safe: no device headers.
 * Two profiles: NORMAL (real timescale) and DEBUG (compressed, for testing).
 * DEBUG build: ufbt --extra-define=DEBUG launch APPSRC=dragotchi */
#include <stdint.h>

/* Meters */
#define MAX_HU 100
#define MAX_HAPPINESS 100
#define MAX_HP 100

/* Care score */
#define CARE_START 50
#define CARE_MIN 0
#define CARE_MAX 100
#define CARE_WHITE 66   // >= -> white dragon
#define CARE_GREY 33    // >= -> grey; below -> black
#define CARE_IMMORTAL 90 // >= while adult -> never dies of old age

/* Care reward/penalty magnitudes */
#define CARE_FEED_HUNGRY 3
#define CARE_PLAY_SAD 3
#define CARE_CLEAN 4
#define CARE_HEAL 4
#define CARE_LIGHTS_OUT 2
#define CARE_SCOLD_CORRECT 3
#define CARE_METER_ZERO 6
#define CARE_POOP_IGNORED 5
#define CARE_SICK_IGNORED 6
#define CARE_SLEEP_DISTURBED 3
#define CARE_OVERFEED 2
#define CARE_SCOLD_WRONG 3

/* Poop / sickness caps */
#define MAX_POOP 3

/* Sleep window (RTC hours) */
#define NIGHT_START 20
#define NIGHT_END 8

#ifdef DEBUG
/* -------- compressed timescale (seconds) -------- */
#define HU_DECAY_FREQ 3
#define HAP_DECAY_FREQ 4
#define HP_CHECK_FREQ 3
#define POOP_FREQ 6
#define SICK_CHECK_FREQ 8
#define OLD_AGE_CHECK_FREQ 5
#define POOP_TOLERANCE 10
#define SICK_TOLERANCE 12
#define AGE_HATCHLING 5
#define AGE_WYRMLING 30
#define AGE_DRAKE 60
#define AGE_ADULT 120
#define AGE_ELDER 300
#else
/* -------- normal timescale (seconds) -------- */
#define HU_DECAY_FREQ 600      // 10 min
#define HAP_DECAY_FREQ 900     // 15 min
#define HP_CHECK_FREQ 300      // 5 min
#define POOP_FREQ 3600         // ~ a few per day
#define SICK_CHECK_FREQ 1800   // 30 min
#define OLD_AGE_CHECK_FREQ 3600
#define POOP_TOLERANCE 3600    // 1h uncleaned = mistake
#define SICK_TOLERANCE 3600    // 1h untreated = mistake
#define AGE_HATCHLING 1800     // 30 min: egg hatches
#define AGE_WYRMLING 86400     // day 1
#define AGE_DRAKE 216000       // day 2.5
#define AGE_ADULT 388800       // day 4.5
#define AGE_ELDER 864000       // day 10: earliest old-age death
#endif

/* Probabilities (0..100) and magnitudes (shared across profiles) */
#define HU_DECAY_PROB 40
#define HU_DECAY_MIN 1
#define HU_DECAY_MAX 4
#define FEED_MIN 20
#define FEED_MAX 40

#define HAP_DECAY_PROB 40
#define HAP_DECAY_MIN 1
#define HAP_DECAY_MAX 4
#define PLAY_MIN 20
#define PLAY_MAX 40

#define HP_DRAIN_MIN 1
#define HP_DRAIN_MAX 5
#define MEDICINE_MIN 30
#define MEDICINE_MAX 60

#define POOP_PROB 50

#define SICK_BASE_PROB 5
#define SICK_HUNGRY_BONUS 20
#define SICK_DIRTY_BONUS 10

/* Old-age death: per-check probability scales with how far below the
 * immortality bar the care score sits. */
#define OLD_AGE_BASE_PROB 8

/* Discipline / attention calls */
#define DISCIPLINE_START 50
#define ATTENTION_CALL_FREQ 1200
#define ATTENTION_CALL_BASE_PROB 10
#define DISCIPLINE_SCOLD_GAIN 8
#define DISCIPLINE_SPOIL_LOSS 5

/* ---- v0.2 Hunt ---- */
#define HUNT_BANDS 4               /* 315, 433, 868, 915 MHz (indices 0..3) */
#ifdef DEBUG
#define FORAGE_COOLDOWN 5
#else
#define FORAGE_COOLDOWN 90         /* 90s */
#endif
#define PREY_PCT_LO 70
#define PREY_PCT_HI 45
#define TREASURE_PCT_LO 25
#define TREASURE_PCT_HI 40
#define PREY_FOOD_SMALL 15
#define PREY_FOOD_MED   30
#define PREY_FOOD_LARGE 45
#define TREASURE_SMALL 5
#define TREASURE_MED   12
#define TREASURE_LARGE 25

/* ---- ESP32 WiFi devboard "signal storm" enrichment (optional hardware) ----
 * When the WiFi devboard is attached and reporting, nearby access-point
 * density raises the effective hunt activity (busier air -> richer catches).
 * Receive-only; no scanning of our own. */
#define STORM_PER_AP   3    /* activity points added per nearby AP */
#define STORM_MAX_BONUS 40  /* cap on the added activity */
#define STORM_MIN_APS  4    /* >= this many APs is flagged a "signal storm" */
#define ESP_LINK_BAUD  115200
#define ESP_PROBE_MS   1500 /* how long to listen for a DRAGO report */
#define STORM_FLOOR_APS 8   /* >= this many APs: no small-prey duds (guaranteed floor) */
#define STORM_EGG_BASE  4   /* base %% chance a storm egg replaces a rolled egg */
#define STORM_EGG_MAX   55  /* cap on storm-egg promotion chance */

/* ---- v0.2 Economy ---- */
#define RANK2_MIN 50
#define RANK3_MIN 200
#define RANK4_MIN 600
#define RANK5_MIN 1500
#define HEIR_STORM_CARE 75
#define HEIR_RARE_CARE 65
#define HEIR_COMMON_CARE 55

/* ---- v0.3 Expeditions ---- */
#ifdef DEBUG
#define EXPED_SEC_PER_MIN 1
#else
#define EXPED_SEC_PER_MIN 60
#endif
#define EXPED_SHORT_MIN 30
#define EXPED_LONG_MIN 120
#define EXPED_EPIC_MIN 480
#define EXPED_FINDS_SHORT 2
#define EXPED_FINDS_LONG 5
#define EXPED_FINDS_EPIC 12
#define EXPED_ACT_SHORT 40
#define EXPED_ACT_LONG 60
#define EXPED_ACT_EPIC 85
#define EXPED_RISK_SHORT 5
#define EXPED_RISK_LONG 15
#define EXPED_RISK_EPIC 30
#define EXPED_HURT_MIN 5
#define EXPED_HURT_MAX 20

#endif

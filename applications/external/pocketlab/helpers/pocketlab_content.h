#pragma once

#include <stddef.h>
#include <stdint.h>

#define POCKETLAB_QUIZ_OPTIONS    3
#define POCKETLAB_MAX_DISTRACTORS 6

typedef enum {
    PocketLabStepText,
    PocketLabStepQuiz,
    PocketLabStepTry,
    PocketLabStepReward,
} PocketLabStepType;

typedef enum {
    PocketLabTrackBasics,
    PocketLabTrackInfrared,
    PocketLabTrackSubGhz,
    PocketLabTrackRfid,
    PocketLabTrackNfc,
    PocketLabTrackIbutton,
    PocketLabTrackUsb,
    PocketLabTrackGpio,
    PocketLabTrackBluetooth,
    PocketLabTrackSecurity,
    PocketLabTrackSystem,
    PocketLabTrackCount,
} PocketLabTrack;

const char* pocketlab_track_name(PocketLabTrack track);

typedef struct {
    PocketLabStepType type;
    const char* title;
    const char* body;
    // Quiz: one correct answer shown with two distractors picked at random.
    const char* correct;
    const char* distractors[POCKETLAB_MAX_DISTRACTORS];
    uint8_t distractor_count;
    const char* feedback_ok;
    const char* feedback_no;
} PocketLabStep;

typedef struct {
    const char* id;
    const char* title;
    const char* badge;
    PocketLabTrack track;
    uint16_t xp;
    // 10x10 topic glyph (one bit per column) shown next to the lesson title.
    uint16_t icon[10];
    const PocketLabStep* steps;
    size_t step_count;
} PocketLabLab;

extern const PocketLabLab pocketlab_labs[];
extern const size_t pocketlab_labs_count;

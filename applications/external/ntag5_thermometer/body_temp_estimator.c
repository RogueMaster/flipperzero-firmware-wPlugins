#include "body_temp_estimator.h"

#include <furi.h>

/* Each segment covers [from, from + 1) degrees: estimate = slope * skin + offset */
typedef struct {
    float from;
    float slope;
    float offset;
} BodyTempSegment;

static const BodyTempSegment body_temp_arm[] = {
    {31.0f, -0.143f, 41.6f},
    {32.0f, -0.188f, 42.9f},
    {33.0f, -0.122f, 41.1f},
    {34.0f, -0.137f, 41.6f},
    {35.0f, -0.112f, 40.9f},
    {36.0f, -0.313f, 48.4f},
};

static const BodyTempSegment body_temp_chest[] = {
    {32.0f, 2.03f, -29.5f},
    {33.0f, -0.104f, 40.9f},
    {34.0f, -0.139f, 41.8f},
    {35.0f, 0.511f, 19.1f},
    {36.0f, 0.383f, 23.0f},
};

static const char* const body_temp_location_names[BodyTempLocationCount] = {
    [BodyTempLocationArm] = "Arm",
    [BodyTempLocationChest] = "Chest",
    [BodyTempLocationAbdomen] = "Abdomen",
    [BodyTempLocationCustom] = "Custom",
};

const char* body_temp_location_name(BodyTempLocation location) {
    furi_check(location < BodyTempLocationCount);
    return body_temp_location_names[location];
}

static float body_temp_apply(float skin, const BodyTempSegment* segments, size_t count) {
    for(size_t i = 0; i < count; i++) {
        if(skin >= segments[i].from && skin < segments[i].from + 1.0f) {
            return segments[i].slope * skin + segments[i].offset;
        }
    }

    return skin;
}

float body_temp_estimate(float skin, BodyTempLocation location) {
    if(location == BodyTempLocationChest) {
        return body_temp_apply(skin, body_temp_chest, COUNT_OF(body_temp_chest));
    }

    return body_temp_apply(skin, body_temp_arm, COUNT_OF(body_temp_arm));
}

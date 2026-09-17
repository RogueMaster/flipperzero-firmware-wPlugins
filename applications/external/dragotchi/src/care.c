#include "care.h"
#include "tuning.h"
static int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
void care_reward(struct PersistentGameState *p, int amount) {
    p->care_score = clampi(p->care_score + amount, CARE_MIN, CARE_MAX);
}
void care_penalty(struct PersistentGameState *p, int amount) {
    p->care_score = clampi(p->care_score - amount, CARE_MIN, CARE_MAX);
}
enum DragonAlignment care_band(int32_t care_score) {
    if(care_score >= CARE_WHITE) return ALIGN_WHITE;
    if(care_score >= CARE_GREY) return ALIGN_GREY;
    return ALIGN_BLACK;
}

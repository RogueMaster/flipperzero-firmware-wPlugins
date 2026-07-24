#include "morse_flipper_icr.h"
#include "morse_flipper_icr_api.h"
#include "../../cw.h"

#include <stdlib.h>

#define ICR_WAIT_MS 1000U
#define ICR_TIMEOUT_MS 5000U
#define ICR_GUARD_MS 100U
#define ICR_RESULT_MS 1000U
#define ICR_FLASH_STEP_MS 250U
#define ICR_FLASH_MS (ICR_FLASH_STEP_MS * 5U)
#define ICR_1000MS_BUCKET 50U

typedef enum { IcrGraphWait, IcrPlayback, IcrRecognition, IcrRecognizedHold, IcrAnswerGuard, IcrAnswer, IcrResult } IcrPhase;
typedef struct {
    MorseFlipperIcrStats stats;
    bool dirty;
    IcrPhase phase;
    uint8_t target, choice, choices[MORSE_FLIPPER_ICR_CHOICE_COUNT], mark_idx;
    uint32_t rng, next_at, reaction_started_at, pending_reaction_ms, guard_until, result_until;
    bool answer_correct, playback_mark;
    MorseFlipperIcrFeedback feedback;
} MorseFlipperIcrState;

static MorseFlipperIcrResult result(MorseFlipperIcrState* s, bool redraw) {
    return (MorseFlipperIcrResult){.handled = true, .redraw = redraw,
        .playback_active = s->phase == IcrPlayback, .playback_mark = s->playback_mark,
        .prompt_visible = s->phase == IcrResult, .prompt_char = morse_flipper_icr_char_at(s->target),
        .feedback = s->feedback};
}
static uint16_t dit_ms(void) { return 48U; }
static void wait_state(MorseFlipperIcrState* s, uint32_t now) {
    s->playback_mark=false; s->phase=IcrGraphWait; s->choice=MORSE_FLIPPER_ICR_NO_CHOICE; s->mark_idx=0;
    s->next_at=now + (s->target < MORSE_FLIPPER_ICR_CHAR_COUNT ? ICR_FLASH_MS : ICR_WAIT_MS);
    s->guard_until=s->target < MORSE_FLIPPER_ICR_CHAR_COUNT ? now + ICR_FLASH_STEP_MS : 0;
    s->reaction_started_at=s->pending_reaction_ms=s->result_until=0; s->answer_correct=false;
}
static void prompt(MorseFlipperIcrState* s, uint32_t now) {
    uint8_t previous=s->target < MORSE_FLIPPER_ICR_CHAR_COUNT ? s->target : MORSE_FLIPPER_ICR_NO_CHOICE;
    s->target=morse_flipper_icr_pick_target_except(&s->stats,&s->rng,previous); s->choice=MORSE_FLIPPER_ICR_NO_CHOICE;
    s->mark_idx=0;s->playback_mark=false;s->pending_reaction_ms=s->reaction_started_at=0;s->phase=IcrPlayback;s->next_at=now;
}
static void timeout(MorseFlipperIcrState* s,uint32_t now) {
    morse_flipper_icr_note_answer(&s->stats,s->target,MORSE_FLIPPER_ICR_NO_CHOICE,ICR_TIMEOUT_MS);s->dirty=true;
    s->choice=MORSE_FLIPPER_ICR_NO_CHOICE;s->answer_correct=false;s->phase=IcrResult;s->result_until=now+ICR_RESULT_MS;s->feedback=MorseFlipperIcrFeedbackTimeout;
}
static void answer(MorseFlipperIcrState* s,uint8_t pos,uint32_t now) {
    if(pos>=MORSE_FLIPPER_ICR_CHOICE_COUNT || s->choices[pos]>=MORSE_FLIPPER_ICR_CHAR_COUNT)return;
    s->choice=s->choices[pos];s->answer_correct=s->choice==s->target;
    morse_flipper_icr_note_answer(&s->stats,s->target,s->choice,s->pending_reaction_ms);s->dirty=true;s->phase=IcrResult;s->result_until=now+ICR_RESULT_MS;
    s->feedback=s->answer_correct?MorseFlipperIcrFeedbackGood:MorseFlipperIcrFeedbackFail;
}
static uint8_t key_choice(InputKey key) { switch(key) {case InputKeyUp:return 0;case InputKeyDown:return 1;case InputKeyLeft:return 2;case InputKeyRight:return 3;case InputKeyOk:return 4;default:return MORSE_FLIPPER_ICR_NO_CHOICE;} }

void* morse_flipper_icr_runtime_alloc(void) { return calloc(1,sizeof(MorseFlipperIcrState)); }
void morse_flipper_icr_runtime_free(void* value) { free(value); }
bool morse_flipper_icr_runtime_enter(void* value,const MorseFlipperIcrEnterArgs* args,MorseFlipperIcrResult* initial) {
    MorseFlipperIcrState* s=value;if(!s||!args||!initial||!morse_flipper_icr_stats_load(&s->stats))return false;
    s->rng=args->rng_seed?args->rng_seed:(args->now_ms^0x49435231UL);s->target=MORSE_FLIPPER_ICR_NO_CHOICE;s->feedback=MorseFlipperIcrFeedbackClear;wait_state(s,args->now_ms);*initial=result(s,true);return true;
}
void morse_flipper_icr_runtime_leave(void* value) { MorseFlipperIcrState* s=value;if(s&&s->dirty)morse_flipper_icr_stats_save(&s->stats); }
MorseFlipperIcrResult morse_flipper_icr_runtime_input(void* value,const InputEvent* event,uint32_t now) {
    MorseFlipperIcrState* s=value;if(!s||!event)return (MorseFlipperIcrResult){0};s->feedback=MorseFlipperIcrFeedbackNone;
    if(event->key==InputKeyBack&&(event->type==InputTypeShort||event->type==InputTypeLong)){MorseFlipperIcrResult r=result(s,false);r.request_back=true;return r;}
    if(s->phase==IcrRecognition&&event->key==InputKeyOk&&event->type==InputTypePress){s->pending_reaction_ms=s->reaction_started_at?now-s->reaction_started_at:0;s->phase=IcrRecognizedHold;return result(s,true);}
    if(s->phase==IcrRecognizedHold&&event->key==InputKeyOk&&event->type==InputTypeRelease){morse_flipper_icr_build_choices(&s->stats,s->target,&s->rng,s->choices);s->phase=IcrAnswerGuard;s->guard_until=now+ICR_GUARD_MS;return result(s,true);}
    if(s->phase==IcrAnswer&&event->type==InputTypeRelease){answer(s,key_choice(event->key),now);return result(s,true);}return result(s,false);
}
MorseFlipperIcrResult morse_flipper_icr_runtime_tick(void* value,uint32_t now) {
    MorseFlipperIcrState* s=value;if(!s)return (MorseFlipperIcrResult){0};s->feedback=MorseFlipperIcrFeedbackNone;
    if(s->phase==IcrGraphWait){if(now>=s->next_at){prompt(s,now);return result(s,true);}if(s->target<MORSE_FLIPPER_ICR_CHAR_COUNT&&s->guard_until&&now>=s->guard_until){do{s->guard_until+=ICR_FLASH_STEP_MS;}while(s->guard_until<=now&&s->guard_until<s->next_at);return result(s,true);}}
    else if(s->phase==IcrPlayback&&now>=s->next_at){uint8_t code=cw(morse_flipper_icr_char_at(s->target)),marks=cw_symbol_count(code);if(!marks){wait_state(s,now);return result(s,true);}if(s->playback_mark){s->playback_mark=false;if(s->mark_idx+1<marks){s->mark_idx++;s->next_at=now+dit_ms();}else{s->phase=IcrRecognition;s->reaction_started_at=now;s->next_at=now+ICR_TIMEOUT_MS;}}else{s->playback_mark=true;s->next_at=now+dit_ms()*cw_symbol_units(code,s->mark_idx);}return result(s,true);}
    else if(s->phase==IcrRecognition&&now>=s->next_at){timeout(s,now);return result(s,true);}
    else if(s->phase==IcrAnswerGuard&&now>=s->guard_until){s->phase=IcrAnswer;return result(s,true);}
    else if(s->phase==IcrResult&&now>=s->result_until){s->feedback=MorseFlipperIcrFeedbackClear;wait_state(s,now);return result(s,true);}
    return result(s,false);
}
static const char* phase_label(const MorseFlipperIcrState* state) {
    switch(state->phase) {
    case IcrGraphWait: return "Wait";
    case IcrPlayback: return "Listen";
    case IcrRecognition: return "React";
    case IcrRecognizedHold: return "Hold";
    case IcrResult: return state->answer_correct ? "OK" : "Fail";
    default: return "";
    }
}
static uint8_t scale_bar(uint8_t b,uint8_t fast,uint8_t slow,uint8_t tall,uint8_t short_h) {
    if(b <= fast) return tall;
    if(b >= slow) return short_h;
    return (uint8_t)(short_h+((uint16_t)(slow-b)*(tall-short_h))/(slow-fast));
}
static uint8_t bar(uint8_t b) {
    if(!b)return 0;
    if(b<=MORSE_FLIPPER_ICR_INSTANT_BUCKET)return scale_bar(b,1U,MORSE_FLIPPER_ICR_INSTANT_BUCKET,44U,34U);
    if(b<=ICR_1000MS_BUCKET)return scale_bar(b,MORSE_FLIPPER_ICR_INSTANT_BUCKET,ICR_1000MS_BUCKET,33U,12U);
    return scale_bar(b,ICR_1000MS_BUCKET,MORSE_FLIPPER_ICR_TIMEOUT_BUCKET,11U,4U);
}
static void draw_bar(Canvas* canvas,uint8_t x,uint8_t base,uint8_t bucket) {
    uint8_t h=bar(bucket);if(!h){canvas_draw_dot(canvas,x,base);canvas_draw_dot(canvas,x+1U,base);return;}
    uint8_t top=(uint8_t)(base-h+1U);if(h>33U){uint8_t solid=(uint8_t)(base-33U+1U);for(uint8_t y=top;y<solid;y++)canvas_draw_dot(canvas,x+((y-top)&1U),y);canvas_draw_box(canvas,x,solid,2U,33U);return;}canvas_draw_box(canvas,x,top,2U,h);
}
static bool flash_visible(const MorseFlipperIcrState* s,uint32_t now) {
    uint32_t elapsed;if(s->phase!=IcrGraphWait||s->target>=MORSE_FLIPPER_ICR_CHAR_COUNT||now>=s->next_at)return true;
    elapsed=s->next_at-now>=ICR_FLASH_MS?0U:ICR_FLASH_MS-(s->next_at-now);return ((elapsed/ICR_FLASH_STEP_MS)&1U)==0U;
}
static void draw_graph(Canvas* canvas,MorseFlipperIcrState* s,uint32_t now) {
    bool visible=flash_visible(s,now);canvas_set_font(canvas,FontSecondary);canvas_draw_str_aligned(canvas,127,8,AlignRight,AlignBottom,phase_label(s));
    for(uint8_t i=0;i<MORSE_FLIPPER_ICR_CHAR_COUNT;i++){uint8_t x=(uint8_t)(4U+i*3U);if(s->phase==IcrGraphWait&&i==s->target&&!visible)continue;draw_bar(canvas,x,59U,s->stats.avg_ms20[i]);}
    if(s->phase==IcrGraphWait&&s->target<MORSE_FLIPPER_ICR_CHAR_COUNT&&visible)canvas_draw_box(canvas,(uint8_t)(4U+s->target*3U),62U,2U,2U);
}
static void draw_choice(Canvas* canvas,int32_t x,int32_t y,uint8_t choice) {
    char text[2]={choice<MORSE_FLIPPER_ICR_CHAR_COUNT?morse_flipper_icr_char_at(choice):'?',0};canvas_set_font(canvas,FontPrimary);canvas_draw_str_aligned(canvas,x,y+1,AlignCenter,AlignCenter,text);
}
MorseFlipperIcrDrawResult morse_flipper_icr_runtime_draw(void* value,Canvas* canvas,uint32_t now) {
    MorseFlipperIcrState* s=value;if(!s||!canvas)return (MorseFlipperIcrDrawResult){0};
    if(s->phase<IcrAnswerGuard){draw_graph(canvas,s,now);return (MorseFlipperIcrDrawResult){0};}
    canvas_set_font(canvas,FontSecondary);canvas_draw_str_aligned(canvas,127,8,AlignRight,AlignBottom,phase_label(s));
    if(s->choice!=MORSE_FLIPPER_ICR_NO_CHOICE||s->phase!=IcrResult){draw_choice(canvas,27,12,s->choices[0]);draw_choice(canvas,27,51,s->choices[1]);draw_choice(canvas,10,32,s->choices[2]);draw_choice(canvas,44,32,s->choices[3]);draw_choice(canvas,27,32,s->choices[4]);}
    canvas_draw_line(canvas,57,0,57,63);if(s->phase!=IcrResult)return (MorseFlipperIcrDrawResult){0};
    return (MorseFlipperIcrDrawResult){.draw_prompt=true,.prompt_char=morse_flipper_icr_char_at(s->target),.prompt_cx=92,.prompt_cy=36};
}

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct MorseFlipperApp MorseFlipperApp;

#ifndef MF_TLM
#define MF_TLM 0
#endif

#if MF_TLM
void mf_tlm_init(const MorseFlipperApp* app);
void mf_tlm_deinit(void);
void mf_tlm_cfg(const MorseFlipperApp* app);
void mf_tlm_session(const MorseFlipperApp* app);
void mf_tlm_group(const MorseFlipperApp* app);
void mf_tlm_open(const MorseFlipperApp* app, uint32_t deadline_ms);
void mf_tlm_answer(const MorseFlipperApp* app, const char* actual, bool ok);
void mf_tlm_done(const MorseFlipperApp* app);
#else
static inline void mf_tlm_init(const MorseFlipperApp* app) {
    (void)app;
}

static inline void mf_tlm_deinit(void) {
}

static inline void mf_tlm_cfg(const MorseFlipperApp* app) {
    (void)app;
}

static inline void mf_tlm_session(const MorseFlipperApp* app) {
    (void)app;
}

static inline void mf_tlm_group(const MorseFlipperApp* app) {
    (void)app;
}

static inline void mf_tlm_open(const MorseFlipperApp* app, uint32_t deadline_ms) {
    (void)app;
    (void)deadline_ms;
}

static inline void mf_tlm_answer(const MorseFlipperApp* app, const char* actual, bool ok) {
    (void)app;
    (void)actual;
    (void)ok;
}

static inline void mf_tlm_done(const MorseFlipperApp* app) {
    (void)app;
}
#endif

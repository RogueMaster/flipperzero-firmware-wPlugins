#pragma once
/*
 * profile_match.h — built-in variant-profile auto-suggest (ESP32, #126).
 *
 * Watches the standard 0x39B DAS parser. When it is FAILING — AP is engaged (the
 * hands-on nag has escalated) yet das_ap_state never reads active — it runs the
 * pure matcher (fsd_logic/fsd_profile_db.h) over a short ring of recent 0x39B
 * frames. On a single confident, override-needing match (i.e. ssw0209's byte0
 * hi-nibble, which the parser can't read) it surfaces a dashboard suggestion.
 *
 * The user taps "Apply" on the dashboard -> the existing sig_cfg path pre-fills
 * the Signal Map (#122). NEVER silent auto-apply. Pure RX / read-only: this
 * module never transmits and never mutates state.
 */

#include <Arduino.h>
#include "fsd_handler.h"  // FSDState, CanFrame

// Wire up shared state (cfg_das_id / hw). Call once from setup().
void profile_match_init(FSDState* state, portMUX_TYPE* state_mux);

// Feed one just-parsed 0x39B DAS frame: raw bytes for the matcher, plus the
// standard parser's das_ap_state / hands_on at that moment (to detect a stuck
// parser). Cheap; called from the CAN task. No-op for other ids.
void profile_match_record(const CanFrame& frame, uint8_t std_ap_state,
                          uint8_t hands_on, uint32_t now_ms);

// Dashboard payload: {"suggest":bool[,"name","das_id","apb","aps","apm",
// "hob","hos","hom"]}. Evaluated on read.
String profile_match_json();

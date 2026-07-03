#pragma once

#include "../app_state.h"

void cfgsave(FlipperHamApp* app);
void cfgload(FlipperHamApp* app);
void callbook_save_txt(FlipperHamApp* app);
void ham_save_txt(FlipperHamApp* app);

bool call_split(const char* s, char* out, uint8_t* ssid, bool* has_ssid);
bool call_validate(char* s);

void bulletin_fix(FlipperHamApp* app);
void status_fix(FlipperHamApp* app);
void message_fix(FlipperHamApp* app);
void position_fix(FlipperHamApp* app);
void calls_fix(FlipperHamApp* app);
void freq_fix(FlipperHamApp* app);

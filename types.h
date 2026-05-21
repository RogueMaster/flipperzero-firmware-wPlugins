#pragma once

#include <stdint.h>

typedef struct Icon Icon;

typedef enum {
  StratagemType_SupportWeapon,
  StratagemType_OrbitalStrike,
  StratagemType_EagleStrike,
  StratagemType_Emplacement,
  StratagemType_Sentry,
  StratagemType_Backpack,
  StratagemType_Vehicle,
  StratagemType_Ship,
  StratagemType_Objective,
  StratagemType_Other,

  StratagemTypeCount
} StratagemType;

typedef struct {
    StratagemType type;
    const char* title;
    const Icon* icon;
    const char* code;
    uint8_t level;
} Stratagem;



#pragma once

#include "../../morse_flipper_mapped_fal.h"
#include "mf_ardf_types.h"

typedef struct VariableItemList VariableItemList;

#define MF_ARDF_API_MAGIC   0x4D464146UL
#define MF_ARDF_API_VERSION 1U

typedef enum {
    MfArdfCommandHostActionInfo = 0,
    MfArdfCommandPopulateSettings,
    MfArdfCommandTextResult,
    MfArdfCommandHostActionResult,
    MfArdfCommandActivateRun,
} MfArdfCommand;

typedef struct {
    const char* text;
    bool accepted;
} MfArdfTextResultCommand;

typedef struct {
    MfArdfHostAction action;
    bool accepted;
} MfArdfHostActionResultCommand;

typedef MorseFlipperHostDialog MfArdfHostActionInfo;

typedef struct {
    MorseFlipperCommandFalApi fal;
} MfArdfApi;

static inline bool mf_ardf_api_valid(const MfArdfApi* api) {
    const MorseFlipperMappedFalApi* mapped = api != NULL ? &api->fal.mapped : NULL;
    return mapped != NULL && mapped->magic == MF_ARDF_API_MAGIC &&
           mapped->api_version == MF_ARDF_API_VERSION &&
           mapped->struct_size == sizeof(MfArdfApi) && mapped->alloc != NULL &&
           mapped->free != NULL && mapped->enter != NULL && mapped->leave != NULL &&
           mapped->input != NULL && mapped->tick != NULL && mapped->draw != NULL &&
           api->fal.command != NULL;
}

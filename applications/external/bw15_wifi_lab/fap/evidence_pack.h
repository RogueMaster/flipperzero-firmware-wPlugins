#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef size_t (*G5EvidenceWriteFn)(void* context, const uint8_t* data, size_t length);
typedef bool (*G5EvidenceStepFn)(void* context);
typedef void (*G5EvidenceDiscardFn)(void* context);

typedef struct {
    void* context;
    G5EvidenceWriteFn write;
    G5EvidenceStepFn sync;
    G5EvidenceStepFn close;
    G5EvidenceStepFn commit;
    G5EvidenceDiscardFn discard;
} G5EvidenceWriter;

typedef enum {
    G5EvidenceOk = 0,
    G5EvidenceInvalidArgument,
    G5EvidenceWriteFailed,
    G5EvidenceSyncFailed,
    G5EvidenceCloseFailed,
    G5EvidenceCommitFailed,
} G5EvidenceResult;

G5EvidenceResult g5_evidence_commit_buffer(
    G5EvidenceWriter* writer,
    const uint8_t* data,
    size_t length);

bool g5_evidence_json_escape(
    const char* input,
    char* output,
    size_t output_capacity,
    size_t* output_length);

const char* g5_evidence_result_name(G5EvidenceResult result);


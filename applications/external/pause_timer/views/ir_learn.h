#pragma once

#include <gui/view.h>
#include <infrared.h>
#include <infrared_worker.h>

typedef struct IrLearnArgs IrLearnArgs;
typedef void (*IrLearnSignalLearnedCallback)(void* context);
typedef void (*IrLearnBackCallback)(void* context);

typedef struct {
    bool has_signal;
    bool is_decoded;
    InfraredMessage decoded_message;
    uint32_t* raw_timings;
    size_t raw_timings_size;
    uint32_t frequency;
    float duty_cycle;
} IrLearnResult;

IrLearnArgs* ir_learn_alloc();
void ir_learn_free(IrLearnArgs* ir_learn);
View* ir_learn_get_view(IrLearnArgs* ir_learn);
void ir_learn_set_callbacks(
    IrLearnArgs* ir_learn,
    IrLearnSignalLearnedCallback signal_learned_callback,
    IrLearnBackCallback back_callback,
    void* context);
IrLearnResult ir_learn_get_result(IrLearnArgs* ir_learn);
void ir_learn_start_receiving(IrLearnArgs* ir_learn);

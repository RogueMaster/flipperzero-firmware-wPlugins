#pragma once

#include <gui/view.h>
#include "../helpers/protopirate_types.h"
#include "../helpers/protopirate_txrx.h"

typedef enum {
    ProtoPirateRemoteAnalyzerFeedbackLevelAll,
    ProtoPirateRemoteAnalyzerFeedbackLevelVibro,
    ProtoPirateRemoteAnalyzerFeedbackLevelMute
} ProtoPirateRemoteAnalyzerFeedbackLevel;

typedef struct ProtoPirateRemoteAnalyzer ProtoPirateRemoteAnalyzer;

typedef void (*ProtoPirateRemoteAnalyzerCallback)(ProtoPirateCustomEvent event, void* context);

void protopirate_remote_analyzer_set_callback(
    ProtoPirateRemoteAnalyzer* protopirate_remote_analyzer,
    ProtoPirateRemoteAnalyzerCallback callback,
    void* context);

ProtoPirateRemoteAnalyzer* protopirate_remote_analyzer_alloc(ProtoPirateTxRx* txrx);

void protopirate_remote_analyzer_free(ProtoPirateRemoteAnalyzer* subghz_static);

View* protopirate_remote_analyzer_get_view(ProtoPirateRemoteAnalyzer* subghz_static);

uint32_t protopirate_remote_analyzer_get_frequency_to_save(ProtoPirateRemoteAnalyzer* instance);

bool protopirate_remote_analyzer_get_is_am_to_save(ProtoPirateRemoteAnalyzer* instance);

ProtoPirateRemoteAnalyzerFeedbackLevel protopirate_remote_analyzer_feedback_level(
    ProtoPirateRemoteAnalyzer* instance,
    ProtoPirateRemoteAnalyzerFeedbackLevel level,
    bool update);

float protopirate_remote_analyzer_get_trigger_level(ProtoPirateRemoteAnalyzer* instance);

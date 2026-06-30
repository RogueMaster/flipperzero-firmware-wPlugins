#pragma once

typedef enum {
    SchedulerStartEventSetInterval,
    SchedulerStartEventSetTiming,
    SchedulerStartEventSetRepeats,
    SchedulerStartEventSetImmediate,
    SchedulerStartEventSetTxDelay,
    SchedulerStartEventSelectFile,
    SchedulerStartRunEvent,
    SchedulerCustomEventErrorBack,
} SchedulerCustomEvent;

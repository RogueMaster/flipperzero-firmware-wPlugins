#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "yrm100x_module.h"
#include "umi_marker_store.h"

/**
 * File that handles the worker for the YRM100
 * @author frux-c
 * @author modified by haffnerriley
*/

/*
 * Read Full has a deep call chain. 2 KiB proved unsafe in field tests. The
 * worker stack remains allocated while the thread is stopped, so 5 KiB keeps
 * the verified margin while recovering 1 KiB versus alpha39's 6 KiB.
 */
#define UHF_WORKER_STACK_SIZE (5U * 1024U)
#define UHF_WORKER_FLAG_STOP  (1U << 0)

// Clone-only flag: zero-fill target User when source dump has no User bytes.
#define UHF_CLONE_FORCE_USER_ZERO (1U << 4)

// Dedicated clone mode for a Saved source with PC=0x3400.
//
// Test UMI Auto learns the User-word marker for each TID model. Clone selects
// a compatible learned entry at runtime and leaves it in the target only when
// the final PC verification succeeds.
#define UHF_CLONE_FORCE_PC3400     (1U << 5)
#define UHF_CLONE_PC3000_SOURCE_PC 0x3000U
#define UHF_CLONE_PC3400_SOURCE_PC 0x3400U

typedef enum {
    // Init states
    UHFWorkerStateNone,
    UHFWorkerStateBroken,
    UHFWorkerStateReady,
    UHFWorkerStateVerify,
    // Main worker states
    UHFWorkerStateDetectSingle,
    UHFWorkerStateDetectSingleFull,
    UHFWorkerStateDetectMultiple,
    UHFWorkerStateDetectMultipleFull,
    UHFWorkerStateDetectForever,
    UHFWorkerStateReadSingleLive,
    UHFWorkerStateDeepReadSelected,
    UHFWorkerStateDeepReadAll,
    UHFWorkerStateReadSingleBank,
    UHFWorkerStateWriteSingle,
    UHFWorkerStateWriteKey,
    UHFWorkerStateCloneScan,
    UHFWorkerStateCloneWrite,
    UHFWorkerStateUmiTest,
    UHFWorkerStateGetBankSizes,
    UHFWorkerStateCheckRewritable,
    //UHFWorkerStateKillTag,
    // Transition
    UHFWorkerStateStop,
} UHFWorkerState;

typedef enum {
    UHFUmiTestOutcomeNone = 0,
    UHFUmiTestOutcomeAffected,
    UHFUmiTestOutcomeUnchanged,
    UHFUmiTestOutcomeAlreadySet,
    UHFUmiTestOutcomeFailed,
    UHFUmiTestOutcomeRestoreFailed,
} UHFUmiTestOutcome;

typedef enum {
    UHFUmiTestStageNone = 0,
    UHFUmiTestStagePoll,
    UHFUmiTestStageSelect,
    UHFUmiTestStageReadUser,
    UHFUmiTestStageWriteTemp,
    UHFUmiTestStageReadPc,
    UHFUmiTestStageRestore,
    UHFUmiTestStageVerifyRestore,
    UHFUmiTestStageFinalPc,
} UHFUmiTestStage;

typedef enum {
    UHFSizeBankStatusUnknown = 0,
    UHFSizeBankStatusOk,
    UHFSizeBankStatusLocked,
    UHFSizeBankStatusWrongPassword,
    UHFSizeBankStatusError,
} UHFSizeBankStatus;

typedef enum {
    UHFRewriteBankStatusUnknown = 0,
    UHFRewriteBankStatusWritable,
    UHFRewriteBankStatusReadOnly,
    UHFRewriteBankStatusProtected,
    UHFRewriteBankStatusUnavailable,
    UHFRewriteBankStatusError,
    UHFRewriteBankStatusRestoreFailed,
} UHFRewriteBankStatus;

typedef enum {
    UHFWorkerEventSuccess,
    UHFWorkerEventFail,
    UHFWorkerEventNoTagDetected,
    UHFWorkerEventAborted,
    UHFWorkerEventCardDetected,
    UHFWorkerEventAccessDenied,
    UHFWorkerEventWrongPassword,
    UHFWorkerEventCloneProgress,
    UHFWorkerEventCloneModeMismatch,
    UHFWorkerEventCloneMarkerUnknown,
} UHFWorkerEvent;

typedef void (*UHFWorkerCallback)(UHFWorkerEvent event, void* ctx);
//Modified by William Riley Haffner
typedef struct UHFWorker {
    FuriThread* thread;
    M100Module* module;
    UHFWorkerCallback callback;
    UHFWorkerState state;
    UHFTagWrapper* uhf_tag_wrapper;
    //Adding tags for writing
    bool KillPwd;
    bool AccessPwd;
    UHFTag* NewTag;
    UHFTag* SelectedTag;
    uint32_t DefaultAP;
    uint32_t DefaultKP;
    // Which bank a single-bank read (UHFWorkerStateReadSingleBank) should fetch.
    BankType TargetBank;
    // When true, a WriteSingle operation targets the specific tag whose EPC is
    // preloaded into SelectedTag (no first-responder poll) and aborts after a
    // 10-second deadline if that tag never answers. Used by the unsaved path.
    bool Targeted;
    // Bitmask of banks to clone (WriteMask flags). Used by CloneWrite state.
    uint16_t CloneMask;
    // Attempts for each retryable Clone I/O stage (Configure: Clone Attempts).
    uint8_t CloneMaxAttempts;
    // Live retry number published to the Clone writing screen.
    volatile uint8_t CloneAttemptCurrent;
    volatile uint8_t CloneAttemptTotal;

    // A PC3000/PC3400 target-mode change always requires explicit UI consent.
    bool CloneModeConversionApproved;
    bool CloneModeConversionApplied;
    uint16_t CloneModeSourcePc;
    uint16_t CloneModeTargetPc;

    // PC3000/PC3400 marker and final verification diagnostics.
    bool ClonePc3400MarkerApplied;
    bool ClonePc3400Verified;
    uint16_t ClonePc3400FinalPc;
    uint16_t ClonePc3400FinalCrc;

    // Read-only registry owned by UHFReaderApp plus the entry selected for the
    // current target before any clone write is allowed.
    const UHFUmiMarkerRegistry* UmiMarkerRegistry;
    bool CloneMarkerKnown;
    uint8_t CloneMarkerModelId[UHF_UMI_MODEL_ID_SIZE];
    uint16_t CloneMarkerWord;
    uint16_t CloneMarkerMask;
    uint16_t CloneMarkerPc3000Bits;
    uint16_t CloneMarkerPc3400Bits;
    uint16_t CloneMarkerAppliedValue;

    uint16_t CloneReservedSourceBytes;
    uint16_t CloneReservedTargetBytes;
    uint16_t CloneReservedWrittenBytes;
    uint16_t CloneReservedSkippedBytes;

    uint16_t BankSizeEpcBytes;
    uint16_t BankSizeTidBytes;
    uint16_t BankSizeUserBytes;
    uint16_t BankSizeReservedBytes;
    UHFSizeBankStatus BankSizeEpcStatus;
    UHFSizeBankStatus BankSizeTidStatus;
    UHFSizeBankStatus BankSizeUserStatus;
    UHFSizeBankStatus BankSizeReservedStatus;
    uint16_t BankSizePc;
    uint16_t BankSizeCrc;

    UHFRewriteBankStatus RewriteEpcStatus;
    UHFRewriteBankStatus RewriteTidStatus;
    UHFRewriteBankStatus RewriteUserStatus;
    UHFRewriteBankStatus RewriteReservedStatus;

    /*
     * Reversible automatic UMI diagnostic.
     *
     * Alpha24 scans User words from the end toward word 0, first with a broad
     * 16-bit change and then with one-bit changes on the best word. Every
     * temporary write is restored and verified before the next candidate.
     */
    UHFUmiTestOutcome UmiTestOutcome;
    UHFUmiTestStage UmiTestFailureStage;
    uint16_t UmiTestPcBefore;
    uint16_t UmiTestPcDuring;
    uint16_t UmiTestPcAfter;
    uint8_t UmiTestUserBefore[2];
    uint8_t UmiTestUserDuring[2];
    uint8_t UmiTestUserAfter[2];
    uint16_t UmiTestUserBytes;
    uint16_t UmiTestBestWord;
    uint16_t UmiTestBestValue;
    uint16_t UmiTestBestMask;
    uint16_t UmiTestPc3000Bits;
    uint16_t UmiTestPc3400Bits;
    uint8_t UmiTestModelId[UHF_UMI_MODEL_ID_SIZE];
    bool UmiTestModelValid;
    uint16_t UmiTestCurrentWord;
    uint16_t UmiTestCurrentValue;
    uint16_t UmiTestCandidatesTested;
    uint16_t UmiTestPositionTests;
    uint16_t UmiTestBitTests;
    bool UmiTestBestFound;
    bool UmiTestBestSingleBit;
    bool UmiTestTemporaryWriteDone;
    bool UmiTestRestoreVerified;
    M100ResponseType UmiTestLastStatus;

    // Full-dump progress for a completed multi inventory.
    volatile size_t DumpCurrent;
    volatile size_t DumpTotal;

    // Live Read Full(Multi) counters.
    volatile size_t FullMultiEpcCount;
    volatile size_t FullMultiDumpCount; // true FULL dumps
    volatile size_t FullMultiPartialCount; // terminal PARTIAL captures

    // Maximum number of whole-card acquisition passes in all Full modes.
    // Current retry counter shown on the Clone writing screen.
    uint8_t FullDumpMaxAttempts;

    /*
     * Read Forever synchronization.
     * The worker publishes exactly one finalized capture at a time and waits
     * until the GUI thread has saved it before beginning the configured delay.
     */
    uint8_t ForeverDelaySeconds;
    volatile bool ForeverWaiting;
    volatile uint8_t ForeverWaitRemainingSeconds;
    volatile uint32_t ForeverCaptureSerial;
    volatile uint32_t ForeverSavedSerial;

    // Software interpretation profile for manufacturer-specific memory layouts.
    uint8_t ReadProfile;

    // Debug-only heap baseline captured immediately before each worker start.

    //uint32_t write_ap;
    void* ctx;
} UHFWorker;

int32_t uhf_worker_task(void* ctx);
UHFWorker* uhf_worker_alloc();
bool uhf_worker_is_ready(const UHFWorker* worker);
void uhf_worker_change_state(UHFWorker* worker, UHFWorkerState state);
// Worker-thread-only stop check. Stop is signalled with a Furi thread flag so
// in-flight UART waits can observe it without racing on the state enum.
bool uhf_worker_stop_requested(const UHFWorker* worker);
void uhf_worker_start(
    UHFWorker* uhf_worker,
    UHFWorkerState state,
    UHFWorkerCallback callback,
    void* ctx);
void uhf_worker_stop(UHFWorker* uhf_worker);
void uhf_worker_free(UHFWorker* uhf_worker);

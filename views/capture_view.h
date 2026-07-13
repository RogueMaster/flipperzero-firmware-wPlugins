#pragma once

#include <gui/view.h>

/* The live-capture screen for the NFC and 1-Wire lessons. While waiting it
 * shows an animated "present your tag" reticle; once a reader hands back a
 * result it shows the decoded fields as annotated lines plus a verdict banner.
 * OK / Right re-scans (via the rescan callback); Back propagates to the scene. */

#define CAPTURE_LINE_LEN 26
#define CAPTURE_VERDICT_LEN 26
#define CAPTURE_MAX_LINES 4

typedef enum {
    CaptureVerdictNeutral,
    CaptureVerdictGood,
    CaptureVerdictBad,
} CaptureVerdictKind;

typedef struct {
    char lines[CAPTURE_MAX_LINES][CAPTURE_LINE_LEN];
    uint8_t nline;
    char verdict[CAPTURE_VERDICT_LEN];
    uint8_t verdict_kind;
} CaptureAnnot;

typedef struct CaptureView CaptureView;
typedef void (*CaptureRescanCb)(void* ctx);

CaptureView* capture_view_alloc(void);
void capture_view_free(CaptureView* v);
View* capture_view_get_view(CaptureView* v);

void capture_view_reset(CaptureView* v, const char* title); // back to waiting
void capture_view_tick(CaptureView* v); // advance animation
void capture_view_set_result(CaptureView* v, const CaptureAnnot* a);
void capture_view_set_rescan_cb(CaptureView* v, CaptureRescanCb cb, void* ctx);

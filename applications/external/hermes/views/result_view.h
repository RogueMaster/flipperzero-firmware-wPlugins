#pragma once

#include <gui/view.h>
#include "../helpers/baud_table.h"

#define RESULT_MAX_ENTRIES (4u)

typedef struct {
    uint32_t baud;
    HermesFraming framing;
    uint8_t confidence; // 0..100
    bool verified; // the UART actually read bytes at this setting
    uint16_t bytes; // read during verification
    uint8_t printable_pct;
    const char* note; // gloss from the baud table, may be ""
} ResultEntry;

typedef struct ResultView ResultView;

/** Fires when the user picks an entry to open the console with. */
typedef void (*ResultViewCallback)(void* context, uint32_t index);

ResultView* result_view_alloc(void);
void result_view_free(ResultView* rv);
View* result_view_get_view(ResultView* rv);

void result_view_set_callback(ResultView* rv, ResultViewCallback cb, void* context);

void result_view_reset(ResultView* rv);
void result_view_add(ResultView* rv, const ResultEntry* entry);

/** Read an entry back out. False when `index` holds nothing. */
bool result_view_get_entry(const ResultView* rv, uint32_t index, ResultEntry* out);

/** Provenance line, e.g. "412 edges - 96% fit". */
void result_view_set_detail(ResultView* rv, const char* detail);

/** No traffic during verification: the answer rests on timing alone. */
void result_view_set_unverified(ResultView* rv, bool unverified);

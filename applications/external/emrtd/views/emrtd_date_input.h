/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * A six digit date field, entered as DD MM YY.
 *
 * The MRZ stores dates as YYMMDD, which is not the order anyone reads them in,
 * and a plain text keyboard makes it far too easy to transpose a pair and be
 * told only that the chip refused to open. This view keeps three separate
 * fields, moves between them with left and right, changes a value with up and
 * down, and will not hand back a date that does not exist.
 */
#pragma once

#include <gui/view.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EmrtdDateInput EmrtdDateInput;

/** Called with the date as six YYMMDD digits plus a terminator. */
typedef void (*EmrtdDateInputCallback)(void* context, const char* yymmdd);

EmrtdDateInput* emrtd_date_input_alloc(void);
void emrtd_date_input_free(EmrtdDateInput* instance);
View* emrtd_date_input_get_view(EmrtdDateInput* instance);

void emrtd_date_input_set_callback(
    EmrtdDateInput* instance,
    EmrtdDateInputCallback callback,
    void* context);

/** Heading, e.g. "Date of birth". */
void emrtd_date_input_set_header(EmrtdDateInput* instance, const char* header);

/** Preload a value; @p yymmdd may be empty. */
void emrtd_date_input_set_value(EmrtdDateInput* instance, const char* yymmdd);

/**
 * Bias the two digit year.
 *
 * A date of birth is in the past, an expiry is in the future, and that is the
 * only way to resolve "30" into 1930 or 2030. Purely a display aid: the value
 * handed back is still the six digits the MRZ carries.
 */
void emrtd_date_input_set_future(EmrtdDateInput* instance, bool future);

#ifdef __cplusplus
}
#endif

#pragma once

#include "amiibo_crypto.h"

/* Name of the figure stored in plaintext on the tag, or NULL if it is not known. */
const char* amiibo_figure_name(const uint8_t tag[AMIIBO_TAG_SIZE]);

/* Official series for one byte of that id, or NULL if this app has no name for it. */
const char* amiibo_series_name(uint8_t series);

/* Series ids in display order. amiibo_series_at writes the id and returns its name. */
size_t amiibo_series_count(void);
const char* amiibo_series_at(size_t index, uint8_t* id);

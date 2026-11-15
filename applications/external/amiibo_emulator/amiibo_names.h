#pragma once

#include "amiibo_crypto.h"

/* Name of the figure stored in plaintext on the tag, or NULL if it is not known. */
const char* amiibo_figure_name(const uint8_t tag[AMIIBO_TAG_SIZE]);

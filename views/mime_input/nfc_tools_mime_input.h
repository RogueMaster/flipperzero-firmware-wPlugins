#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stddef.h>

// Specialised MIME text keyboard: identical to TextInput but with
// /  .  -  in place of 7  8  9 on the 3rd row.

typedef struct MimeInput MimeInput;
typedef void (*MimeInputCallback)(void* context);

MimeInput* mime_input_alloc(void);
void       mime_input_free(MimeInput* mime_input);
void       mime_input_reset(MimeInput* mime_input);
View*      mime_input_get_view(MimeInput* mime_input);

void mime_input_set_header_text(MimeInput* mime_input, const char* text);

void mime_input_set_result_callback(
    MimeInput*        mime_input,
    MimeInputCallback callback,
    void*             callback_context,
    char*             text_buffer,
    size_t            text_buffer_size,
    bool              clear_default_text);

void mime_input_set_minimum_length(MimeInput* mime_input, size_t minimum_length);

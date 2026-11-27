#include "morse_flipper_radio_host.h"

static const MfRadioDecoderServices decoder_services = {
    .struct_size = sizeof(MfRadioDecoderServices),
    .init = morse_flipper_cw_decoder_init,
    .feed_mark = morse_flipper_cw_decoder_feed_mark,
    .feed_space = morse_flipper_cw_decoder_feed_space,
    .dit_ms = morse_flipper_cw_decoder_dit_ms,
    .output = morse_flipper_cw_decoder_output,
    .clear_output = morse_flipper_cw_decoder_clear_output,
    .preview = morse_flipper_cw_decoder_preview,
    .preview_extendable = morse_flipper_cw_decoder_preview_extendable,
};

const MfRadioDecoderServices* morse_flipper_radio_decoder_services(void) {
    return &decoder_services;
}

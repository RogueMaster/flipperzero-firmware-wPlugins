#include "morse_flipper_radio_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void compare_sequence(
    MorseFlipperCwDecoder* direct,
    MorseFlipperCwDecoder* serviced,
    const MfRadioDecoderServices* services) {
    morse_flipper_cw_decoder_feed_mark(direct, 100U);
    services->feed_mark(serviced, 100U);
    morse_flipper_cw_decoder_feed_space(direct, 100U);
    services->feed_space(serviced, 100U);
    morse_flipper_cw_decoder_feed_mark(direct, 300U);
    services->feed_mark(serviced, 300U);
    morse_flipper_cw_decoder_feed_space(direct, 300U);
    services->feed_space(serviced, 300U);
    assert(strcmp(morse_flipper_cw_decoder_output(direct), services->output(serviced)) == 0);
    assert(morse_flipper_cw_decoder_dit_ms(direct) == services->dit_ms(serviced));
    assert(morse_flipper_cw_decoder_preview(direct) == services->preview(serviced));
    assert(
        morse_flipper_cw_decoder_preview_extendable(direct) ==
        services->preview_extendable(serviced));
}

int main(void) {
    MorseFlipperCwDecoder direct;
    MorseFlipperCwDecoder serviced;
    const MfRadioDecoderServices* services = morse_flipper_radio_decoder_services();
    MfRadioDecoderServices invalid;

    assert(!mf_radio_decoder_services_valid(NULL));
    invalid = *services;
    invalid.struct_size--;
    assert(!mf_radio_decoder_services_valid(&invalid));
    invalid = *services;
    invalid.feed_mark = NULL;
    assert(!mf_radio_decoder_services_valid(&invalid));
    assert(mf_radio_decoder_services_valid(services));

    morse_flipper_cw_decoder_init(&direct, 100U);
    services->init(&serviced, 100U);
    compare_sequence(&direct, &serviced, services);

    services->feed_mark(&serviced, 100U);
    assert(services->preview(&serviced) != 0U);
    services->feed_space(&serviced, 500U);
    assert(strlen(services->output(&serviced)) != 0U);
    services->clear_output(&serviced);
    assert(strcmp(services->output(&serviced), "") == 0);

    puts("test_radio_decoder_services: passed");
    return 0;
}

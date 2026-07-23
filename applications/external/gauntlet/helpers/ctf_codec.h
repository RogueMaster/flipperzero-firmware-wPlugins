/**
 * Gauntlet - the decoder Toolkit.
 *
 * A set of pure, self-contained text transforms so a cipher challenge can be
 * cracked right on the Flipper, no laptop at the workshop. Every codec takes a
 * NUL-terminated string in and writes a NUL-terminated string out; non-printable
 * results are rendered as '.' so the tiny screen never chokes. No radio, no UI,
 * no globals - trivially testable.
 */
#pragma once

#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CodecRot = 0, /* Caesar / ROT-N, shift is adjustable (param 0..25) */
    CodecBase64, /* Base64 -> text                                   */
    CodecHex, /* hex pairs -> text                                */
    CodecBinary, /* 8-bit binary groups -> text                      */
    CodecMorse, /* Morse (. - / space) -> text                      */
    CodecAtbash, /* A<->Z mirror alphabet                            */
    CodecReverse, /* reverse the string                               */
    CodecCount,
} CodecId;

/** Human name of a codec ("ROT-13", "Base64", "Hex", ...). For CodecRot the
 *  name folds in the shift, so pass the current shift; ignored otherwise. */
const char* codec_name(CodecId id, int param);

/** True if the codec has a tunable parameter (only CodecRot today). */
bool codec_uses_param(CodecId id);

/** Apply a codec: transform `in` into `out` (always NUL-terminated, capped to
 *  out_sz). `param` is the ROT shift for CodecRot (0..25), ignored otherwise. */
void codec_apply(CodecId id, const char* in, int param, char* out, size_t out_sz);

#ifdef __cplusplus
}
#endif

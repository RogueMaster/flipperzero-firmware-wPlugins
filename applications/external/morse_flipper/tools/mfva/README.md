# MFVA voice packs

MFVA is Morse Flipper's small, private voice-pack format. It keeps forty short
clips in one file, so playback needs one open file and one indexed seek rather
than a small blizzard of SD-card opens. WAV remains the source and audition
format; MFVA is merely the runtime packing crate.

Version 1 is little-endian. Its 32-byte header contains `MFVA`, version, codec,
token count, sample rate, table and data offsets, file size, and CRC32 values
for the table and payload. Forty 20-byte entries follow, one each for `A-Z`,
`0-9`, `stroke`, `period`, `comma`, and `question-mark`. Each entry records the
token ID, absolute payload offset, byte length, decoded sample count, and codec
state. Payloads follow the table and are independently playable.

The retained source set is mono PCM16 at 16 kHz. Rebuild both reference packs
and verify the shipped PCM8 copy with:

```sh
python3 tools/mfva/build_passive_voice_pack.py \
  --wav-input tools/mfva/voice_en_gb_amy_v1/wav \
  --variant s16_16k \
  --output tools/mfva/voice_en_gb_amy_v1/voice_en_gb_amy_v1_s16_16k.mfa
python3 tools/mfva/build_passive_voice_pack.py \
  --from-mfva tools/mfva/voice_en_gb_amy_v1/voice_en_gb_amy_v1_s16_16k.mfa \
  --output tools/mfva/voice_en_gb_amy_v1/voice_en_gb_amy_v1_u8_16k.mfa
cmp tools/mfva/voice_en_gb_amy_v1/voice_en_gb_amy_v1_u8_16k.mfa \
  assets/audio/voice_en_gb_amy_v1.mfa
```

The firmware validates magic, version, codec, bounds, token coverage, and both
CRCs before playback. MFVA is not a standard, and nobody else is expected to
understand it. That is fine; it is documented so we still do.

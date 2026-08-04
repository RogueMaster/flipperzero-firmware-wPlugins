#ifndef yo3gnd_wav_11e0
#define yo3gnd_wav_11e0

#include <stdbool.h>
#include <stdint.h>

typedef struct Wav Wav;

Wav *wavnew(void);
void wavfree(Wav *wav);
bool wavopen(Wav *wav, const char *path);
bool wavnext(Wav *wav, uint8_t *s);

#endif

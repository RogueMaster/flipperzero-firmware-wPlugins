#ifndef AUDIO_H
#define AUDIO_H

#include "sdlint.h"

/* Haszn�lhat� hangok flag-jei */
#define SOUND_SHOT     0x00000001 /* L�v�shang */
#define SOUND_DEATH    0x00000002 /* Hal�lhang */
#define SOUND_BONUSWPN 0x00000004 /* B�nuszfegyverek hangja */
#define SOUND_MENUBTN  0x10000000 /* Men�hang */

/* Hangok frekvenci�i */
#define BUTTON_FREQ          1000 /* A gombnyom�s hangj�nak frekvenci�ja */
#define SHOT_FREQ_DISTORT    1500 /* A l�v�s torz komponense */
#define SHOT_FREQ_CONTINOUS  6000 /* A l�v�s folyamatos komponense */
#define DEATH_FREQ_DISTORT   4200 /* A hal�l torz komponense */
#define DEATH_FREQ_CONTINOUS 5200 /* A hal�l folyamatos komponense */
#define BONUS_FREQ_DISTORT   4900 /* A b�nusz torz komponense */
#define BONUS_FREQ_CONTINOUS 5250 /* A b�nusz folyamatos komponense */

/* A hangfunkci�k le�r�sa az audio.c-ben tal�lhat� */
void AudioCallback(void*, Uint8*, int);

#endif /* AUDIO_H */

#include "radio_audio.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_interrupt.h>

#define RADIO_TIMER_DIVISOR           69U
#define RADIO_SAMPLE_RATE             14493U
#define RADIO_RING_SAMPLES            8192U
#define RADIO_RING_MASK               (RADIO_RING_SAMPLES - 1U)
#define RADIO_PRIME_SAMPLES           6144U
#define RADIO_CONCEAL_FADE_SAMPLES    116U
#define RADIO_UNDERFLOW_GRACE_SAMPLES 1449U
#define RADIO_PWM_PERIOD              1024U
#define RADIO_MAX_GAIN_Q8             (6U * 256U)
#define RADIO_LEVEL_TARGET            32700U
#define RADIO_LEVEL_UPDATE_MASK       7U
#define RADIO_MAKEUP_GAIN_PERCENT     150
#define RADIO_SOFT_KNEE               24576

struct RadioAudio {
    volatile bool running;
    volatile bool abort_write;
    volatile bool primed;
    volatile uint8_t volume;
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t underflows;
    volatile uint16_t conceal_samples;
    volatile int16_t last_sample;
    volatile bool underflow_active;
    uint32_t level_envelope;
    uint16_t level_gain_q8;
    uint8_t level_update_counter;
    int16_t ring[RADIO_RING_SAMPLES];
    bool timer_owned;
    bool speaker_owned;
};

static uint16_t radio_audio_available(const RadioAudio* audio) {
    return (audio->head - audio->tail) & RADIO_RING_MASK;
}

static int16_t radio_audio_pop(RadioAudio* audio) {
    if(!audio->primed) {
        if(radio_audio_available(audio) < RADIO_PRIME_SAMPLES) return 0;
        audio->primed = true;
        audio->underflow_active = false;
        audio->conceal_samples = 0U;
    }
    if(audio->tail == audio->head) {
        /* Scheduler and USB delivery can occasionally be late by only a few
         * milliseconds. Do not turn that into a long audible rebuffer: fade
         * the last sample briefly and keep the stream armed for 100 ms. */
        if(!audio->underflow_active) {
            audio->underflows++;
            audio->underflow_active = true;
            audio->conceal_samples = 0U;
        }
        int16_t concealed = 0;
        if(audio->conceal_samples < RADIO_CONCEAL_FADE_SAMPLES) {
            concealed =
                (int16_t)(((int32_t)audio->last_sample *
                           (int32_t)(RADIO_CONCEAL_FADE_SAMPLES - audio->conceal_samples)) /
                          (int32_t)RADIO_CONCEAL_FADE_SAMPLES);
        }
        if(audio->conceal_samples < RADIO_UNDERFLOW_GRACE_SAMPLES) {
            audio->conceal_samples++;
        } else {
            audio->primed = false;
        }
        return concealed;
    }
    const int16_t sample = audio->ring[audio->tail];
    audio->tail = (audio->tail + 1U) & RADIO_RING_MASK;
    audio->last_sample = sample;
    audio->underflow_active = false;
    audio->conceal_samples = 0U;
    return sample;
}

static void radio_audio_isr(void* context) {
    RadioAudio* audio = context;
    if(LL_TIM_IsActiveFlag_UPDATE(TIM2)) LL_TIM_ClearFlag_UPDATE(TIM2);
    int32_t sample = radio_audio_pop(audio);

    /* Preserve the requested six-times boost for quiet material without
     * hard-clipping normal music. The peak follower attacks immediately and
     * releases slowly, producing the loudest clean level the PWM can carry. */
    const uint32_t magnitude = sample < 0 ? (uint32_t)-sample : (uint32_t)sample;
    if(magnitude > audio->level_envelope) {
        audio->level_envelope = magnitude;
    } else if(audio->level_envelope != 0U) {
        audio->level_envelope -= (audio->level_envelope + 4095U) >> 12U;
    }
    if((audio->level_update_counter++ & RADIO_LEVEL_UPDATE_MASK) == 0U) {
        const uint32_t minimum_envelope = (RADIO_LEVEL_TARGET * 256U) / RADIO_MAX_GAIN_Q8;
        const uint32_t envelope = audio->level_envelope < minimum_envelope ? minimum_envelope :
                                                                             audio->level_envelope;
        uint32_t desired_gain = (RADIO_LEVEL_TARGET * 256U) / envelope;
        if(desired_gain > RADIO_MAX_GAIN_Q8) desired_gain = RADIO_MAX_GAIN_Q8;
        if(desired_gain < 256U) desired_gain = 256U;
        if(desired_gain < audio->level_gain_q8) {
            audio->level_gain_q8 = (uint16_t)desired_gain;
        } else if(desired_gain > audio->level_gain_q8) {
            const uint32_t rise = (desired_gain - audio->level_gain_q8 + 31U) >> 5U;
            audio->level_gain_q8 = (uint16_t)(audio->level_gain_q8 + rise);
        }
    }
    sample = (sample * (int32_t)audio->level_gain_q8) / 256;
    sample = (sample * audio->volume) / 100;

    /* Raise perceived loudness after automatic levelling. Above 75% of the
     * PWM range a soft knee compresses peaks instead of flattening them, so
     * the speaker is louder without returning to the old hard-clipped sound. */
    sample = (sample * RADIO_MAKEUP_GAIN_PERCENT) / 100;
    const int32_t boosted_magnitude = sample < 0 ? -sample : sample;
    if(boosted_magnitude > RADIO_SOFT_KNEE) {
        const int32_t limited = RADIO_SOFT_KNEE + (boosted_magnitude - RADIO_SOFT_KNEE) / 3;
        sample = sample < 0 ? -limited : limited;
    }
    if(sample > 32767) sample = 32767;
    if(sample < -32768) sample = -32768;
    TIM16->CCR1 = (uint16_t)(sample + 32768) >> 6U;
}

RadioAudio* radio_audio_alloc(void) {
    return calloc(1, sizeof(RadioAudio));
}

void radio_audio_stop(RadioAudio* audio) {
    if(!audio || !audio->running) return;
    audio->running = false;
    TIM2->DIER = 0;
    TIM2->CR1 = 0;
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, NULL, NULL);
    if(audio->timer_owned) {
        furi_hal_bus_disable(FuriHalBusTIM2);
        audio->timer_owned = false;
    }
    if(audio->speaker_owned) {
        furi_hal_speaker_release();
        audio->speaker_owned = false;
    }
    furi_hal_power_insomnia_exit();
}

void radio_audio_free(RadioAudio* audio) {
    if(!audio) return;
    radio_audio_stop(audio);
    free(audio);
}

bool radio_audio_start(RadioAudio* audio, uint8_t volume) {
    if(!audio || furi_hal_bus_is_enabled(FuriHalBusTIM2) || !furi_hal_speaker_acquire(1000U))
        return false;
    memset(audio->ring, 0, sizeof(audio->ring));
    audio->head = audio->tail = 0U;
    audio->primed = false;
    audio->underflows = 0U;
    audio->conceal_samples = 0U;
    audio->last_sample = 0;
    audio->underflow_active = false;
    audio->level_envelope = 0U;
    audio->level_gain_q8 = RADIO_MAX_GAIN_Q8;
    audio->level_update_counter = 0U;
    audio->volume = volume;
    audio->speaker_owned = true;
    audio->running = true;
    audio->abort_write = false;
    furi_hal_power_insomnia_enter();

    TIM16->CR1 = 0;
    TIM16->CR2 = 0;
    TIM16->DIER = 0;
    TIM16->CCER = 0;
    TIM16->CCMR1 = TIM_CCMR1_OC1PE | (6U << TIM_CCMR1_OC1M_Pos);
    TIM16->PSC = 0U;
    TIM16->ARR = RADIO_PWM_PERIOD - 1U;
    TIM16->CCR1 = RADIO_PWM_PERIOD / 2U;
    TIM16->EGR = TIM_EGR_UG;
    TIM16->SR = 0;
    TIM16->CCER = TIM_CCER_CC1E;
    TIM16->BDTR = TIM_BDTR_MOE;
    TIM16->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;

    furi_hal_bus_enable(FuriHalBusTIM2);
    audio->timer_owned = true;
    TIM2->PSC = (SystemCoreClock / 1000000U) - 1U;
    TIM2->ARR = RADIO_TIMER_DIVISOR - 1U;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    furi_hal_interrupt_set_isr_ex(
        FuriHalInterruptIdTIM2, FuriHalInterruptPriorityHighest, radio_audio_isr, audio);
    TIM2->DIER = TIM_DIER_UIE;
    TIM2->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
    return true;
}

bool radio_audio_write(RadioAudio* audio, int16_t sample) {
    while(audio && audio->running && !audio->abort_write) {
        const uint16_t next = (audio->head + 1U) & RADIO_RING_MASK;
        if(next != audio->tail) {
            audio->ring[audio->head] = sample;
            audio->head = next;
            return true;
        }
        furi_delay_tick(1U);
    }
    return false;
}

void radio_audio_request_stop(RadioAudio* audio) {
    if(!audio) return;
    audio->abort_write = true;
    /* Silence the PWM immediately on Back/Stop. Resource release remains on
     * the decoder thread, but the user never hears it while that thread exits. */
    FURI_CRITICAL_ENTER();
    if(audio->running) {
        TIM2->DIER = 0;
        TIM2->CR1 = 0;
        TIM16->CCR1 = RADIO_PWM_PERIOD / 2U;
    }
    FURI_CRITICAL_EXIT();
}

uint32_t radio_audio_sample_rate(void) {
    return RADIO_SAMPLE_RATE;
}

uint32_t radio_audio_underflows(const RadioAudio* audio) {
    return audio ? audio->underflows : 0U;
}

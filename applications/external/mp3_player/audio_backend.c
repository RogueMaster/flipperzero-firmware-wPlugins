#include "audio_backend.h"

#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_spi.h>
#include <furi_hal_spi_config.h>

#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_dmamux.h>
#include <stm32wbxx_ll_tim.h>

#define INTERNAL_SAMPLE_RATE 15625U
#define MAX98357_SAMPLE_RATE 16000U
#define MAX98357_BITS_PER_FRAME 32U
#define MAX98357_BCLK_RATE (MAX98357_SAMPLE_RATE * MAX98357_BITS_PER_FRAME)
#define AUDIO_RING_SAMPLES 4096U
#define AUDIO_RING_MASK (AUDIO_RING_SAMPLES - 1U)
#define AUDIO_PRIME_SAMPLES 512U
#define INTERNAL_PWM_PERIOD 1024U
#define MAX98357_BUFFER_BITS 1024U
#define MAX98357_HALF_BITS (MAX98357_BUFFER_BITS / 2U)

struct AudioBackend {
  volatile bool running;
  volatile bool paused;
  volatile uint8_t volume;
  AudioOutput output;

  volatile uint16_t head;
  volatile uint16_t tail;
  int16_t ring[AUDIO_RING_SAMPLES];
  volatile uint32_t underflows;
  volatile uint32_t played_samples;
  volatile bool primed;
  volatile bool draining;

  uint32_t max_gpio_bits[MAX98357_BUFFER_BITS];
  AudioBackendError error;
  bool tim2_owned;
  bool speaker_acquired;
  bool otg_owned;
  bool spi_initialized;
  bool spi_acquired;
};

static uint16_t audio_available(const AudioBackend *backend) {
  return (backend->head - backend->tail) & AUDIO_RING_MASK;
}

static int16_t audio_pop(AudioBackend *backend) {
  if (backend->paused)
    return 0;

  if (!backend->primed) {
    const uint16_t threshold = backend->draining ? 1U : AUDIO_PRIME_SAMPLES;
    if (audio_available(backend) < threshold)
      return 0;
    backend->primed = true;
  }

  const uint16_t tail = backend->tail;
  if (tail == backend->head) {
    if (!backend->draining)
      backend->underflows++;
    backend->primed = false;
    return 0;
  }

  const int16_t sample = backend->ring[tail];
  backend->tail = (tail + 1U) & AUDIO_RING_MASK;
  backend->played_samples++;
  return sample;
}

static int16_t audio_scale(const AudioBackend *backend, int16_t sample) {
  return (int16_t)(((int32_t)sample * backend->volume) / 100);
}

static int16_t audio_internal_scale(const AudioBackend *backend,
                                    int16_t sample) {
  int32_t magnitude = sample < 0 ? -(int32_t)sample : sample;

  /* The GPIO voltage is already at its safe limit, so increase perceived
     loudness by lifting quiet material and gently compressing peaks. This
     keeps the final waveform inside the same full-scale PWM envelope. */
  if (magnitude <= 16384) {
    magnitude += magnitude / 2;
  } else {
    magnitude = 24576 + (magnitude - 16384) / 2;
  }
  if (magnitude > 32767)
    magnitude = 32767;
  if (sample < 0)
    magnitude = -magnitude;

  return (int16_t)((magnitude * backend->volume) / 100);
}

static void audio_internal_isr(void *context) {
  AudioBackend *backend = context;
  if (LL_TIM_IsActiveFlag_UPDATE(TIM2))
    LL_TIM_ClearFlag_UPDATE(TIM2);

  const int32_t sample = audio_internal_scale(backend, audio_pop(backend));
  /* Convert signed 16-bit PCM to the full 10-bit PWM duty range. A zero
     sample becomes 50% duty, which is electrical silence after the piezo's
     carrier is averaged. */
  TIM16->CCR1 = (uint16_t)(sample + 32768) >> 6U;
}

static void audio_max_fill(AudioBackend *backend, uint16_t offset) {
  const uint16_t frames = MAX98357_HALF_BITS / MAX98357_BITS_PER_FRAME;
  uint16_t cursor = offset;

  for (uint16_t frame = 0; frame < frames; frame++) {
    const int16_t sample = audio_scale(backend, audio_pop(backend));
    const uint16_t bits = (uint16_t)sample;

    /* MAX98357A uses 16-bit I2S slots. The first clock after an LRCLK change
       is the I2S delay; the following 15 clocks carry bits 15..1. The lost
       least-significant bit still leaves 15-bit audio. Send mono to both
       channel slots so breakout-board channel selection does not matter. */
    for (uint8_t channel = 0; channel < 2U; channel++) {
      const uint32_t lr = channel ? (1UL << 4U) : (1UL << (4U + 16U));
      backend->max_gpio_bits[cursor++] = lr | (1UL << (7U + 16U));
      for (int8_t bit = 15; bit >= 1; bit--) {
        const uint32_t data =
            (bits & (1U << bit)) ? (1UL << 7U) : (1UL << (7U + 16U));
        backend->max_gpio_bits[cursor++] = lr | data;
      }
    }
  }
}

static void audio_max_dma_isr(void *context) {
  AudioBackend *backend = context;
  if (LL_DMA_IsActiveFlag_HT3(DMA1)) {
    LL_DMA_ClearFlag_HT3(DMA1);
    audio_max_fill(backend, 0);
  }
  if (LL_DMA_IsActiveFlag_TC3(DMA1)) {
    LL_DMA_ClearFlag_TC3(DMA1);
    audio_max_fill(backend, MAX98357_HALF_BITS);
  }
  if (LL_DMA_IsActiveFlag_TE3(DMA1))
    LL_DMA_ClearFlag_TE3(DMA1);
}

static bool audio_internal_start(AudioBackend *backend) {
  if (furi_hal_bus_is_enabled(FuriHalBusTIM2)) {
    backend->error = AudioBackendErrorResourceBusy;
    return false;
  }
  if (!furi_hal_speaker_acquire(1000)) {
    backend->error = AudioBackendErrorResourceBusy;
    return false;
  }
  backend->speaker_acquired = true;

  /* furi_hal_speaker_start() uses a fixed /500 timer prescaler. At a high
     carrier frequency that leaves only a few PWM duty steps, which sounds
     like isolated clicks. Configure TIM16 directly for 10-bit PWM instead:
     64 MHz / 1024 = 62.5 kHz carrier, exactly four PWM cycles per sample. */
  TIM16->CR1 = 0;
  TIM16->CR2 = 0;
  TIM16->DIER = 0;
  TIM16->CCER = 0;
  TIM16->CCMR1 = TIM_CCMR1_OC1PE | (6U << TIM_CCMR1_OC1M_Pos);
  TIM16->PSC = 0U;
  TIM16->ARR = INTERNAL_PWM_PERIOD - 1U;
  TIM16->CCR1 = INTERNAL_PWM_PERIOD / 2U;
  TIM16->EGR = TIM_EGR_UG;
  TIM16->SR = 0;
  TIM16->CCER = TIM_CCER_CC1E;
  TIM16->BDTR = TIM_BDTR_MOE;
  TIM16->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;

  furi_hal_bus_enable(FuriHalBusTIM2);
  backend->tim2_owned = true;

  TIM2->PSC = (SystemCoreClock / 1000000U) - 1U;
  TIM2->ARR = (1000000U / INTERNAL_SAMPLE_RATE) - 1U;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->SR = 0;
  furi_hal_interrupt_set_isr_ex(FuriHalInterruptIdTIM2,
                                FuriHalInterruptPriorityHighest,
                                audio_internal_isr, backend);
  TIM2->DIER = TIM_DIER_UIE;
  TIM2->CR1 = TIM_CR1_CEN | TIM_CR1_ARPE;
  return true;
}

static bool audio_max_start(AudioBackend *backend) {
  if (furi_hal_bus_is_enabled(FuriHalBusTIM2)) {
    backend->error = AudioBackendErrorResourceBusy;
    return false;
  }

  /* Pin 1 is already supplied by USB when VBUS is present. Enabling battery
     boost in that state fails, so only request OTG power when running from the
     battery and no other service already owns it. */
  const bool usb_powered = furi_hal_power_get_usb_voltage() >= 4.0f;
  if (!usb_powered && !furi_hal_power_is_otg_enabled()) {
    if (!furi_hal_power_enable_otg()) {
      backend->error = AudioBackendErrorPowerUnavailable;
      return false;
    }
    backend->otg_owned = true;
  }

  furi_hal_bus_enable(FuriHalBusTIM2);
  backend->tim2_owned = true;
  /* PB3 and PA7 are also the shared radio/NFC SPI bus. Hold its mutex while
     those pins are repurposed for I2S so no system client can reconfigure
     them during playback. */
  furi_hal_spi_bus_handle_init(&furi_hal_spi_bus_handle_external);
  backend->spi_initialized = true;
  furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
  backend->spi_acquired = true;
  furi_hal_gpio_init_ex(&gpio_ext_pb3, GpioModeAltFunctionPushPull, GpioPullNo,
                        GpioSpeedVeryHigh, GpioAltFn1TIM2);
  furi_hal_gpio_init(&gpio_ext_pa7, GpioModeOutputPushPull, GpioPullNo,
                     GpioSpeedVeryHigh);
  furi_hal_gpio_init(&gpio_ext_pa4, GpioModeOutputPushPull, GpioPullNo,
                     GpioSpeedVeryHigh);

  audio_max_fill(backend, 0);
  audio_max_fill(backend, MAX98357_HALF_BITS);

  /* A TIM2 update starts each BCLK period and DMA publishes LRCLK plus DIN
     together before the rising edge. DMA1 channel 3 is documented as free for
     user code, unlike the system SPI DMA channels used by SD-card reads. */
  LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
  LL_DMA_ClearFlag_GI3(DMA1);
  LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_3, LL_DMAMUX_REQ_TIM2_UP);
  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_3,
                                  LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
  LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_3,
                                 LL_DMA_PRIORITY_VERYHIGH);
  LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MODE_CIRCULAR);
  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PERIPH_NOINCREMENT);
  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MEMORY_INCREMENT);
  LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_PDATAALIGN_WORD);
  LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_3, LL_DMA_MDATAALIGN_WORD);
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_3, (uint32_t)&GPIOA->BSRR);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_3,
                          (uint32_t)backend->max_gpio_bits);
  LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, MAX98357_BUFFER_BITS);
  LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_3);
  LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_3);
  LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_3);
  furi_hal_interrupt_set_isr_ex(FuriHalInterruptIdDma1Ch3,
                                FuriHalInterruptPriorityHigh, audio_max_dma_isr,
                                backend);

  const uint32_t timer_period = SystemCoreClock / MAX98357_BCLK_RATE;
  TIM2->CR1 = 0;
  TIM2->CR2 = 0;
  TIM2->DIER = 0;
  TIM2->CCER = 0;
  TIM2->CCMR1 = TIM_CCMR1_OC2PE | (7U << TIM_CCMR1_OC2M_Pos);
  TIM2->PSC = 0;
  TIM2->ARR = timer_period - 1U;
  TIM2->CCR2 = timer_period / 2U;
  TIM2->CNT = 0;
  TIM2->CR1 = TIM_CR1_ARPE;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->SR = 0;
  TIM2->CCER = TIM_CCER_CC2E;

  LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);
  TIM2->DIER = TIM_DIER_UDE;
  TIM2->EGR = TIM_EGR_UG; /* Publish bit zero before the first BCLK edge. */
  TIM2->SR = 0;
  for (uint16_t wait = 0;
       wait < 1024U &&
       LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_3) == MAX98357_BUFFER_BITS;
       wait++) {
    __NOP();
  }
  TIM2->CR1 |= TIM_CR1_CEN;
  return true;
}

AudioBackend *audio_backend_alloc(void) {
  return calloc(1, sizeof(AudioBackend));
}

void audio_backend_free(AudioBackend *backend) {
  if (!backend)
    return;
  audio_backend_stop(backend);
  free(backend);
}

bool audio_backend_start(AudioBackend *backend, AudioOutput output,
                         uint8_t volume) {
  furi_assert(backend);
  backend->head = 0;
  backend->tail = 0;
  backend->underflows = 0;
  backend->played_samples = 0;
  backend->paused = false;
  backend->primed = false;
  backend->draining = false;
  backend->volume = volume;
  backend->output = output;
  backend->error = AudioBackendErrorNone;
  backend->tim2_owned = false;
  backend->speaker_acquired = false;
  backend->otg_owned = false;
  backend->spi_initialized = false;
  backend->spi_acquired = false;
  backend->running = true;
  furi_hal_power_insomnia_enter();

  const bool started = output == AudioOutputInternal
                           ? audio_internal_start(backend)
                           : audio_max_start(backend);
  if (!started) {
    backend->running = false;
    furi_hal_power_insomnia_exit();
  }
  return started;
}

void audio_backend_stop(AudioBackend *backend) {
  if (!backend || !backend->running)
    return;
  backend->running = false;

  if (backend->output == AudioOutputInternal) {
    TIM2->DIER = 0;
    TIM2->CR1 = 0;
    furi_hal_interrupt_set_isr(FuriHalInterruptIdTIM2, NULL, NULL);
    if (backend->tim2_owned) {
      furi_hal_bus_disable(FuriHalBusTIM2);
      backend->tim2_owned = false;
    }
    if (backend->speaker_acquired) {
      furi_hal_speaker_release();
      backend->speaker_acquired = false;
    }
  } else {
    /* Stop BCLK before changing LRCLK or DIN. */
    TIM2->CR1 = 0;
    TIM2->DIER = 0;
    TIM2->CCER = 0;
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_ClearFlag_GI3(DMA1);
    furi_hal_interrupt_set_isr(FuriHalInterruptIdDma1Ch3, NULL, NULL);
    if (backend->spi_acquired) {
      furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
      backend->spi_acquired = false;
    }
    if (backend->spi_initialized) {
      furi_hal_spi_bus_handle_deinit(&furi_hal_spi_bus_handle_external);
      backend->spi_initialized = false;
    }
    if (backend->tim2_owned) {
      furi_hal_bus_disable(FuriHalBusTIM2);
      backend->tim2_owned = false;
    }
    if (backend->otg_owned) {
      furi_hal_power_disable_otg();
      backend->otg_owned = false;
    }
  }

  furi_hal_power_insomnia_exit();
}

bool audio_backend_write(AudioBackend *backend, int16_t sample) {
  while (backend->running) {
    const uint16_t next = (backend->head + 1U) & AUDIO_RING_MASK;
    if (next != backend->tail) {
      backend->ring[backend->head] = sample;
      backend->head = next;
      return true;
    }
    furi_delay_tick(1);
  }
  return false;
}

void audio_backend_set_volume(AudioBackend *backend, uint8_t volume) {
  if (backend)
    backend->volume = volume;
}

void audio_backend_set_paused(AudioBackend *backend, bool paused) {
  if (backend)
    backend->paused = paused;
}

void audio_backend_drain(AudioBackend *backend, uint32_t timeout_ms) {
  if (!backend || !backend->running)
    return;

  backend->draining = true;
  if (audio_available(backend))
    backend->primed = true;

  const uint32_t started = furi_get_tick();
  const uint32_t timeout = furi_ms_to_ticks(timeout_ms);
  while (backend->running && audio_available(backend)) {
    if ((uint32_t)(furi_get_tick() - started) >= timeout)
      break;
    furi_delay_tick(1);
  }
  backend->draining = false;
}

uint32_t audio_backend_get_underflows(const AudioBackend *backend) {
  return backend ? backend->underflows : 0;
}

uint32_t audio_backend_get_played_samples(const AudioBackend *backend) {
  return backend ? backend->played_samples : 0;
}

void audio_backend_reset_progress(AudioBackend *backend) {
  if (backend)
    backend->played_samples = 0;
}

AudioBackendError audio_backend_get_error(const AudioBackend *backend) {
  return backend ? backend->error : AudioBackendErrorResourceBusy;
}

uint32_t audio_backend_get_sample_rate(AudioOutput output) {
  return output == AudioOutputMax98357a ? MAX98357_SAMPLE_RATE
                                        : INTERNAL_SAMPLE_RATE;
}

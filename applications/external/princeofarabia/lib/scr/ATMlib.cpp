#ifdef ARDULIB_USE_ATM
#include "../ATMlib.h"
#include "../include/ArduboyAudioState.h"

#include <string.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <stm32wbxx_ll_tim.h>
#include <stm32wbxx_ll_dma.h>

uint8_t trackCount = 0;
uint8_t tickRate = 25;
const uint16_t* trackList = NULL;
const uint8_t* trackBase = NULL;

uint8_t pcm = 128;
uint16_t cia = 1;
uint16_t cia_count = 1;

osc_t osc[4];

static uint8_t ChannelActiveMute = 0b11110000;

static const uint16_t noteTable[64] = {0,    262,  277,  294,  311,  330,  349,  370,  392,  415,
                                       440,  466,  494,  523,  554,  587,  622,  659,  698,  740,
                                       784,  831,  880,  932,  988,  1047, 1109, 1175, 1245, 1319,
                                       1397, 1480, 1568, 1661, 1760, 1865, 1976, 2093, 2217, 2349,
                                       2489, 2637, 2794, 2960, 3136, 3322, 3520, 3729, 3951, 4186,
                                       4435, 4699, 4978, 5274, 5588, 5920, 6272, 6645, 7040, 7459,
                                       7902, 8372, 8870, 9397};

struct ch_t {
    const uint8_t* ptr;
    uint8_t note;

    uint16_t stackPointer[7];
    uint8_t stackCounter[7];
    uint8_t stackTrack[7];

    uint8_t stackIndex;
    uint8_t repeatPoint;

    uint16_t delay;
    uint8_t counter;
    uint8_t track;

    uint16_t freq;
    uint8_t vol;

    int8_t volFreSlide;
    uint8_t volFreConfig;
    uint8_t volFreCount;

    uint8_t arpNotes;
    uint8_t arpTiming;
    uint8_t arpCount;

    uint8_t reConfig;
    uint8_t reCount;

    int8_t transConfig;

    uint8_t treviDepth;
    uint8_t treviConfig;
    uint8_t treviCount;

    int8_t glisConfig;
    uint8_t glisCount;
};

static ch_t channel_state[4];

static uint16_t read_vle(const uint8_t** pp) {
    uint16_t q = 0;
    uint8_t d;
    do {
        q <<= 7;
        d = *(*pp)++;
        q |= (d & 0x7F);
    } while(d & 0x80);
    return q;
}

static inline const uint8_t* getTrackPointer(uint8_t track) {
    return trackBase + trackList[track];
}

static inline uint16_t read_u16_le(const uint8_t** pp) {
    uint16_t lo = *(*pp)++;
    uint16_t hi = *(*pp)++;
    return (uint16_t)(lo | (hi << 8));
}

static constexpr uint32_t ATM_PWM_ARR = 255;
static constexpr uint32_t ATM_PWM_PSC = 7;
static constexpr uint32_t ATM_LOGICAL_HZ = 31250;

static inline uint32_t tick_div_from_rate(uint8_t tr) {
    if(tr < 1) tr = 1;
    uint32_t div = ATM_LOGICAL_HZ / (uint32_t)tr;
    if(div < 1) div = 1;
    return div;
}

static constexpr size_t ATM_LOGICAL_SAMPLES_PER_HALF = 128;
static constexpr size_t ATM_DMA_SAMPLES_PER_HALF = ATM_LOGICAL_SAMPLES_PER_HALF;
static constexpr size_t ATM_DMA_TOTAL = ATM_DMA_SAMPLES_PER_HALF * 2;

static uint32_t dma_buf[ATM_DMA_TOTAL];

static bool ardulib_atm_running = false;
static bool ardulib_atm_paused = false;

static constexpr float ATM_MASTER_GAIN_MAX = 1.7f;
static constexpr uint16_t ATM_MASTER_GAIN_DEFAULT_Q8 = (uint16_t)(1.8f * 256.0f + 0.5f);
static uint16_t ardulib_atm_master_gain_q8 = ATM_MASTER_GAIN_DEFAULT_Q8;

static uint32_t ardulib_atm_tick_div = 0;
static uint32_t ardulib_atm_tick_acc = 0;
static uint32_t ardulib_atm_tick_pending = 0;

static FuriThread* ardulib_atm_thread = NULL;
static FuriMessageQueue* ardulib_atm_cmd_q = NULL;
static void dma_isr(void* ctx);

static uint8_t ardulib_atm_audio_enabled = 0;

static inline uint8_t ardulib_atm_render_logical_sample_u8() {
    osc[2].phase = (uint16_t)(osc[2].phase + osc[2].freq);
    int8_t phase2 = (int8_t)(osc[2].phase >> 8);
    if(phase2 < 0) phase2 = (int8_t)(~phase2);
    phase2 = (int8_t)(phase2 << 1);
    phase2 = (int8_t)(phase2 - 128);
    int8_t c2 = (int8_t)((((int16_t)phase2 * (int8_t)osc[2].vol) << 1) >> 8);
    int16_t mix = c2;

    osc[0].phase = (uint16_t)(osc[0].phase + osc[0].freq);
    int8_t c0 = (int8_t)osc[0].vol;
    if(osc[0].phase >= 0xC000) c0 = (int8_t)(-c0);
    mix += c0;

    osc[1].phase = (uint16_t)(osc[1].phase + osc[1].freq);
    int8_t c1 = (int8_t)osc[1].vol;
    if(osc[1].phase & 0x8000) c1 = (int8_t)(-c1);
    mix += c1;

    uint16_t freq = osc[3].freq;
    freq <<= 1;
    if(freq & 0x8000) freq ^= 1;
    if(freq & 0x4000) freq ^= 1;
    osc[3].freq = freq;

    int8_t c3 = (int8_t)osc[3].vol;
    if(freq & 0x8000) c3 = (int8_t)(-c3);
    mix += c3;

    const uint16_t gain_q8 = __atomic_load_n(&ardulib_atm_master_gain_q8, __ATOMIC_RELAXED);
    int32_t centered = ((int32_t)mix * (int32_t)gain_q8) >> 9;
    if(centered > 127) centered = 127;
    if(centered < -127) centered = -127;
    int16_t outv = (int16_t)(128 + centered);
    if(outv < 0) outv = 0;
    if(outv > 255) outv = 255;

    ardulib_atm_tick_acc++;
    if(ardulib_atm_tick_acc >= ardulib_atm_tick_div) {
        ardulib_atm_tick_acc = 0;
        __atomic_fetch_add(&ardulib_atm_tick_pending, 1, __ATOMIC_RELAXED);
    }

    return (uint8_t)outv;
}

static inline void ardulib_atm_fill_half(size_t half_index) {
    uint32_t* dst = dma_buf + (half_index * ATM_DMA_SAMPLES_PER_HALF);
    const uint8_t en = __atomic_load_n(&ardulib_atm_audio_enabled, __ATOMIC_RELAXED);
    for(size_t i = 0; i < ATM_LOGICAL_SAMPLES_PER_HALF; i++) {
        uint8_t s = (!en || ardulib_atm_paused) ? 128 : ardulib_atm_render_logical_sample_u8();
        uint32_t duty = (uint32_t)s;
        if(duty > ATM_PWM_ARR) duty = ATM_PWM_ARR;
        dst[i] = duty;
    }
}

static void tim16_dma_start() {
    furi_check(furi_hal_speaker_is_mine());
    furi_hal_gpio_init_ex(
        &gpio_speaker, GpioModeAltFunctionPushPull, GpioPullNo, GpioSpeedVeryHigh, GpioAltFn14TIM16);

    ardulib_atm_tick_div = tick_div_from_rate(tickRate);

    for(size_t i = 0; i < ATM_DMA_TOTAL; i++)
        dma_buf[i] = 128;

    ardulib_atm_tick_acc = 0;
    __atomic_store_n(&ardulib_atm_tick_pending, 0, __ATOMIC_RELAXED);

    ardulib_atm_fill_half(0);
    ardulib_atm_fill_half(1);

    LL_TIM_DisableAllOutputs(TIM16);
    LL_TIM_DisableCounter(TIM16);
    LL_TIM_SetCounter(TIM16, 0);

    LL_TIM_SetPrescaler(TIM16, ATM_PWM_PSC);
    LL_TIM_SetAutoReload(TIM16, ATM_PWM_ARR);
    LL_TIM_EnableARRPreload(TIM16);

    LL_TIM_OC_SetMode(TIM16, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_EnablePreload(TIM16, LL_TIM_CHANNEL_CH1);
    LL_TIM_OC_SetCompareCH1(TIM16, dma_buf[0]);
    LL_TIM_OC_SetPolarity(TIM16, LL_TIM_CHANNEL_CH1, LL_TIM_OCPOLARITY_HIGH);

    LL_TIM_CC_EnableChannel(TIM16, LL_TIM_CHANNEL_CH1);
    LL_TIM_EnableAllOutputs(TIM16);

    LL_DMA_InitTypeDef dma_init;
    memset(&dma_init, 0, sizeof(dma_init));
    dma_init.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_init.PeriphOrM2MSrcAddress = (uint32_t)&TIM16->CCR1;
    dma_init.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_init.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;
    dma_init.MemoryOrM2MDstAddress = (uint32_t)dma_buf;
    dma_init.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_init.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;
    dma_init.Mode = LL_DMA_MODE_CIRCULAR;
    dma_init.NbData = ATM_DMA_TOTAL;
    dma_init.Priority = LL_DMA_PRIORITY_HIGH;
    dma_init.PeriphRequest = LL_DMAMUX_REQ_TIM16_UP;

    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_Init(DMA1, LL_DMA_CHANNEL_1, &dma_init);

    LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);

    furi_hal_interrupt_set_isr_ex(
        FuriHalInterruptIdDma1Ch1, FuriHalInterruptPriorityNormal, dma_isr, NULL);

    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

    LL_TIM_EnableUpdateEvent(TIM16);
    LL_TIM_EnableDMAReq_UPDATE(TIM16);

    LL_TIM_EnableCounter(TIM16);
    LL_TIM_GenerateEvent_UPDATE(TIM16);
}

static void tim16_dma_stop() {
    furi_hal_interrupt_set_isr_ex(
        FuriHalInterruptIdDma1Ch1, FuriHalInterruptPriorityNormal, NULL, NULL);

    LL_TIM_DisableDMAReq_UPDATE(TIM16);

    LL_DMA_DisableIT_HT(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);

    LL_TIM_DisableAllOutputs(TIM16);
    LL_TIM_DisableCounter(TIM16);
}

static void dma_isr(void* /*ctx*/) {
    if(LL_DMA_IsActiveFlag_HT1(DMA1)) {
        LL_DMA_ClearFlag_HT1(DMA1);
        ardulib_atm_fill_half(0);
    }
    if(LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        ardulib_atm_fill_half(1);
    }
}

void ATM_playroutine(void) {
    ch_t* ch;

    for(uint8_t n = 0; n < 4; n++) {
        ch = &channel_state[n];

        if(ch->reConfig) {
            if(ch->reCount >= (ch->reConfig & 0x03)) {
                osc[n].freq = noteTable[ch->reConfig >> 2];
                ch->reCount = 0;
            } else {
                ch->reCount++;
            }
        }

        if(ch->glisConfig) {
            if(ch->glisCount >= (uint8_t)(ch->glisConfig & 0x7F)) {
                if(ch->glisConfig & 0x80)
                    ch->note -= 1;
                else
                    ch->note += 1;

                if(ch->note < 1)
                    ch->note = 1;
                else if(ch->note > 63)
                    ch->note = 63;

                ch->freq = noteTable[ch->note];
                ch->glisCount = 0;
            } else {
                ch->glisCount++;
            }
        }

        if(ch->volFreSlide) {
            if(!ch->volFreCount) {
                int16_t vf = ((ch->volFreConfig & 0x40) ? (int16_t)ch->freq : (int16_t)ch->vol);
                vf += ch->volFreSlide;

                if(!(ch->volFreConfig & 0x80)) {
                    if(vf < 0)
                        vf = 0;
                    else if(ch->volFreConfig & 0x40) {
                        if(vf > 9397) vf = 9397;
                    } else {
                        if(vf > 63) vf = 63;
                    }
                }

                if(ch->volFreConfig & 0x40)
                    ch->freq = (uint16_t)vf;
                else
                    ch->vol = (uint8_t)vf;
            }

            if(ch->volFreCount++ >= (ch->volFreConfig & 0x3F)) ch->volFreCount = 0;
        }

        if(ch->arpNotes && ch->note) {
            if((ch->arpCount & 0x1F) < (ch->arpTiming & 0x1F)) {
                ch->arpCount++;
            } else {
                if((ch->arpCount & 0xE0) == 0x00)
                    ch->arpCount = 0x20;
                else if(
                    (ch->arpCount & 0xE0) == 0x20 && !(ch->arpTiming & 0x40) &&
                    (ch->arpNotes != 0xFF))
                    ch->arpCount = 0x40;
                else
                    ch->arpCount = 0x00;

                uint8_t arpNote = ch->note;

                if((ch->arpCount & 0xE0) != 0x00) {
                    if(ch->arpNotes == 0xFF)
                        arpNote = 0;
                    else
                        arpNote = (uint8_t)(arpNote + (ch->arpNotes >> 4));
                }
                if((ch->arpCount & 0xE0) == 0x40)
                    arpNote = (uint8_t)(arpNote + (ch->arpNotes & 0x0F));

                int16_t idx = (int16_t)arpNote + (int16_t)ch->transConfig;
                if(idx < 0) idx = 0;
                if(idx > 63) idx = 63;
                ch->freq = noteTable[idx];
            }
        }

        if(ch->treviDepth) {
            int16_t vt = ((ch->treviConfig & 0x40) ? (int16_t)ch->freq : (int16_t)ch->vol);
            vt = (ch->treviCount & 0x80) ? (vt + ch->treviDepth) : (vt - ch->treviDepth);

            if(vt < 0)
                vt = 0;
            else if(ch->treviConfig & 0x40) {
                if(vt > 9397) vt = 9397;
            } else {
                if(vt > 63) vt = 63;
            }

            if(ch->treviConfig & 0x40)
                ch->freq = (uint16_t)vt;
            else
                ch->vol = (uint8_t)vt;

            if((ch->treviCount & 0x1F) < (ch->treviConfig & 0x1F))
                ch->treviCount++;
            else
                ch->treviCount = (ch->treviCount & 0x80) ? 0 : 0x80;
        }

        if(ch->delay) {
            if(ch->delay != 0xFFFF) ch->delay--;
        } else {
            do {
                uint8_t cmd = *ch->ptr++;

                if(cmd < 64) {
                    if((ch->note = cmd)) ch->note = (uint8_t)(ch->note + (int8_t)ch->transConfig);

                    int16_t ni = (int16_t)ch->note;
                    if(ni < 0) ni = 0;
                    if(ni > 63) ni = 63;
                    ch->freq = noteTable[ni];

                    if(!ch->volFreConfig) ch->vol = ch->reCount;

                    if(ch->arpTiming & 0x20) ch->arpCount = 0;
                } else if(cmd < 160) {
                    switch(cmd - 64) {
                    case 0:
                        ch->vol = *ch->ptr++;
                        ch->reCount = ch->vol;
                        break;

                    case 1:
                    case 4:
                        ch->volFreSlide = (int8_t)(*ch->ptr++);
                        ch->volFreConfig = ((cmd - 64) == 1) ? 0x00 : 0x40;
                        break;

                    case 2:
                    case 5:
                        ch->volFreSlide = (int8_t)(*ch->ptr++);
                        ch->volFreConfig = *ch->ptr++;
                        break;

                    case 3:
                    case 6:
                        ch->volFreSlide = 0;
                        break;

                    case 7:
                        ch->arpNotes = *ch->ptr++;
                        ch->arpTiming = *ch->ptr++;
                        break;

                    case 8:
                        ch->arpNotes = 0;
                        break;

                    case 9:
                        ch->reConfig = *ch->ptr++;
                        break;

                    case 10:
                        ch->reConfig = 0;
                        break;

                    case 11:
                        ch->transConfig = (int8_t)(ch->transConfig + (int8_t)(*ch->ptr++));
                        break;

                    case 12:
                        ch->transConfig = (int8_t)(*ch->ptr++);
                        break;

                    case 13:
                        ch->transConfig = 0;
                        break;

                    case 14:
                    case 16: {
                        uint16_t depth_w = read_u16_le(&ch->ptr);
                        uint16_t cfg_w = read_u16_le(&ch->ptr);
                        ch->treviDepth = (uint8_t)(depth_w & 0xFF);
                        ch->treviConfig =
                            (uint8_t)((cfg_w & 0xFF) + (((cmd - 64) == 14) ? 0x00 : 0x40));
                        break;
                    }

                    case 15:
                    case 17:
                        ch->treviDepth = 0;
                        break;

                    case 18:
                        ch->glisConfig = (int8_t)(*ch->ptr++);
                        break;

                    case 19:
                        ch->glisConfig = 0;
                        break;

                    case 20:
                        ch->arpNotes = 0xFF;
                        ch->arpTiming = *ch->ptr++;
                        break;

                    case 21:
                        ch->arpNotes = 0;
                        break;

                    case 92:
                        tickRate = (uint8_t)(tickRate + *ch->ptr++);
                        if(tickRate < 1) tickRate = 1;
                        ardulib_atm_tick_div = tick_div_from_rate(tickRate);
                        break;

                    case 93:
                        tickRate = *ch->ptr++;
                        if(tickRate < 1) tickRate = 1;
                        ardulib_atm_tick_div = tick_div_from_rate(tickRate);
                        break;

                    case 94:
                        for(uint8_t i = 0; i < 4; i++)
                            channel_state[i].repeatPoint = *ch->ptr++;
                        break;

                    case 95:
                        ChannelActiveMute = (uint8_t)(ChannelActiveMute ^ (1 << (n + 4)));
                        ch->vol = 0;
                        ch->delay = 0xFFFF;
                        break;

                    default:
                        break;
                    }
                } else if(cmd < 224) {
                    ch->delay = (uint16_t)(cmd - 159);
                } else if(cmd == 224) {
                    ch->delay = (uint16_t)(read_vle(&ch->ptr) + 65);
                } else if(cmd == 252 || cmd == 253) {
                    uint8_t new_counter = (cmd == 252) ? 0 : *ch->ptr++;
                    uint8_t new_track = *ch->ptr++;

                    if(new_track != ch->track) {
                        ch->stackCounter[ch->stackIndex] = ch->counter;
                        ch->stackTrack[ch->stackIndex] = ch->track;
                        ch->stackPointer[ch->stackIndex] = (uint16_t)(ch->ptr - trackBase);
                        ch->stackIndex++;
                        ch->track = new_track;
                    }
                    ch->counter = new_counter;
                    ch->ptr = getTrackPointer(ch->track);
                } else if(cmd == 254) {
                    if(ch->counter > 0 || ch->stackIndex == 0) {
                        if(ch->counter) ch->counter--;
                        ch->ptr = getTrackPointer(ch->track);
                    } else {
                        if(ch->stackIndex == 0) {
                            ch->delay = 0xFFFF;
                        } else {
                            ch->stackIndex--;
                            ch->ptr = ch->stackPointer[ch->stackIndex] + trackBase;
                            ch->counter = ch->stackCounter[ch->stackIndex];
                            ch->track = ch->stackTrack[ch->stackIndex];
                        }
                    }
                } else if(cmd == 255) {
                    ch->ptr += read_vle(&ch->ptr);
                } else {
                }
            } while(ch->delay == 0);

            if(ch->delay != 0xFFFF) ch->delay--;
        }

        if(!(ChannelActiveMute & (1 << n))) {
            if(n == 3) {
                osc[n].vol = (uint8_t)(ch->vol >> 1);
            } else {
                osc[n].freq = ch->freq;
                osc[n].vol = ch->vol;
            }
        }

        if(!(ChannelActiveMute & 0xF0)) {
            uint8_t repeatSong = 0;
            for(uint8_t j = 0; j < 4; j++)
                repeatSong = (uint8_t)(repeatSong + channel_state[j].repeatPoint);

            if(repeatSong) {
                for(uint8_t k = 0; k < 4; k++) {
                    channel_state[k].ptr = getTrackPointer(channel_state[k].repeatPoint);
                    channel_state[k].delay = 0;
                }
                ChannelActiveMute = 0b11110000;
            } else {
                ATMsynth::stop();
            }
        }
    }
}

enum AtmCmdType : uint8_t {
    AtmCmdPlay,
    AtmCmdStop,
    AtmCmdTogglePause,
    AtmCmdMute,
    AtmCmdUnmute,
    AtmCmdSetVolume,
    AtmCmdQuit,
};

struct AtmCmd {
    AtmCmdType type;
    union {
        struct {
            const uint8_t* song;
        } play;
        struct {
            uint8_t ch;
        } ch;
        struct {
            float v;
        } vol;
    } u;
};

static inline void push_cmd(const AtmCmd& c) {
    if(!ardulib_atm_cmd_q) ATMsynth::systemInit();
    furi_message_queue_put(ardulib_atm_cmd_q, &c, FuriWaitForever);
}

static int32_t ardulib_atm_thread_fn(void* /*ctx*/) {
    AtmCmd cmd;
    bool speaker_owned = false;

    while(true) {
        if(furi_message_queue_get(ardulib_atm_cmd_q, &cmd, 10) == FuriStatusOk) {
            if(cmd.type == AtmCmdStop) {
                ardulib_atm_running = false;
                ardulib_atm_paused = false;

                if(speaker_owned) {
                    tim16_dma_stop();
                    furi_hal_speaker_release();
                    speaker_owned = false;
                }

                memset(channel_state, 0, sizeof(channel_state));
                ChannelActiveMute = 0b11110000;
                continue;
            }

            if(cmd.type == AtmCmdQuit) {
                ardulib_atm_running = false;
                ardulib_atm_paused = false;

                if(speaker_owned) {
                    tim16_dma_stop();
                    furi_hal_speaker_release();
                    speaker_owned = false;
                }

                memset(channel_state, 0, sizeof(channel_state));
                ChannelActiveMute = 0b11110000;
                break;
            }

            if(cmd.type == AtmCmdTogglePause) {
                if(ardulib_atm_running) ardulib_atm_paused = !ardulib_atm_paused;
                continue;
            }

            if(cmd.type == AtmCmdMute) {
                ChannelActiveMute = (uint8_t)(ChannelActiveMute | (1 << cmd.u.ch.ch));
                continue;
            }

            if(cmd.type == AtmCmdUnmute) {
                ChannelActiveMute = (uint8_t)(ChannelActiveMute & (uint8_t)~(1 << cmd.u.ch.ch));
                continue;
            }

            if(cmd.type == AtmCmdSetVolume) {
                float v = cmd.u.vol.v;
                if(v < 0.0f) v = 0.0f;
                if(v > ATM_MASTER_GAIN_MAX) v = ATM_MASTER_GAIN_MAX;
                __atomic_store_n(
                    &ardulib_atm_master_gain_q8, (uint16_t)(v * 256.0f + 0.5f), __ATOMIC_RELAXED);
                continue;
            }

            if(cmd.type == AtmCmdPlay) {
                const uint8_t* song = cmd.u.play.song;

                memset(channel_state, 0, sizeof(channel_state));
                ChannelActiveMute = 0b11110000;

                tickRate = 25;
                ardulib_atm_tick_div = tick_div_from_rate(tickRate);

                osc[3].freq = 0x0001;
                channel_state[3].freq = 0x0001;

                trackCount = *song++;
                trackList = (const uint16_t*)song;
                song += (trackCount << 1);
                trackBase = song + 4;

                for(uint8_t n = 0; n < 4; n++) {
                    channel_state[n].ptr = getTrackPointer(*song++);
                }

                ardulib_atm_running = true;
                ardulib_atm_paused = false;

                const uint8_t en = __atomic_load_n(&ardulib_atm_audio_enabled, __ATOMIC_RELAXED);
                if(en && !speaker_owned) {
                    if(furi_hal_speaker_acquire(200)) {
                        speaker_owned = true;
                        tim16_dma_start();
                    } else {
                        ardulib_atm_running = false;
                    }
                }
                continue;
            }
        }

        const uint8_t en = __atomic_load_n(&ardulib_atm_audio_enabled, __ATOMIC_RELAXED);

        if(!en) {
            if(speaker_owned) {
                tim16_dma_stop();
                furi_hal_speaker_release();
                speaker_owned = false;
            }
        } else {
            if(ardulib_atm_running && !speaker_owned) {
                if(furi_hal_speaker_acquire(200)) {
                    speaker_owned = true;
                    tim16_dma_start();
                }
            }
        }

        if(ardulib_atm_running && en && !ardulib_atm_paused) {
            uint32_t pending = __atomic_load_n(&ardulib_atm_tick_pending, __ATOMIC_RELAXED);
            if(pending) {
                __atomic_fetch_sub(&ardulib_atm_tick_pending, 1, __ATOMIC_RELAXED);
                ATM_playroutine();
            }
        }
    }
    return 0;
}

ATMsynth ATM;

void ATMsynth::systemInit() {
    if(ardulib_atm_cmd_q) return;
    __atomic_store_n(&ardulib_atm_audio_enabled, g_arduboy_audio_enabled ? 1 : 0, __ATOMIC_RELAXED);
    ardulib_atm_cmd_q = furi_message_queue_alloc(8, sizeof(AtmCmd));
    if(!ardulib_atm_cmd_q) return;

    ardulib_atm_thread = furi_thread_alloc();
    if(!ardulib_atm_thread) {
        furi_message_queue_free(ardulib_atm_cmd_q);
        ardulib_atm_cmd_q = NULL;
        return;
    }

    furi_thread_set_name(ardulib_atm_thread, "ArdulibATM");
    furi_thread_set_stack_size(ardulib_atm_thread, 2048);
    furi_thread_set_priority(ardulib_atm_thread, FuriThreadPriorityHigh);
    furi_thread_set_callback(ardulib_atm_thread, ardulib_atm_thread_fn);
    furi_thread_start(ardulib_atm_thread);
}

void ATMsynth::systemDeinit() {
    if(!ardulib_atm_cmd_q || !ardulib_atm_thread) {
        ardulib_atm_running = false;
        ardulib_atm_paused = false;
        __atomic_store_n(&ardulib_atm_tick_pending, 0, __ATOMIC_RELAXED);
        return;
    }

    AtmCmd c{};
    c.type = AtmCmdQuit;
    furi_message_queue_put(ardulib_atm_cmd_q, &c, FuriWaitForever);
    __atomic_store_n(&ardulib_atm_tick_pending, 0, __ATOMIC_RELAXED);
    furi_thread_join(ardulib_atm_thread);
    furi_thread_free(ardulib_atm_thread);
    furi_message_queue_free(ardulib_atm_cmd_q);
    ardulib_atm_cmd_q = NULL;
    ardulib_atm_thread = NULL;
}

void ATMsynth::play(const uint8_t* song) {
    AtmCmd c{};
    c.type = AtmCmdPlay;
    c.u.play.song = song;
    push_cmd(c);
}

void ATMsynth::stop() {
    AtmCmd c{};
    c.type = AtmCmdStop;
    push_cmd(c);
}

void ATMsynth::playPause() {
    AtmCmd c{};
    c.type = AtmCmdTogglePause;
    push_cmd(c);
}

void ATMsynth::muteChannel(uint8_t ch) {
    AtmCmd c{};
    c.type = AtmCmdMute;
    c.u.ch.ch = ch;
    push_cmd(c);
}

void ATMsynth::unMuteChannel(uint8_t ch) {
    AtmCmd c{};
    c.type = AtmCmdUnmute;
    c.u.ch.ch = ch;
    push_cmd(c);
}

void ATMsynth::setEnabled(bool en) {
    __atomic_store_n(&ardulib_atm_audio_enabled, en ? 1 : 0, __ATOMIC_RELAXED);
}
#endif

/*
 * utd_proto - vedi utd_proto.h
 *
 * La logica di scansione (trigger "a meta' buffer" oppure "burst finito")
 * e l'estrazione dei campi seguono proto6_bridge.c di TPMS-Editor-EN.
 */
#include "utd_proto.h"
#include "protocols/proto6/proto6_core.h"
#include "protocols/proto6/custom_presets.h"

#include <furi_hal.h>
#include <lib/toolbox/level_duration.h>
#include <string.h>

#define UTD_TAG "UtdProto"

/* definiti qui perche' proto6_bridge.c non fa parte di questa app */
RawSamplesBuffer* RawSamples = NULL;
RawSamplesBuffer* DetectedSamples = NULL;

static ProtoViewApp g_pv;
static bool g_ready = false;
static uint32_t g_last_observed_idx = 0;
static uint32_t g_last_progress_tick = 0;

/* ------------------------------------------------------------------------ */
/* init / deinit                                                              */
/* ------------------------------------------------------------------------ */

bool utd_init(void) {
    if(g_ready) return true;
    RawSamples = raw_samples_alloc();
    DetectedSamples = raw_samples_alloc();
    if(!RawSamples || !DetectedSamples) {
        if(RawSamples) raw_samples_free(RawSamples);
        if(DetectedSamples) raw_samples_free(DetectedSamples);
        RawSamples = NULL;
        DetectedSamples = NULL;
        return false;
    }
    memset(&g_pv, 0, sizeof(g_pv));
    g_pv.modulation = ProtoTpmsModulationFSK;
    reset_current_signal(&g_pv);
    g_last_observed_idx = 0;
    g_last_progress_tick = furi_get_tick();
    g_ready = true;
    return true;
}

void utd_deinit(void) {
    if(!g_ready) return;
    free_msg_info(g_pv.msg_info);
    g_pv.msg_info = NULL;
    raw_samples_free(RawSamples);
    raw_samples_free(DetectedSamples);
    RawSamples = NULL;
    DetectedSamples = NULL;
    g_ready = false;
}

/* ------------------------------------------------------------------------ */
/* RX                                                                         */
/* ------------------------------------------------------------------------ */

void utd_rx_reset(bool ook) {
    if(!g_ready) return;
    g_pv.modulation = ook ? ProtoTpmsModulationOOK : ProtoTpmsModulationFSK;
    reset_current_signal(&g_pv);
    g_pv.signal_last_scan_idx = 0;
    g_last_observed_idx = 0;
    g_last_progress_tick = furi_get_tick();
}

void utd_rx_feed(bool level, uint32_t duration_us) {
    if(!g_ready || !RawSamples) return;
    raw_samples_add(RawSamples, level, duration_us);
}

static uint32_t read_id(const ProtoViewField* f, uint8_t* len_out) {
    if(len_out) *len_out = 0;
    if(!f || f->type != FieldTypeBytes || !f->bytes) return 0;
    uint32_t count = (f->len + 1U) / 2U;
    if(count > UTD_ID_MAX) count = UTD_ID_MAX;
    uint32_t id = 0;
    for(uint32_t i = 0; i < count; i++) id = (id << 8U) | f->bytes[i];
    if(len_out) *len_out = (uint8_t)count;
    return id;
}

static void extract_frame(const ProtoViewMsgInfo* info, UtdFrame* out) {
    memset(out, 0, sizeof(*out));
    out->kind = tpms_encoder_for_model(info->decoder->name);

    ProtoViewFieldSet* fs = info->fieldset;
    ProtoViewField* id = proto6_field_find(fs, "Tire ID");
    ProtoViewField* p_kpa = proto6_field_find(fs, "Pressure kpa");
    ProtoViewField* p_psi = proto6_field_find(fs, "Pressure psi");
    ProtoViewField* temp = proto6_field_find(fs, "Temperature C");
    ProtoViewField* flags = proto6_field_find(fs, "Flags");
    ProtoViewField* status = proto6_field_find(fs, "Status");
    ProtoViewField* moving = proto6_field_find(fs, "Moving");
    ProtoViewField* repeat = proto6_field_find(fs, "Repeat");
    ProtoViewField* temp_f = proto6_field_find(fs, "Temperature F Raw");
    ProtoViewField* raw = proto6_field_find(fs, "Raw payload");
    ProtoViewField* raw_bits = proto6_field_find(fs, "Raw payload bits");

    out->id = read_id(id, &out->id_len);

    if(p_kpa && p_kpa->type == FieldTypeFloat) {
        out->kpa = p_kpa->fvalue;
    } else if(p_psi && p_psi->type == FieldTypeFloat) {
        out->kpa = p_psi->fvalue * 6.894757293168f;
    }
    if(temp && temp->type == FieldTypeSignedInt) {
        out->temp_c = (int16_t)temp->value;
        out->temp_valid = true;
    }

    if(flags) {
        out->flags = (uint32_t)flags->uvalue;
    } else if(status) {
        out->flags = (uint32_t)status->uvalue;
    } else if(moving) {
        out->flags = (uint32_t)moving->uvalue;
    } else if(repeat) {
        out->flags = (uint32_t)repeat->uvalue;
    }

    if(temp_f) {
        out->temp_f_raw = (uint8_t)temp_f->uvalue;
        out->temp_f_valid = true;
    }

    if(raw && raw->type == FieldTypeBytes && raw->bytes) {
        uint32_t bytes = (raw->len + 1U) / 2U;
        if(bytes > UTD_RAW_MAX) bytes = UTD_RAW_MAX;
        memcpy(out->raw, raw->bytes, bytes);
        out->raw_size = (uint8_t)bytes;
        uint32_t bits = raw_bits ? (uint32_t)raw_bits->uvalue : bytes * 8U;
        if(bits > 128U) bits = 128U;
        out->raw_bits = (uint8_t)bits;
        out->raw_valid = bytes > 0U;
    }
}

bool utd_rx_scan(UtdFrame* out) {
    if(!g_ready || !RawSamples) return false;

    uint32_t current_idx;
    uint32_t total;
    furi_mutex_acquire(RawSamples->mutex, FuriWaitForever);
    current_idx = RawSamples->idx;
    total = RawSamples->total;
    furi_mutex_release(RawSamples->mutex);

    const uint32_t now = furi_get_tick();
    if(current_idx != g_last_observed_idx) {
        g_last_observed_idx = current_idx;
        g_last_progress_tick = now;
    }

    const uint32_t last_idx = g_pv.signal_last_scan_idx;
    uint32_t delta;
    if(last_idx <= current_idx) {
        delta = current_idx - last_idx;
    } else {
        delta = total - last_idx + current_idx;
    }

    /* come nel bridge: scansione a meta' buffer, oppure a burst finito */
    const bool half_buffer_ready = delta >= total / 2U;
    const bool quiet_burst_ready =
        delta >= 18U && (now - g_last_progress_tick) >= furi_ms_to_ticks(50U);
    if(!half_buffer_ready && !quiet_burst_ready) return false;

    g_pv.signal_last_scan_idx = current_idx;
    scan_for_signal(&g_pv, RawSamples, 30U);
    /* Mercedes Sprinter usa impulsi da 25 us, sotto la soglia di 30 us: seconda
     * passata piu' tollerante, solo se la prima non ha decodificato nulla. */
    if(!g_pv.signal_decoded) scan_for_signal(&g_pv, RawSamples, 20U);

    if(!g_pv.signal_decoded || !g_pv.msg_info || !g_pv.msg_info->decoder) return false;

    extract_frame(g_pv.msg_info, out);

    reset_current_signal(&g_pv);
    g_pv.signal_last_scan_idx = 0;
    g_last_observed_idx = 0;
    g_last_progress_tick = now;
    return out->kind != TpmsEncoderNone && out->id_len > 0U;
}

/* ------------------------------------------------------------------------ */
/* profili radio                                                              */
/* ------------------------------------------------------------------------ */

uint32_t utd_profile_freq(uint8_t profile) {
    return (profile >= 3U) ? 433920000UL : 315000000UL;
}

bool utd_profile_is_ook(uint8_t profile) {
    return (profile % 3U) == 1U;
}

const uint8_t* utd_profile_preset(uint8_t profile) {
    switch(profile % 3U) {
    case 1:
        return (const uint8_t*)protoview_subghz_tpms2_ook_async_regs;
    case 2:
        return (const uint8_t*)protoview_subghz_tpms3_gfsk_async_regs;
    default:
        return (const uint8_t*)protoview_subghz_tpms1_fsk_async_regs;
    }
}

const char* utd_profile_name(uint8_t profile) {
    static const char* const names[UTD_PROFILES] = {
        "315 FSK", "315 OOK", "315 GFSK", "433 FSK", "433 OOK", "433 GFSK"};
    return names[profile % UTD_PROFILES];
}

uint8_t utd_profile_auto_order(uint8_t n) {
    /* 433 FSK, 315 FSK, 433 OOK, 315 OOK, 433 GFSK, 315 GFSK */
    static const uint8_t order[UTD_PROFILES] = {3, 0, 4, 1, 5, 2};
    return order[n % UTD_PROFILES];
}

/* ------------------------------------------------------------------------ */
/* informazioni sui protocolli                                                */
/* ------------------------------------------------------------------------ */

static const char* const k_short[TpmsEncoderCount] = {
    [TpmsEncoderNone] = "None",
    [TpmsEncoderRenault] = "Renault",
    [TpmsEncoderToyota] = "Toyota C210",
    [TpmsEncoderSchrader] = "Schrader",
    [TpmsEncoderSchraderEG53MA4] = "Schr EG53",
    [TpmsEncoderCitroen] = "VDO/PSA/FCA",
    [TpmsEncoderFord] = "Ford",
    [TpmsEncoderBMWGen23] = "BMW Gen2/3",
    [TpmsEncoderElantra2012] = "Elantra",
    [TpmsEncoderHyundaiVDO] = "Hyundai VDO",
    [TpmsEncoderTruck] = "Truck Solar",
    [TpmsEncoderRenault0435R] = "Ren 0435R",
    [TpmsEncoderHondaTRW] = "Honda TRW",
    [TpmsEncoderPorsche] = "Porsche",
    [TpmsEncoderKia] = "Kia",
    [TpmsEncoderPMV107J] = "PMV-107J",
    [TpmsEncoderTG1C] = "VDO TG1C",
    [TpmsEncoderQ85] = "Q85",
    [TpmsEncoderMercedesSprinter] = "Sprinter",
};

const char* utd_kind_short(TpmsEncoderKind kind) {
    if(kind >= TpmsEncoderCount) return "?";
    return k_short[kind];
}

uint8_t utd_kind_maturity(TpmsEncoderKind kind) {
    if(kind < TpmsEncoderBMWGen23) return 0;
    if(kind <= TpmsEncoderKia) return 2; /* EXP */
    return 1; /* TEST */
}

static ProtoViewDecoder* decoder_by_name(const char* name) {
    for(size_t i = 0; Decoders[i]; i++) {
        if(strcmp(Decoders[i]->name, name) == 0) return Decoders[i];
    }
    return NULL;
}

bool utd_kind_is_ook(TpmsEncoderKind kind) {
    ProtoViewDecoder* d = decoder_by_name(tpms_encoder_name(kind));
    return d && tpms_decoder_modulation(d) == ProtoTpmsModulationOOK;
}

uint8_t utd_kind_id_len(TpmsEncoderKind kind) {
    static uint8_t cache[TpmsEncoderCount];
    if(kind <= TpmsEncoderNone || kind >= TpmsEncoderCount) return 4;
    if(cache[kind]) return cache[kind];

    uint8_t len = 4;
    ProtoViewDecoder* d = decoder_by_name(tpms_encoder_name(kind));
    if(d && d->get_fields) {
        ProtoViewFieldSet* fs = fieldset_new();
        if(fs) {
            d->get_fields(fs);
            ProtoViewField* f = proto6_field_find(fs, "Tire ID");
            if(f && f->type == FieldTypeBytes) {
                uint32_t n = (f->len + 1U) / 2U;
                if(n >= 1U && n <= UTD_ID_MAX) len = (uint8_t)n;
            }
            fieldset_free(fs);
        }
    }
    cache[kind] = len;
    return len;
}

uint8_t utd_kind_default_profile(TpmsEncoderKind kind) {
    if(kind == TpmsEncoderPMV107J) return 0; /* 315 FSK */
    if(utd_kind_is_ook(kind)) return 4;      /* 433 OOK */
    return 3;                                /* 433 FSK */
}

uint16_t utd_kind_step_kpa(TpmsEncoderKind kind) {
    float s = tpms_encoder_pressure_step_kpa(kind);
    uint16_t v = (uint16_t)(s + 0.5f);
    return v < 1U ? 1U : v;
}

/* ------------------------------------------------------------------------ */
/* TX                                                                         */
/* ------------------------------------------------------------------------ */

#define UTD_REPEAT_GAP_US 10000U

typedef struct {
    const TpmsTxWave* wave;
    size_t index;
    uint8_t repeat;
    uint8_t repeat_target;
} UtdTxCtx;

static UtdTxCtx g_txc;

static LevelDuration utd_tx_yield(void* context) {
    UtdTxCtx* t = context;
    if(!t || !t->wave || t->wave->pulse_count == 0U) return level_duration_reset();

    if(t->index >= t->wave->pulse_count) {
        t->repeat++;
        if(t->repeat >= t->repeat_target) return level_duration_reset();
        t->index = 0;
        return level_duration_make(false, UTD_REPEAT_GAP_US);
    }
    const TpmsPulse* p = &t->wave->pulses[t->index++];
    return level_duration_make(p->level, p->duration_us);
}

bool utd_tx_allowed(uint8_t profile) {
    return furi_hal_subghz_is_tx_allowed(utd_profile_freq(profile));
}

bool utd_transmit(const UtdTx* tx) {
    if(!tx || tx->kind == TpmsEncoderNone || tx->kind >= TpmsEncoderCount) return false;

    TpmsEditValues v;
    tpms_encoder_default_values(tx->kind, &v);
    v.id = tx->id;
    v.pressure_kpa = tx->kpa;
    v.temperature_c = (float)tx->temp_c;
    if(tx->raw_valid) {
        v.flags = tx->flags; /* senza payload catturato restano i flag predefiniti del protocollo */
        v.temperature_raw_f = tx->temp_f_raw;
        v.temperature_raw_f_valid = tx->temp_f_valid;
    }
    v.frequency_hz = utd_profile_freq(tx->profile);
    v.edit_mask = tx->edit_mask;
    v.repeat_count = tx->repeats ? tx->repeats : 3U;
    if(tx->raw_valid && tx->raw_size > 0U && tx->raw_size <= sizeof(v.raw_payload)) {
        memcpy(v.raw_payload, tx->raw, tx->raw_size);
        v.raw_payload_size = tx->raw_size;
        v.raw_payload_bits = tx->raw_bits;
        v.raw_payload_valid = true;
    }

    TpmsTxWave* wave = malloc(sizeof(TpmsTxWave));
    if(!wave) return false;
    if(!tpms_encoder_build(&v, wave) || wave->pulse_count == 0U || wave->overflowed) {
        FURI_LOG_E(UTD_TAG, "frame build failed (kind %d)", (int)tx->kind);
        free(wave);
        return false;
    }

    const uint32_t freq = utd_profile_freq(tx->profile);
    if(!furi_hal_subghz_is_tx_allowed(freq)) {
        FURI_LOG_E(UTD_TAG, "TX not allowed on %lu", (unsigned long)freq);
        free(wave);
        return false;
    }

    uint8_t repeat = v.repeat_count;
    if(repeat > 10U) repeat = 10U;

    g_txc.wave = wave;
    g_txc.index = 0;
    g_txc.repeat = 0;
    g_txc.repeat_target = repeat;

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(utd_profile_preset(tx->profile));
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_cc1101_g0, false);
    furi_hal_subghz_set_frequency_and_path(freq);

    bool ok = furi_hal_subghz_start_async_tx(utd_tx_yield, &g_txc);
    if(ok) {
        const uint32_t started = furi_get_tick();
        const uint32_t timeout = furi_ms_to_ticks(10000U);
        while(!furi_hal_subghz_is_async_tx_complete()) {
            if((furi_get_tick() - started) > timeout) break;
            furi_delay_ms(2U);
        }
        ok = furi_hal_subghz_is_async_tx_complete();
        furi_hal_subghz_stop_async_tx();
    }
    furi_hal_subghz_sleep();

    g_txc.wave = NULL;
    free(wave);
    return ok;
}

#include "tpms_encoder.h"
#include "protocols/proto6/proto6_core.h"

#include <string.h>

#define PSI_TO_KPA 6.894757293168f

static const TpmsEncoderKind test_kinds[] = {
    TpmsEncoderRenault,
    TpmsEncoderToyota,
    TpmsEncoderSchrader,
    TpmsEncoderSchraderEG53MA4,
    TpmsEncoderCitroen,
    TpmsEncoderFord,
    TpmsEncoderBMWGen23,
    TpmsEncoderElantra2012,
    TpmsEncoderHyundaiVDO,
    TpmsEncoderTruck,
    TpmsEncoderRenault0435R,
    TpmsEncoderHondaTRW,
    TpmsEncoderPorsche,
    TpmsEncoderKia,
    TpmsEncoderPMV107J,
    TpmsEncoderTG1C,
    TpmsEncoderQ85,
    TpmsEncoderMercedesSprinter,
};

static ProtoViewDecoder* decoder_for_kind(TpmsEncoderKind kind) {
    const char* name = tpms_encoder_name(kind);
    for(size_t i = 0U; Decoders[i]; i++) {
        if(strcmp(Decoders[i]->name, name) == 0) return Decoders[i];
    }
    return NULL;
}

static void wave_add(TpmsTxWave* wave, bool level, uint32_t duration) {
    if(!duration || wave->overflowed) return;
    if(wave->pulse_count && wave->pulses[wave->pulse_count - 1U].level == level) {
        wave->pulses[wave->pulse_count - 1U].duration_us += duration;
        return;
    }
    if(wave->pulse_count >= TPMS_TX_MAX_PULSES) {
        wave->overflowed = true;
        return;
    }
    wave->pulses[wave->pulse_count].level = level;
    wave->pulses[wave->pulse_count].duration_us = duration;
    wave->pulse_count++;
}

static void set_id_field(ProtoViewField* field, uint32_t id) {
    if(!field || field->type != FieldTypeBytes || !field->bytes) return;
    uint32_t count = (field->len + 1U) / 2U;
    if(count > 4U) count = 4U;
    for(uint32_t i = 0U; i < count; i++) {
        const uint32_t shift = (count - i - 1U) * 8U;
        field->bytes[i] = (uint8_t)(id >> shift);
    }
}

static void set_builder_fields(ProtoViewFieldSet* fields, const TpmsEditValues* values) {
    ProtoViewField* field = proto6_field_find(fields, "Tire ID");
    set_id_field(field, values->id);

    field = proto6_field_find(fields, "Pressure kpa");
    if(field && field->type == FieldTypeFloat) field->fvalue = (float)values->pressure_kpa;

    field = proto6_field_find(fields, "Pressure psi");
    if(field && field->type == FieldTypeFloat) {
        field->fvalue = (float)values->pressure_kpa / PSI_TO_KPA;
    }

    field = proto6_field_find(fields, "Temperature C");
    if(field && field->type == FieldTypeSignedInt) {
        field->value = (int64_t)values->temperature_c;
    }

    field = proto6_field_find(fields, "Temperature F Raw");
    if(field) {
        uint8_t temp_f = values->temperature_raw_f;
        if(!values->temperature_raw_f_valid) {
            int32_t converted =
                (int32_t)(values->temperature_c * 9.0f / 5.0f + 32.0f + 0.5f);
            if(converted < 0) converted = 0;
            if(converted > 255) converted = 255;
            temp_f = (uint8_t)converted;
        }
        field->uvalue = temp_f;
    }

    field = proto6_field_find(fields, "Flags");
    if(field) field->uvalue = values->flags;
    field = proto6_field_find(fields, "Status");
    if(field) field->uvalue = values->flags;
    field = proto6_field_find(fields, "Moving");
    if(field) field->uvalue = values->flags & 1U;

    fieldset_add_uint(fields, "Edit mask", values->edit_mask, 8U);

    if(values->raw_payload_valid && values->raw_payload_size > 0U &&
       values->raw_payload_size <= sizeof(values->raw_payload)) {
        fieldset_add_bytes(
            fields,
            "Raw payload",
            values->raw_payload,
            (uint32_t)values->raw_payload_size * 2U);
        fieldset_add_uint(
            fields,
            "Raw payload bits",
            values->raw_payload_bits ? values->raw_payload_bits :
                                       (uint32_t)values->raw_payload_size * 8U,
            8U);
    }
}

const char* tpms_encoder_name(TpmsEncoderKind kind) {
    switch(kind) {
    case TpmsEncoderRenault: return "Renault TPMS";
    case TpmsEncoderToyota: return "Toyota TPMS";
    case TpmsEncoderSchrader: return "Schrader TPMS";
    case TpmsEncoderSchraderEG53MA4: return "Schrader EG53MA4 TPMS";
    case TpmsEncoderCitroen: return "Citroen TPMS";
    case TpmsEncoderFord: return "Ford TPMS";
    case TpmsEncoderBMWGen23: return "BMW Gen2/3 EXP";
    case TpmsEncoderElantra2012: return "Elantra/Honda EXP";
    case TpmsEncoderHyundaiVDO: return "Hyundai VDO EXP";
    case TpmsEncoderTruck: return "Truck Solar EXP";
    case TpmsEncoderRenault0435R: return "Renault 0435R EXP";
    case TpmsEncoderHondaTRW: return "Honda TRW EXP";
    case TpmsEncoderPorsche: return "Porsche EXP";
    case TpmsEncoderKia: return "Kia EXP";
    case TpmsEncoderPMV107J: return "Toyota PMV-107J TEST";
    case TpmsEncoderTG1C: return "VDO TG1C/Abarth TEST";
    case TpmsEncoderQ85: return "Shenzhen Q85 TEST";
    case TpmsEncoderMercedesSprinter: return "Mercedes Sprinter TEST";
    default: return "None";
    }
}

const char* tpms_protocol_display_name(const char* internal_name) {
    if(!internal_name) return "None";
    if(strcmp(internal_name, "Renault TPMS") == 0) return "Renault";
    if(strcmp(internal_name, "Toyota TPMS") == 0) return "Pacific C210/Toyota";
    if(strcmp(internal_name, "Schrader TPMS") == 0) return "Schrader";
    if(strcmp(internal_name, "Schrader EG53MA4 TPMS") == 0) return "Schrader EG53MA4";
    if(strcmp(internal_name, "Citroen TPMS") == 0) return "VDO/PSA/FCA";
    if(strcmp(internal_name, "Ford TPMS") == 0) return "Ford";
    if(strcmp(internal_name, "Toyota PMV-107J TEST") == 0) return "Pacific PMV-107J TEST";
    return internal_name;
}

const char* tpms_protocol_list_name(const char* internal_name) {
    if(!internal_name) return "None";
    /* TPMS 4.0: keep the live RX row short enough to leave room for the full ID.
       The detailed screen still uses tpms_protocol_display_name(). */
    if(strcmp(internal_name, "Toyota TPMS") == 0) return "Pacific C210";
    return tpms_protocol_display_name(internal_name);
}

const char* tpms_radio_preset_name(uint8_t preset) {
    switch(preset) {
    case TpmsRadioPresetFSK: return "FSK";
    case TpmsRadioPresetOOK: return "OOK";
    case TpmsRadioPresetGFSK: return "GFSK";
    default: return "AUTO";
    }
}

uint8_t tpms_radio_preset_from_name(const char* preset_name) {
    if(!preset_name) return TpmsRadioPresetAuto;
    if(strstr(preset_name, "GFSK")) return TpmsRadioPresetGFSK;
    if(strstr(preset_name, "OOK") || strstr(preset_name, "AM")) return TpmsRadioPresetOOK;
    if(strstr(preset_name, "FSK") || strstr(preset_name, "FM")) return TpmsRadioPresetFSK;
    return TpmsRadioPresetAuto;
}

const char* tpms_wake_profile_name(uint8_t profile) {
    switch(profile) {
    case TpmsWakeProfileFord5A5A: return "Ford 5A5A";
    case TpmsWakeProfileVDOFCA615E: return "VDO/FCA 615E";
    case TpmsWakeProfileAutoFordVDO: return "AUTO Ford+VDO";
    default: return "NONE";
    }
}

const char* tpms_wake_profile_short_name(uint8_t profile) {
    switch(profile) {
    case TpmsWakeProfileFord5A5A: return "Ford 5A5A";
    case TpmsWakeProfileVDOFCA615E: return "VDO/FCA 615E";
    case TpmsWakeProfileAutoFordVDO: return "AUTO Ford+VDO";
    default: return "-";
    }
}

const char* tpms_encoder_display_name(TpmsEncoderKind kind) {
    return tpms_protocol_display_name(tpms_encoder_name(kind));
}

TpmsEncoderKind tpms_encoder_for_model(const char* model) {
    if(!model) return TpmsEncoderNone;
    for(size_t i = 0U; i < COUNT_OF(test_kinds); i++) {
        const TpmsEncoderKind kind = test_kinds[i];
        if(strcmp(model, tpms_encoder_name(kind)) == 0 ||
           strcmp(model, tpms_encoder_display_name(kind)) == 0) return kind;
    }
    return TpmsEncoderNone;
}

bool tpms_encoder_build(const TpmsEditValues* values, TpmsTxWave* wave) {
    if(!values || !wave || values->kind == TpmsEncoderNone) return false;
    memset(wave, 0, sizeof(*wave));

    ProtoViewDecoder* decoder = decoder_for_kind(values->kind);
    if(!decoder || !decoder->get_fields || !decoder->build_message) return false;

    ProtoViewFieldSet* fields = fieldset_new();
    if(!fields) return false;
    decoder->get_fields(fields);
    set_builder_fields(fields, values);

    RawSamplesBuffer* samples = raw_samples_alloc();
    if(!samples) {
        fieldset_free(fields);
        return false;
    }
    decoder->build_message(samples, fields);

    furi_mutex_acquire(samples->mutex, FuriWaitForever);
    const uint32_t count = samples->idx > RAW_SAMPLES_NUM ? RAW_SAMPLES_NUM : samples->idx;
    for(uint32_t i = 0U; i < count; i++) {
        const bool level = samples->samples[i].level != 0U;
        const uint32_t duration = samples->samples[i].dur;
        wave_add(wave, level, duration);
    }
    furi_mutex_release(samples->mutex);

    wave->frequency_hz = values->frequency_hz ? values->frequency_hz : 433920000U;
    wave->modulation =
        tpms_decoder_modulation(decoder) == ProtoTpmsModulationOOK ? OOK_PULSE_PCM :
                                                                    FSK_PULSE_PCM;
    wave->logical_bits = wave->pulse_count;

    raw_samples_free(samples);
    fieldset_free(fields);
    return wave->pulse_count > 0U && !wave->overflowed;
}

bool tpms_encoder_quantize(const TpmsEditValues* values, TpmsEncodedValues* encoded) {
    if(!values || !encoded || values->kind == TpmsEncoderNone) return false;
    encoded->pressure_kpa = values->pressure_kpa;
    if(encoded->pressure_kpa < 0.0f) encoded->pressure_kpa = 0.0f;
    if(encoded->pressure_kpa > 700.0f) encoded->pressure_kpa = 700.0f;
    encoded->temperature_c = values->temperature_c;
    if(encoded->temperature_c < -50.0f) encoded->temperature_c = -50.0f;
    if(encoded->temperature_c > 150.0f) encoded->temperature_c = 150.0f;
    encoded->flags = values->flags;
    return true;
}

size_t tpms_encoder_test_count(void) {
    return COUNT_OF(test_kinds);
}

TpmsEncoderKind tpms_encoder_test_kind(size_t index) {
    if(index >= COUNT_OF(test_kinds)) return TpmsEncoderNone;
    return test_kinds[index];
}

void tpms_encoder_default_values(TpmsEncoderKind kind, TpmsEditValues* values) {
    if(!values) return;
    memset(values, 0, sizeof(*values));
    values->kind = kind;
    values->id = 0x12345678U;
    if(kind == TpmsEncoderRenault || kind == TpmsEncoderSchraderEG53MA4) {
        values->id = 0x00123456U;
    }
    values->pressure_kpa = 220.0f;
    values->temperature_c = 20.0f;
    values->temperature_raw_f = 68U;
    values->temperature_raw_f_valid = false;
    values->radio_preset = TpmsRadioPresetAuto;
    values->wake_profile = TpmsWakeProfileNone;
    values->wake_profile_valid = false;
    values->repeat_count = 3U;
    values->frequency_hz =
        kind == TpmsEncoderPMV107J ? 315000000U : 433920000U;
    values->edit_mask = TPMS_EDIT_ALL;

    switch(kind) {
    case TpmsEncoderSchraderEG53MA4: values->flags = 0x4D030060U; break;
    case TpmsEncoderBMWGen23: values->flags = 0xF802U; break;
    case TpmsEncoderElantra2012: values->flags = 0x00C0U; break;
    case TpmsEncoderHyundaiVDO: values->flags = 0x0000U; break;
    case TpmsEncoderTruck: values->flags = 0x000BU; break;
    case TpmsEncoderRenault0435R: values->flags = 0x00C0U; break;
    case TpmsEncoderHondaTRW: values->flags = 0x00E1U; break;
    case TpmsEncoderPorsche: values->flags = 0xBB02U; break;
    case TpmsEncoderKia: values->flags = 0x000FU; break;
    case TpmsEncoderPMV107J: values->flags = 0x0000U; break;
    case TpmsEncoderTG1C: values->flags = 0x0000U; break;
    case TpmsEncoderQ85: values->flags = 0x0000U; break;
    case TpmsEncoderMercedesSprinter: values->flags = 0x836B00U; break;
    default: break;
    }
}

bool tpms_encoder_has_temperature(TpmsEncoderKind kind) {
    return kind > TpmsEncoderNone && kind < TpmsEncoderCount;
}

bool tpms_encoder_flags_editable(TpmsEncoderKind kind) {
    /* Ford now exposes a diagnostic 0xTTFF Status, but it is intentionally
     * read-only: the raw frame is preserved losslessly and the dedicated Ford
     * builder owns its temperature/moving bits. The four V2.6 TEST codecs below
     * have independently documented status/state bytes and rebuild integrity. */
    return kind == TpmsEncoderRenault || kind == TpmsEncoderSchrader ||
           kind == TpmsEncoderPMV107J || kind == TpmsEncoderTG1C ||
           kind == TpmsEncoderQ85 || kind == TpmsEncoderMercedesSprinter;
}

bool tpms_encoder_is_experimental(TpmsEncoderKind kind) {
    return kind >= TpmsEncoderBMWGen23 && kind < TpmsEncoderCount;
}

bool tpms_encoder_is_ook(TpmsEncoderKind kind) {
    ProtoViewDecoder* decoder = decoder_for_kind(kind);
    return decoder && tpms_decoder_modulation(decoder) == ProtoTpmsModulationOOK;
}

float tpms_encoder_pressure_step_kpa(TpmsEncoderKind kind) {
    const float psi_to_kpa = 6.89475729f;
    switch(kind) {
    case TpmsEncoderRenault: return 0.75f;
    case TpmsEncoderToyota: return 0.25f * psi_to_kpa;
    case TpmsEncoderSchrader:
    case TpmsEncoderSchraderEG53MA4:
    case TpmsEncoderBMWGen23:
    case TpmsEncoderPorsche: return 2.5f;
    case TpmsEncoderCitroen: return 1.364f;
    case TpmsEncoderFord: return 0.25f * psi_to_kpa;
    case TpmsEncoderElantra2012:
    case TpmsEncoderTruck: return 1.0f;
    case TpmsEncoderHyundaiVDO: return 1.375f;
    case TpmsEncoderRenault0435R: return 1.0f / 0.75f;
    case TpmsEncoderHondaTRW:
    case TpmsEncoderKia: return 0.2f * psi_to_kpa;
    case TpmsEncoderPMV107J: return 2.48f;
    case TpmsEncoderTG1C: return 1.38f;
    case TpmsEncoderQ85: return 3.0f;
    case TpmsEncoderMercedesSprinter: return psi_to_kpa / 2.75f;
    default: return 5.0f;
    }
}

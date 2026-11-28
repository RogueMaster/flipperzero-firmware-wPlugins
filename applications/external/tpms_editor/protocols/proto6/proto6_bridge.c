#include "proto6_bridge.h"
#include "proto6_core.h"

#include <lib/subghz/protocols/base.h>
#include <string.h>

#define BRIDGE_TAG "TPMSProto6Bridge"

RawSamplesBuffer* RawSamples = NULL;
RawSamplesBuffer* DetectedSamples = NULL;

typedef struct {
    SubGhzProtocolDecoderBase base;
    TPMSBlockGeneric generic;
} Proto6ResultDecoder;

struct Proto6Bridge {
    ProtoViewApp pv;
    Proto6ResultDecoder result;
    uint32_t last_observed_idx;
    uint32_t last_progress_tick;
    int16_t last_capture[TPMS_CAPTURE_MAX_SAMPLES];
    uint16_t last_capture_count;
    bool last_capture_truncated;
};

static SubGhzProtocolStatus proto6_result_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset);
static SubGhzProtocolStatus proto6_result_deserialize(void* context, FlipperFormat* flipper_format);
static uint8_t proto6_result_hash(void* context);
static void proto6_result_get_string(void* context, FuriString* output);
static void* proto6_result_alloc(SubGhzEnvironment* environment);
static void proto6_result_free(void* context);
static void proto6_result_reset(void* context);
static void proto6_result_feed(void* context, bool level, uint32_t duration);

static const SubGhzProtocolDecoder proto6_result_decoder = {
    .alloc = proto6_result_alloc,
    .free = proto6_result_free,
    .feed = proto6_result_feed,
    .reset = proto6_result_reset,
    .get_hash_data = proto6_result_hash,
    .serialize = proto6_result_serialize,
    .deserialize = proto6_result_deserialize,
    .get_string = proto6_result_get_string,
};

static const SubGhzProtocolEncoder proto6_result_encoder = {0};

static const SubGhzProtocol proto6_result_protocol = {
    .name = "ProtoView TPMS6",
    .type = SubGhzProtocolTypeStatic,
    .flag = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_AM | SubGhzProtocolFlag_FM |
            SubGhzProtocolFlag_Decodable,
    .decoder = &proto6_result_decoder,
    .encoder = &proto6_result_encoder,
};

static void* proto6_result_alloc(SubGhzEnvironment* environment) {
    UNUSED(environment);
    Proto6ResultDecoder* result = calloc(1, sizeof(Proto6ResultDecoder));
    if(result) {
        result->base.protocol = &proto6_result_protocol;
        result->generic.protocol_name = proto6_result_protocol.name;
        result->generic.battery_low = TPMS_NO_BATT;
    }
    return result;
}

static void proto6_result_free(void* context) {
    free(context);
}

static void proto6_result_reset(void* context) {
    UNUSED(context);
}

static void proto6_result_feed(void* context, bool level, uint32_t duration) {
    UNUSED(context);
    UNUSED(level);
    UNUSED(duration);
}

static uint8_t proto6_result_hash(void* context) {
    Proto6ResultDecoder* result = context;
    const uint32_t id = result->generic.id;
    return (uint8_t)(id ^ (id >> 8U) ^ (id >> 16U) ^ (id >> 24U));
}

static SubGhzProtocolStatus proto6_result_serialize(
    void* context,
    FlipperFormat* flipper_format,
    SubGhzRadioPreset* preset) {
    Proto6ResultDecoder* result = context;
    return tpms_block_generic_serialize(&result->generic, flipper_format, preset);
}

static SubGhzProtocolStatus proto6_result_deserialize(void* context, FlipperFormat* flipper_format) {
    Proto6ResultDecoder* result = context;
    return tpms_block_generic_deserialize(&result->generic, flipper_format);
}

static void proto6_result_get_string(void* context, FuriString* output) {
    Proto6ResultDecoder* result = context;
    furi_string_printf(
        output,
        "%s\r\nId:0x%08lX\r\nTemp:%2.0f C Bar:%2.2f",
        result->generic.protocol_name,
        (unsigned long)result->generic.id,
        (double)result->generic.temperature,
        (double)result->generic.pressure);
}

static uint32_t proto6_read_id(const ProtoViewField* field) {
    if(!field || field->type != FieldTypeBytes || !field->bytes) return 0U;
    uint32_t id = 0U;
    uint32_t count = (field->len + 1U) / 2U;
    if(count > 4U) count = 4U;
    for(uint32_t i = 0U; i < count; i++) id = (id << 8U) | field->bytes[i];
    return id;
}

static uint64_t proto6_copy_signal_bits(const ProtoViewMsgInfo* info) {
    if(!info || !info->bits || info->bits_bytes == 0U) return 0U;
    uint64_t data = 0U;
    const uint32_t count = info->bits_bytes > 6U ? 6U : info->bits_bytes;
    for(uint32_t i = 0U; i < count; i++) data = (data << 8U) | info->bits[i];
    return data;
}

static void proto6_copy_result(Proto6Bridge* bridge, const ProtoViewMsgInfo* info) {
    TPMSBlockGeneric* generic = &bridge->result.generic;
    memset(generic, 0, sizeof(*generic));
    generic->protocol_name = info->decoder ? info->decoder->name : "ProtoView TPMS6";
    generic->battery_low = TPMS_NO_BATT;
    generic->data_count_bit =
        (uint8_t)(info->pulses_count > 255U ? 255U : info->pulses_count);

    ProtoViewField* id = proto6_field_find(info->fieldset, "Tire ID");
    ProtoViewField* pressure_kpa = proto6_field_find(info->fieldset, "Pressure kpa");
    ProtoViewField* pressure_psi = proto6_field_find(info->fieldset, "Pressure psi");
    ProtoViewField* temperature = proto6_field_find(info->fieldset, "Temperature C");
    ProtoViewField* battery = proto6_field_find(info->fieldset, "Battery");
    ProtoViewField* flags = proto6_field_find(info->fieldset, "Flags");
    ProtoViewField* status = proto6_field_find(info->fieldset, "Status");
    ProtoViewField* moving = proto6_field_find(info->fieldset, "Moving");
    ProtoViewField* repeat = proto6_field_find(info->fieldset, "Repeat");
    ProtoViewField* temperature_f_raw =
        proto6_field_find(info->fieldset, "Temperature F Raw");
    ProtoViewField* raw_payload = proto6_field_find(info->fieldset, "Raw payload");
    ProtoViewField* raw_payload_bits = proto6_field_find(info->fieldset, "Raw payload bits");

    generic->id = proto6_read_id(id);
    if(pressure_kpa && pressure_kpa->type == FieldTypeFloat) {
        generic->pressure = pressure_kpa->fvalue / 100.0f;
    } else if(pressure_psi && pressure_psi->type == FieldTypeFloat) {
        generic->pressure = pressure_psi->fvalue * 0.0689475729f;
    }
    if(temperature && temperature->type == FieldTypeSignedInt) {
        generic->temperature = (float)temperature->value;
    }
    if(battery && battery->type == FieldTypeUnsignedInt) {
        generic->battery_low = battery->uvalue <= 20U ? 1U : 0U;
    }

    uint32_t edit_flags = 0U;
    if(flags) edit_flags = (uint32_t)flags->uvalue;
    else if(status) edit_flags = (uint32_t)status->uvalue;
    else if(moving) edit_flags = (uint32_t)moving->uvalue;
    else if(repeat) edit_flags = (uint32_t)repeat->uvalue;

    if(raw_payload && raw_payload->type == FieldTypeBytes && raw_payload->bytes) {
        uint32_t bytes = (raw_payload->len + 1U) / 2U;
        if(bytes > sizeof(generic->raw_payload)) bytes = sizeof(generic->raw_payload);
        memcpy(generic->raw_payload, raw_payload->bytes, bytes);
        generic->raw_payload_size = (uint8_t)bytes;
        uint32_t bits = raw_payload_bits ? (uint32_t)raw_payload_bits->uvalue : bytes * 8U;
        generic->raw_payload_bits = (uint8_t)(bits > TPMS_RAWBITS_BUF ? TPMS_RAWBITS_BUF : bits);
        generic->raw_payload_valid = bytes > 0U;
    }

    if(info->decoder && strcmp(info->decoder->name, "Schrader EG53MA4 TPMS") == 0) {
        const uint8_t temp_f_raw =
            temperature_f_raw ? (uint8_t)temperature_f_raw->uvalue : 0U;
        /* EG53 needs all 32 status bits and the exact Fahrenheit byte for
         * lossless RX -> edit -> TX. ID and pressure already have dedicated
         * fields in TPMSBlockGeneric. */
        generic->data = ((uint64_t)edit_flags << 32U) | ((uint64_t)temp_f_raw << 24U);
    } else {
        generic->data = ((uint64_t)(edit_flags & 0xFFFFU) << 48U) |
                        proto6_copy_signal_bits(info);
    }
}

static void proto6_store_last_capture(Proto6Bridge* bridge) {
    bridge->last_capture_count = 0U;
    bridge->last_capture_truncated = false;
    if(!DetectedSamples || bridge->pv.signal_bestlen == 0U) return;

    const uint32_t full_count = bridge->pv.signal_bestlen;
    const uint32_t copy_count =
        full_count > TPMS_CAPTURE_MAX_SAMPLES ? TPMS_CAPTURE_MAX_SAMPLES : full_count;

    furi_mutex_acquire(DetectedSamples->mutex, FuriWaitForever);
    for(uint32_t i = 0U; i < copy_count; i++) {
        bool level = false;
        uint32_t duration = 0U;
        raw_samples_get_unlocked(DetectedSamples, i, &level, &duration);
        if(duration > 32767U) duration = 32767U;
        bridge->last_capture[i] =
            level ? (int16_t)duration : (int16_t)(-(int32_t)duration);
    }
    furi_mutex_release(DetectedSamples->mutex);

    bridge->last_capture_count = (uint16_t)copy_count;
    bridge->last_capture_truncated = full_count > copy_count;
}

Proto6Bridge* proto6_bridge_alloc(NotificationApp* notifications) {
    Proto6Bridge* bridge = calloc(1, sizeof(Proto6Bridge));
    if(!bridge) return NULL;

    RawSamples = raw_samples_alloc();
    DetectedSamples = raw_samples_alloc();
    if(!RawSamples || !DetectedSamples) {
        if(RawSamples) raw_samples_free(RawSamples);
        if(DetectedSamples) raw_samples_free(DetectedSamples);
        RawSamples = NULL;
        DetectedSamples = NULL;
        free(bridge);
        return NULL;
    }

    bridge->pv.notification = notifications;
    bridge->pv.modulation = ProtoTpmsModulationOOK;
    bridge->result.base.protocol = &proto6_result_protocol;
    bridge->result.generic.protocol_name = proto6_result_protocol.name;
    bridge->result.generic.battery_low = TPMS_NO_BATT;
    reset_current_signal(&bridge->pv);
    bridge->last_observed_idx = 0U;
    bridge->last_progress_tick = furi_get_tick();
    bridge->last_capture_count = 0U;
    bridge->last_capture_truncated = false;
    return bridge;
}

void proto6_bridge_free(Proto6Bridge* bridge) {
    if(!bridge) return;
    free_msg_info(bridge->pv.msg_info);
    bridge->pv.msg_info = NULL;
    if(RawSamples) raw_samples_free(RawSamples);
    if(DetectedSamples) raw_samples_free(DetectedSamples);
    RawSamples = NULL;
    DetectedSamples = NULL;
    free(bridge);
}

void proto6_bridge_reset(Proto6Bridge* bridge) {
    if(!bridge) return;
    reset_current_signal(&bridge->pv);
    bridge->pv.signal_last_scan_idx = 0U;
    bridge->last_observed_idx = 0U;
    bridge->last_progress_tick = furi_get_tick();
    bridge->last_capture_count = 0U;
    bridge->last_capture_truncated = false;
}

void proto6_bridge_feed(Proto6Bridge* bridge, bool level, uint32_t duration) {
    UNUSED(bridge);
    if(!RawSamples) return;
    raw_samples_add(RawSamples, level, duration);
}

void proto6_bridge_rx_callback(bool level, uint32_t duration, void* context) {
    Proto6Bridge* bridge = context;
    if(!bridge) return;
    proto6_bridge_feed(bridge, level, duration);
}

void proto6_bridge_set_mode(Proto6Bridge* bridge, Proto6Mode mode) {
    if(!bridge) return;
    bridge->pv.modulation =
        mode == Proto6ModeOOK ? ProtoTpmsModulationOOK : ProtoTpmsModulationFSK;
}

SubGhzProtocolDecoderBase* proto6_bridge_scan(Proto6Bridge* bridge) {
    if(!bridge || !RawSamples) return NULL;

    /* ProtoView normally scans after half of its 2048-edge ring has changed.
     * Keep that exact path for continuous traffic. The classic UI also needs
     * to decode a single short TPMS burst, so after the input has been quiet
     * for one GUI tick we run the same original scanner on the accumulated
     * RAW data. Only the trigger is extended; scan_for_signal(), bitmap
     * conversion and all six protocol decoders remain ProtoView code. */
    uint32_t current_idx;
    uint32_t total;
    furi_mutex_acquire(RawSamples->mutex, FuriWaitForever);
    current_idx = RawSamples->idx;
    total = RawSamples->total;
    furi_mutex_release(RawSamples->mutex);

    const uint32_t now = furi_get_tick();
    if(current_idx != bridge->last_observed_idx) {
        bridge->last_observed_idx = current_idx;
        bridge->last_progress_tick = now;
    }

    const uint32_t last_idx = bridge->pv.signal_last_scan_idx;
    uint32_t delta;
    if(last_idx <= current_idx) {
        delta = current_idx - last_idx;
    } else {
        delta = total - last_idx + current_idx;
    }

    const bool half_buffer_ready = delta >= total / 2U;
    const bool quiet_burst_ready =
        delta >= 18U && (now - bridge->last_progress_tick) >= furi_ms_to_ticks(50U);
    if(!half_buffer_ready && !quiet_burst_ready) return NULL;

    bridge->pv.signal_last_scan_idx = current_idx;
    scan_for_signal(&bridge->pv, RawSamples, 30U);

    if(!bridge->pv.signal_decoded || !bridge->pv.msg_info ||
       !bridge->pv.msg_info->decoder) {
        return NULL;
    }

    proto6_copy_result(bridge, bridge->pv.msg_info);
    /* DetectedSamples is cleared by reset_current_signal(), so retain the
     * exact signed timings before resetting the ProtoView scanner. */
    proto6_store_last_capture(bridge);
    reset_current_signal(&bridge->pv);
    bridge->pv.signal_last_scan_idx = 0U;
    bridge->last_observed_idx = 0U;
    bridge->last_progress_tick = now;
    return &bridge->result.base;
}

size_t proto6_bridge_copy_last_capture(
    const Proto6Bridge* bridge,
    int16_t* output,
    size_t capacity,
    bool* truncated) {
    if(truncated) *truncated = false;
    if(!bridge || !output || capacity == 0U || bridge->last_capture_count == 0U) return 0U;

    size_t count = bridge->last_capture_count;
    if(count > capacity) count = capacity;
    memcpy(output, bridge->last_capture, count * sizeof(output[0]));
    if(truncated) {
        *truncated = bridge->last_capture_truncated ||
                     bridge->last_capture_count > capacity;
    }
    return count;
}

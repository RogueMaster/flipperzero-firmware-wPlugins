#include "ac_protocols.h"
#include "ac_mitsubishi.h"
#include "ac_mhi.h"
#include "ac_gree.h"
#include "ac_fujitsu.h"
#include "ac_mitsubishi_legacy.h"
#include "ac_lg.h"

#define AC_PROBE_TEMP_C     24U
#define AC_PROBE_DUTY_CYCLE INFRARED_COMMON_DUTY_CYCLE

/* ---- Matchers (wrap the drivers' own decoders; no extra signatures) ---- */

/* Fixed scratch space for matchers, so identification needs no heap and adds
 * little to the app's 4 KB stack. Matchers only run on the app thread, one at
 * a time, so a single shared buffer is safe. Not reentrant. */
static uint32_t matcher_prefix_scratch[AC_MITSUBISHI_PREFIX_MAX];

static bool
    match_fujitsu_variant(const InfraredSignal* signal, AcFujitsuVariant expected_variant) {
    AcFujitsuVariant detected_variant = AcFujitsuVariantUnknown;
    uint8_t frame[AC_FUJITSU_MAX_STATE_BYTES];
    return ac_fujitsu_decode_signal(signal, &detected_variant, frame) &&
           detected_variant == expected_variant;
}

static bool match_fujitsu_long30(const InfraredSignal* signal) {
    return match_fujitsu_variant(signal, AcFujitsuVariantLong30);
}

static bool match_fujitsu_compact(const InfraredSignal* signal) {
    return match_fujitsu_variant(signal, AcFujitsuVariantCompact);
}

static bool match_fujitsu_long31(const InfraredSignal* signal) {
    return match_fujitsu_variant(signal, AcFujitsuVariantLong31);
}

static bool build_fujitsu_probe(AcFujitsuVariant variant, bool power_on, InfraredSignal* signal) {
    return ac_fujitsu_build_probe(variant, power_on, signal);
}

static bool build_fujitsu_long30_probe(bool power_on, InfraredSignal* signal) {
    return build_fujitsu_probe(AcFujitsuVariantLong30, power_on, signal);
}

static bool build_fujitsu_compact_probe(bool power_on, InfraredSignal* signal) {
    return build_fujitsu_probe(AcFujitsuVariantCompact, power_on, signal);
}

static bool build_fujitsu_long31_probe(bool power_on, InfraredSignal* signal) {
    return build_fujitsu_probe(AcFujitsuVariantLong31, power_on, signal);
}

static bool match_mitsubishi_ac_144(const InfraredSignal* signal) {
    uint8_t frame[AC_MITSUBISHI_FRAME_BYTES];
    size_t prefix_count = 0;
    return ac_mitsubishi_decode_profile(signal, frame, matcher_prefix_scratch, &prefix_count);
}

static bool match_mitsubishi_legacy_variant(
    const InfraredSignal* signal,
    AcMitsubishiLegacyVariant expected_variant) {
    AcMitsubishiLegacyVariant detected_variant = AcMitsubishiLegacyNone;
    uint8_t frame[AC_MITSUBISHI_LEGACY_MAX_FRAME_BYTES];
    return ac_mitsubishi_legacy_decode(signal, &detected_variant, frame) &&
           detected_variant == expected_variant;
}

static bool match_mitsubishi_112(const InfraredSignal* signal) {
    return match_mitsubishi_legacy_variant(signal, AcMitsubishiLegacy112);
}

static bool match_mitsubishi_136(const InfraredSignal* signal) {
    return match_mitsubishi_legacy_variant(signal, AcMitsubishiLegacy136);
}

static bool build_mitsubishi_legacy_probe(
    AcMitsubishiLegacyVariant variant,
    bool power_on,
    InfraredSignal* signal) {
    return ac_mitsubishi_legacy_build_probe(variant, power_on, signal);
}

static bool build_mitsubishi_112_probe(bool power_on, InfraredSignal* signal) {
    return build_mitsubishi_legacy_probe(AcMitsubishiLegacy112, power_on, signal);
}

static bool build_mitsubishi_136_probe(bool power_on, InfraredSignal* signal) {
    return build_mitsubishi_legacy_probe(AcMitsubishiLegacy136, power_on, signal);
}

static bool match_gree_variant(const InfraredSignal* signal, AcGreeModel expected_model) {
    AcGreeModel detected_model = AcGreeModelUnknown;
    uint8_t frame[AC_GREE_STATE_BYTES];
    return ac_gree_decode_signal(signal, &detected_model, frame) &&
           detected_model == expected_model;
}

static bool match_gree_yaw1f(const InfraredSignal* signal) {
    return match_gree_variant(signal, AcGreeModelYaw1f);
}

static bool match_gree_ybofb(const InfraredSignal* signal) {
    return match_gree_variant(signal, AcGreeModelYbofb);
}

static bool match_gree_yx1fsf(const InfraredSignal* signal) {
    return match_gree_variant(signal, AcGreeModelYx1fsf);
}

static bool build_gree_probe_model(AcGreeModel model, bool power_on, InfraredSignal* signal) {
    return ac_gree_build_probe(model, power_on, signal);
}

static bool build_gree_yaw1f_probe(bool power_on, InfraredSignal* signal) {
    return build_gree_probe_model(AcGreeModelYaw1f, power_on, signal);
}

static bool build_gree_ybofb_probe(bool power_on, InfraredSignal* signal) {
    return build_gree_probe_model(AcGreeModelYbofb, power_on, signal);
}

static bool build_gree_yx1fsf_probe(bool power_on, InfraredSignal* signal) {
    return build_gree_probe_model(AcGreeModelYx1fsf, power_on, signal);
}

static bool
    match_mitsubishi_heavy_variant(const InfraredSignal* signal, AcMhiVariant expected_variant) {
    AcMhiVariant detected_variant = AcMhiVariantUnknown;
    uint8_t state[AC_MHI_MAX_STATE_BYTES];
    return ac_mhi_decode_signal(signal, &detected_variant, state) &&
           detected_variant == expected_variant;
}

static bool match_mitsubishi_heavy_88(const InfraredSignal* signal) {
    return match_mitsubishi_heavy_variant(signal, AcMhiVariant88);
}

static bool match_mitsubishi_heavy_152(const InfraredSignal* signal) {
    return match_mitsubishi_heavy_variant(signal, AcMhiVariant152);
}

static bool build_mitsubishi_ac_probe(bool power_on, InfraredSignal* signal) {
    uint8_t frame[AC_MITSUBISHI_FRAME_BYTES] = {
        0x23U,
        0xCBU,
        0x26U,
        0x01U,
        0x00U,
    };
    uint32_t timings[AC_MITSUBISHI_RAW_MAX];
    size_t timings_count = 0U;

    frame[5] = power_on ? 0x20U : 0x00U;
    frame[6] = (uint8_t)(3U << 3); /* Cool */
    frame[7] = (uint8_t)(AC_PROBE_TEMP_C - 16U);
    frame[8] = 6U; /* Cool setpoint encoding */
    frame[9] = 0x80U; /* Fan auto */
    ac_mitsubishi_update_checksum(frame);

    if(!ac_mitsubishi_build_signal(
           frame, NULL, 0U, timings, AC_MITSUBISHI_RAW_MAX, &timings_count)) {
        return false;
    }
    infrared_signal_set_raw_signal(signal, timings, timings_count, 38000U, AC_PROBE_DUTY_CYCLE);
    return true;
}

static bool build_mhi_probe(AcMhiVariant variant, bool power_on, InfraredSignal* signal) {
    uint8_t state[AC_MHI_MAX_STATE_BYTES];
    uint32_t timings[AC_MHI_MAX_RAW_TIMINGS];
    size_t timings_count = 0U;

    if(!ac_mhi_build_state(variant, power_on, AC_PROBE_TEMP_C, AcMhiModeCool, 0U, state) ||
       !ac_mhi_build_raw_timings(variant, state, timings, AC_MHI_MAX_RAW_TIMINGS, &timings_count)) {
        return false;
    }
    infrared_signal_set_raw_signal(signal, timings, timings_count, 38000U, AC_PROBE_DUTY_CYCLE);
    return true;
}

static bool build_mhi_88_probe(bool power_on, InfraredSignal* signal) {
    return build_mhi_probe(AcMhiVariant88, power_on, signal);
}

static bool build_mhi_152_probe(bool power_on, InfraredSignal* signal) {
    return build_mhi_probe(AcMhiVariant152, power_on, signal);
}

static bool match_lg_variant(const InfraredSignal* signal, AcLgVariant expected) {
    return ac_lg_matches_variant(signal, expected);
}

static bool match_lg(const InfraredSignal* signal) {
    return match_lg_variant(signal, AcLgVariantLg);
}

static bool match_lg2(const InfraredSignal* signal) {
    return match_lg_variant(signal, AcLgVariantLg2);
}

static bool build_lg_probe(bool power_on, InfraredSignal* signal) {
    return ac_lg_build_probe(AcLgVariantLg, power_on, signal);
}

static bool build_lg2_probe(bool power_on, InfraredSignal* signal) {
    return ac_lg_build_probe(AcLgVariantLg2, power_on, signal);
}

/* ---- Catalogue ---- */

enum {
    ManufacturerMitsubishiElectric,
    ManufacturerMitsubishiHeavy,
    ManufacturerFujitsu,
    ManufacturerGree,
    ManufacturerLg,
};

static const AcManufacturer manufacturers[] = {
    [ManufacturerMitsubishiElectric] = {.name = "MITSUBISHI ELECTRIC"},
    [ManufacturerMitsubishiHeavy] = {.name = "MITSUBISHI HEAVY IND."},
    [ManufacturerFujitsu] = {.name = "FUJITSU"},
    [ManufacturerGree] = {.name = "GREE"},
    [ManufacturerLg] = {.name = "LG"},
};

enum {
    FamilyMitsubishiAc,
    FamilyMitsubishiHeavy,
    FamilyGree,
    FamilyFujitsu,
    FamilyLg,
};

static const AcProtocolFamily families[] = {
    [FamilyMitsubishiAc] =
        {
            .manufacturer = ManufacturerMitsubishiElectric,
            .name = "MITSUBISHI_AC",
        },
    [FamilyMitsubishiHeavy] =
        {
            .manufacturer = ManufacturerMitsubishiHeavy,
            .name = "MHI",
        },
    [FamilyGree] =
        {
            .manufacturer = ManufacturerGree,
            .name = "GREE",
        },
    [FamilyFujitsu] =
        {
            .manufacturer = ManufacturerFujitsu,
            .name = "FUJITSU_AC",
        },
    [FamilyLg] =
        {
            .manufacturer = ManufacturerLg,
            .name = "LG_AC",
        },
};

enum {
    /* Keep the known working baseline first in the Mitsubishi Electric list. */
    VariantMitsubishiAc144,
    VariantMitsubishiAc112,
    VariantMitsubishiAc136,
    VariantMitsubishiHeavy88,
    VariantMitsubishiHeavy152,
    VariantGreeYaw1f,
    VariantGreeYbofb,
    VariantGreeYx1fsf,
    VariantFujitsuLong30,
    VariantFujitsuCompact,
    VariantFujitsuLong31,
    VariantLg,
    VariantLg2,
};

static const AcProtocolVariant variants[] = {
    [VariantMitsubishiAc144] =
        {
            .family = FamilyMitsubishiAc,
            .name = "144-BIT",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_mitsubishi_ac_144,
            .build_probe_signal = build_mitsubishi_ac_probe,
        },
    [VariantMitsubishiAc112] =
        {
            .family = FamilyMitsubishiAc,
            .name = "112-BIT",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_mitsubishi_112,
            .build_probe_signal = build_mitsubishi_112_probe,
        },
    [VariantMitsubishiAc136] =
        {
            .family = FamilyMitsubishiAc,
            .name = "136-BIT",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_mitsubishi_136,
            .build_probe_signal = build_mitsubishi_136_probe,
        },
    [VariantMitsubishiHeavy88] =
        {
            .family = FamilyMitsubishiHeavy,
            .name = "88-BIT",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_mitsubishi_heavy_88,
            .build_probe_signal = build_mhi_88_probe,
        },
    [VariantMitsubishiHeavy152] =
        {
            .family = FamilyMitsubishiHeavy,
            .name = "152-BIT",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_mitsubishi_heavy_152,
            .build_probe_signal = build_mhi_152_probe,
        },
    /* Gree uses one 64-bit frame with model-specific state bits/modes. */
    [VariantGreeYaw1f] =
        {
            .family = FamilyGree,
            .name = "YAW1F",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_gree_yaw1f,
            .build_probe_signal = build_gree_yaw1f_probe,
        },
    [VariantGreeYbofb] =
        {
            .family = FamilyGree,
            .name = "YBOFB",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_gree_ybofb,
            .build_probe_signal = build_gree_ybofb_probe,
        },
    [VariantGreeYx1fsf] =
        {
            .family = FamilyGree,
            .name = "YX1FSF",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_gree_yx1fsf,
            .build_probe_signal = build_gree_yx1fsf_probe,
        },
    /* Unique Fujitsu wire families with all swing bits kept at zero. */
    [VariantFujitsuLong30] =
        {
            .family = FamilyFujitsu,
            .name = "16B V30",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_fujitsu_long30,
            .build_probe_signal = build_fujitsu_long30_probe,
        },
    [VariantFujitsuCompact] =
        {
            .family = FamilyFujitsu,
            .name = "15B FC",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_fujitsu_compact,
            .build_probe_signal = build_fujitsu_compact_probe,
        },
    [VariantFujitsuLong31] =
        {
            .family = FamilyFujitsu,
            .name = "16B V31",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_fujitsu_long31,
            .build_probe_signal = build_fujitsu_long31_probe,
        },
    /* Two documented LG A/C wire formats. These probes are experimental and
     * profiles remain power-replay-only until controls are validated. */
    [VariantLg] =
        {
            .family = FamilyLg,
            .name = "LG 28-BIT",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_lg,
            .build_probe_signal = build_lg_probe,
        },
    [VariantLg2] =
        {
            .family = FamilyLg,
            .name = "LG2 28-BIT",
            .supports_state_control = true,
            .supports_room_probe = true,
            .matches = match_lg2,
            .build_probe_signal = build_lg2_probe,
        },
};

/* MHI remote mappings are reference leads; capture validation is required. */
static const AcRemoteEntry remotes[] = {
    {
        .manufacturer = ManufacturerMitsubishiElectric,
        .remote_code = "SG17B",
        .variant = VariantMitsubishiAc144,
        .tested_unit = "MSZ-AG35VGD",
    },
    {
        .manufacturer = ManufacturerMitsubishiHeavy,
        .remote_code = "RKX502A001C",
        .variant = VariantMitsubishiHeavy88,
        .tested_unit = NULL,
    },
    {
        .manufacturer = ManufacturerMitsubishiHeavy,
        .remote_code = "RLA502A700B",
        .variant = VariantMitsubishiHeavy152,
        .tested_unit = NULL,
    },
};

#define COUNT_OF_TABLE(table) (sizeof(table) / sizeof((table)[0]))

size_t ac_manufacturer_count(void) {
    return COUNT_OF_TABLE(manufacturers);
}

const AcManufacturer* ac_manufacturer_get(size_t index) {
    return index < COUNT_OF_TABLE(manufacturers) ? &manufacturers[index] : NULL;
}

size_t ac_variant_count(void) {
    return COUNT_OF_TABLE(variants);
}

const AcProtocolVariant* ac_variant_get(size_t index) {
    return index < COUNT_OF_TABLE(variants) ? &variants[index] : NULL;
}

const AcProtocolFamily* ac_variant_family(size_t variant_index) {
    const AcProtocolVariant* variant = ac_variant_get(variant_index);
    if(!variant || variant->family >= COUNT_OF_TABLE(families)) return NULL;
    return &families[variant->family];
}

size_t ac_remote_count_for_manufacturer(size_t manufacturer) {
    size_t count = 0;
    for(size_t i = 0; i < COUNT_OF_TABLE(remotes); i++) {
        if(remotes[i].manufacturer == manufacturer) count++;
    }
    return count;
}

const AcRemoteEntry* ac_remote_get_for_manufacturer(size_t manufacturer, size_t position) {
    for(size_t i = 0; i < COUNT_OF_TABLE(remotes); i++) {
        if(remotes[i].manufacturer != manufacturer) continue;
        if(position == 0U) return &remotes[i];
        position--;
    }
    return NULL;
}

AcIdentifyResult ac_identify_signal(
    const InfraredSignal* signal,
    size_t matches[AC_IDENTIFY_MAX_MATCHES],
    size_t* match_count) {
    size_t count = 0;
    if(signal) {
        for(size_t i = 0; i < COUNT_OF_TABLE(variants); i++) {
            if(variants[i].matches && variants[i].matches(signal)) {
                if(count < AC_IDENTIFY_MAX_MATCHES) matches[count] = i;
                count++;
            }
        }
    }

    if(match_count)
        *match_count = count < AC_IDENTIFY_MAX_MATCHES ? count : AC_IDENTIFY_MAX_MATCHES;
    if(count == 0U) return AcIdentifyNoMatch;
    return count == 1U ? AcIdentifyMatch : AcIdentifyAmbiguous;
}

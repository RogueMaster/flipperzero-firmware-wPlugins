/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * SecurityInfos, ICAO Doc 9303 part 11 section 9.2 and appendix A, with the
 * protocol identifiers of BSI TR-03110 part 3 appendix A.1.1.1.
 *
 * EF.CardAccess is the first thing this reader gets from a chip and the last
 * word on how the chip wants to be opened. Every entry is classified, and an
 * entry this build cannot run is kept with the reason why rather than
 * discarded: a document that fails should be able to say "this chip wants the
 * integrated mapping" instead of "authentication failed".
 */

#include "emrtd_security_info.h"

#include <string.h>

#include "emrtd_tlv.h"

#define EMRTD_ASN1_INTEGER  0x02u
#define EMRTD_ASN1_OID      0x06u
#define EMRTD_ASN1_SEQUENCE 0x30u
#define EMRTD_ASN1_SET      0x31u
#define EMRTD_TAG_DG14      0x6Eu

/** Object identifier prefix bsi-de protocols, 0.4.0.127.0.7.2.2. */
static const uint8_t emrtd_bsi_prefix[] = {0x04, 0x00, 0x7F, 0x00, 0x07, 0x02, 0x02};

/** 2.23.136.1.1.5, the Active Authentication protocol object. */
static const uint8_t emrtd_oid_active_auth[] = {0x67, 0x81, 0x08, 0x01, 0x01, 0x05};

/* The protocol component that follows the prefix (TR-03110-3, A.1.1.1). */
#define EMRTD_PROTOCOL_PK   1u
#define EMRTD_PROTOCOL_TA   2u
#define EMRTD_PROTOCOL_CA   3u
#define EMRTD_PROTOCOL_PACE 4u

/* The mapping component of a PACE identifier. */
#define EMRTD_PACE_DH_GM    1u
#define EMRTD_PACE_ECDH_GM  2u
#define EMRTD_PACE_DH_IM    3u
#define EMRTD_PACE_ECDH_IM  4u
#define EMRTD_PACE_ECDH_CAM 6u

/** A full PACEInfo identifier is the prefix, id-PACE, a mapping and a cipher. */
#define EMRTD_PACE_OID_LEN 10u

/** Append a string, never writing past @p dst_size. Returns the new position. */
static size_t str_append(char* dst, size_t dst_size, size_t pos, const char* text) {
    while(*text != '\0' && pos + 1 < dst_size) {
        dst[pos++] = *text++;
    }
    return pos;
}

static bool oid_has_prefix(const EmrtdTlv* oid, const uint8_t* prefix, size_t prefix_len) {
    return oid->value_len >= prefix_len && memcmp(oid->value, prefix, prefix_len) == 0;
}

static bool oid_equals(const EmrtdTlv* oid, const uint8_t* value, size_t value_len) {
    return oid->value_len == value_len && memcmp(oid->value, value, value_len) == 0;
}

/** The published name of a PACE identifier, which is what the report prints. */
static const char* pace_oid_name(uint8_t mapping, uint8_t cipher) {
    switch(mapping) {
    case EMRTD_PACE_DH_GM:
        switch(cipher) {
        case 1:
            return "id-PACE-DH-GM-3DES-CBC-CBC";
        case 2:
            return "id-PACE-DH-GM-AES-CBC-CMAC-128";
        case 3:
            return "id-PACE-DH-GM-AES-CBC-CMAC-192";
        case 4:
            return "id-PACE-DH-GM-AES-CBC-CMAC-256";
        default:
            return "id-PACE-DH-GM";
        }
    case EMRTD_PACE_ECDH_GM:
        switch(cipher) {
        case 1:
            return "id-PACE-ECDH-GM-3DES-CBC-CBC";
        case 2:
            return "id-PACE-ECDH-GM-AES-CBC-CMAC-128";
        case 3:
            return "id-PACE-ECDH-GM-AES-CBC-CMAC-192";
        case 4:
            return "id-PACE-ECDH-GM-AES-CBC-CMAC-256";
        default:
            return "id-PACE-ECDH-GM";
        }
    case EMRTD_PACE_DH_IM:
        return "id-PACE-DH-IM";
    case EMRTD_PACE_ECDH_IM:
        switch(cipher) {
        case 2:
            return "id-PACE-ECDH-IM-AES-CBC-CMAC-128";
        case 3:
            return "id-PACE-ECDH-IM-AES-CBC-CMAC-192";
        case 4:
            return "id-PACE-ECDH-IM-AES-CBC-CMAC-256";
        default:
            return "id-PACE-ECDH-IM";
        }
    case EMRTD_PACE_ECDH_CAM:
        switch(cipher) {
        case 2:
            return "id-PACE-ECDH-CAM-AES-CBC-CMAC-128";
        case 3:
            return "id-PACE-ECDH-CAM-AES-CBC-CMAC-192";
        case 4:
            return "id-PACE-ECDH-CAM-AES-CBC-CMAC-256";
        default:
            return "id-PACE-ECDH-CAM";
        }
    default:
        return "id-PACE";
    }
}

/** Build one PACEInfo and decide whether this build could run it. */
static void pace_info_build(
    const EmrtdTlv* oid,
    const uint32_t* integers,
    size_t integer_count,
    EmrtdPaceInfo* info) {
    memset(info, 0, sizeof(*info));
    info->parameter_id = 0xFF;
    info->mapping = EmrtdPaceMappingUnknown;
    info->agreement = EmrtdPaceAgreementUnknown;
    /* No enumerator means "not one of ours", so the count stands in for it. */
    info->cipher = EmrtdCipherCount;
    info->reason = EmrtdErrorUnsupported;
    info->oid_name = "id-PACE";

    /* The caller matches the length first; this keeps the reads below honest. */
    if(oid->value_len != EMRTD_PACE_OID_LEN) {
        return;
    }

    if(oid->value_len <= EMRTD_OID_MAX) {
        memcpy(info->oid, oid->value, oid->value_len);
        info->oid_len = oid->value_len;
    }

    const uint8_t mapping = oid->value[8];
    const uint8_t cipher = oid->value[9];
    info->oid_name = pace_oid_name(mapping, cipher);

    switch(mapping) {
    case EMRTD_PACE_DH_GM:
        info->agreement = EmrtdPaceAgreementDh;
        info->mapping = EmrtdPaceMappingGeneric;
        break;
    case EMRTD_PACE_ECDH_GM:
        info->agreement = EmrtdPaceAgreementEcdh;
        info->mapping = EmrtdPaceMappingGeneric;
        break;
    case EMRTD_PACE_DH_IM:
        info->agreement = EmrtdPaceAgreementDh;
        info->mapping = EmrtdPaceMappingIntegrated;
        break;
    case EMRTD_PACE_ECDH_IM:
        info->agreement = EmrtdPaceAgreementEcdh;
        info->mapping = EmrtdPaceMappingIntegrated;
        break;
    case EMRTD_PACE_ECDH_CAM:
        info->agreement = EmrtdPaceAgreementEcdh;
        info->mapping = EmrtdPaceMappingChipAuth;
        break;
    default:
        break;
    }

    switch(cipher) {
    case 1:
        info->cipher = EmrtdCipherTdes;
        break;
    case 2:
        info->cipher = EmrtdCipherAes128;
        break;
    case 3:
        info->cipher = EmrtdCipherAes192;
        break;
    case 4:
        info->cipher = EmrtdCipherAes256;
        break;
    default:
        break;
    }

    /* PACEInfo ::= SEQUENCE { protocol OID, version INTEGER, parameterId OPTIONAL }. */
    info->version = integer_count > 0 ? integers[0] : 2;
    if(integer_count > 1 && integers[1] <= 0xFF) {
        info->parameter_id = (uint8_t)integers[1];
    }

    if(info->agreement == EmrtdPaceAgreementEcdh && info->parameter_id != 0xFF) {
        info->curve = emrtd_ec_curve_by_param_id(info->parameter_id);
    }

    /*
     * The order of these tests is the order in which the obstacles are
     * insurmountable. A MODP group needs modular exponentiation, which this
     * firmware's mbed TLS cannot link at all, so it outranks the mapping;
     * only then does the curve, which merely has to be small enough, matter.
     * See docs/platform.md.
     */
    if(info->agreement == EmrtdPaceAgreementDh) {
        info->reason = EmrtdErrorPaceUnsupportedDh;
    } else if(info->agreement != EmrtdPaceAgreementEcdh) {
        info->reason = EmrtdErrorUnsupported;
    } else if(info->mapping != EmrtdPaceMappingGeneric) {
        info->reason = EmrtdErrorPaceUnsupportedMapping;
    } else if(info->curve == NULL || !emrtd_ec_curve_supported(info->curve)) {
        info->reason = EmrtdErrorPaceUnsupportedCurve;
    } else if(info->cipher == EmrtdCipherCount) {
        info->reason = EmrtdErrorUnsupported;
    } else {
        info->usable = true;
        info->reason = EmrtdErrorNone;
    }
}

/**
 * How much this reader would like to run a given PACEInfo.
 *
 * Whether it can run at all dominates; among equals the stronger cipher wins,
 * which is what ICAO 9303-11 section 9.2 expects of a terminal offered a
 * choice.
 */
static unsigned pace_rank(const EmrtdPaceInfo* info) {
    unsigned cipher_rank;
    switch(info->cipher) {
    case EmrtdCipherAes256:
        cipher_rank = 4;
        break;
    case EmrtdCipherAes192:
        cipher_rank = 3;
        break;
    case EmrtdCipherAes128:
        cipher_rank = 2;
        break;
    case EmrtdCipherTdes:
        cipher_rank = 1;
        break;
    default:
        cipher_rank = 0;
        break;
    }
    return (info->usable ? 0x10u : 0u) | cipher_rank;
}

/** Fold one SecurityInfo into the summary. */
static void security_info_handle(const EmrtdTlv* sequence, EmrtdSecurityInfos* out) {
    EmrtdTlvIter iter;
    EmrtdTlv node;
    emrtd_tlv_iter_children(&iter, sequence);

    /* Every SecurityInfo opens with the object identifier of its protocol. */
    if(!emrtd_tlv_iter_next(&iter, &node) || node.tag != EMRTD_ASN1_OID) {
        return;
    }
    const EmrtdTlv oid = node;

    if(out->entry_count < 0xFF) {
        out->entry_count++;
    }

    uint32_t integers[2] = {0, 0};
    size_t integer_count = 0;
    while(emrtd_tlv_iter_next(&iter, &node)) {
        uint32_t value = 0;
        if(node.tag == EMRTD_ASN1_INTEGER && integer_count < 2 &&
           emrtd_tlv_read_uint(&node, &value)) {
            integers[integer_count++] = value;
        }
    }

    if(oid_equals(&oid, emrtd_oid_active_auth, sizeof(emrtd_oid_active_auth))) {
        out->has_active_auth = true;
        return;
    }
    if(!oid_has_prefix(&oid, emrtd_bsi_prefix, sizeof(emrtd_bsi_prefix)) ||
       oid.value_len <= sizeof(emrtd_bsi_prefix)) {
        return;
    }

    switch(oid.value[sizeof(emrtd_bsi_prefix)]) {
    case EMRTD_PROTOCOL_PK:
        /* ChipAuthenticationPublicKeyInfo: the key Chip Authentication uses. */
        out->has_chip_auth = true;
        break;
    case EMRTD_PROTOCOL_TA:
        out->has_terminal_auth = true;
        break;
    case EMRTD_PROTOCOL_CA:
        out->has_chip_auth = true;
        break;
    case EMRTD_PROTOCOL_PACE: {
        /*
         * A shorter identifier under id-PACE is a PACEDomainParameterInfo,
         * which names a mapping without a cipher and carries the parameters
         * of a curve this reader does not support anyway.
         */
        if(oid.value_len != EMRTD_PACE_OID_LEN) {
            break;
        }
        EmrtdPaceInfo info;
        pace_info_build(&oid, integers, integer_count, &info);
        if(!out->has_pace || pace_rank(&info) > pace_rank(&out->pace)) {
            out->pace = info;
        }
        out->has_pace = true;
        break;
    }
    default:
        break;
    }
}

EmrtdError emrtd_security_infos_parse(const uint8_t* data, size_t len, EmrtdSecurityInfos* out) {
    if(data == NULL || out == NULL) {
        return EmrtdErrorInvalidInput;
    }

    memset(out, 0, sizeof(*out));
    out->pace.parameter_id = 0xFF;
    out->pace.cipher = EmrtdCipherCount;
    out->pace.oid_name = "";

    EmrtdTlv root;
    if(!emrtd_tlv_parse_first(data, len, &root)) {
        return EmrtdErrorParse;
    }

    /*
     * EF.CardAccess is a bare SET OF SecurityInfo; DG14 wraps the same set in
     * its template. Both end up as a buffer of sibling SEQUENCEs.
     */
    const uint8_t* body = data;
    size_t body_len = len;

    if(root.tag == EMRTD_TAG_DG14) {
        body = root.value;
        body_len = root.value_len;
        if(!emrtd_tlv_parse_first(body, body_len, &root)) {
            return EmrtdErrorParse;
        }
    }

    if(root.tag == EMRTD_ASN1_SET) {
        body = root.value;
        body_len = root.value_len;
    } else if(root.tag == EMRTD_ASN1_SEQUENCE) {
        /*
         * A SEQUENCE here is either one SecurityInfo, which begins with an
         * object identifier, or a container of them. The first child settles
         * which, and getting it wrong would silently drop every entry.
         */
        EmrtdTlv first;
        if(emrtd_tlv_parse_first(root.value, root.value_len, &first) &&
           first.tag != EMRTD_ASN1_OID) {
            body = root.value;
            body_len = root.value_len;
        }
    } else {
        return EmrtdErrorParse;
    }

    EmrtdTlvIter iter;
    EmrtdTlv node;
    emrtd_tlv_iter_init(&iter, body, body_len);

    /*
     * Iteration stops at the first malformed node and keeps what came before
     * it: chips do append padding to EF.CardAccess, and one trailing byte is
     * no reason to refuse a file whose entries all parsed.
     */
    while(emrtd_tlv_iter_next(&iter, &node)) {
        if(node.tag == EMRTD_ASN1_SEQUENCE) {
            security_info_handle(&node, out);
        }
    }

    if(out->entry_count == 0) {
        return EmrtdErrorParse;
    }
    return EmrtdErrorNone;
}

bool emrtd_security_infos_best_pace(const uint8_t* data, size_t len, EmrtdPaceInfo* out) {
    if(out == NULL) {
        return false;
    }

    EmrtdSecurityInfos infos;
    if(emrtd_security_infos_parse(data, len, &infos) != EmrtdErrorNone || !infos.has_pace) {
        return false;
    }

    *out = infos.pace;
    return true;
}

void emrtd_pace_info_describe(const EmrtdPaceInfo* info, char* out, size_t out_size) {
    if(out == NULL || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if(info == NULL) {
        return;
    }

    const char* agreement = "?";
    switch(info->agreement) {
    case EmrtdPaceAgreementEcdh:
        agreement = "ECDH";
        break;
    case EmrtdPaceAgreementDh:
        agreement = "DH";
        break;
    default:
        break;
    }

    const char* mapping = "?";
    switch(info->mapping) {
    case EmrtdPaceMappingGeneric:
        mapping = "GM";
        break;
    case EmrtdPaceMappingIntegrated:
        mapping = "IM";
        break;
    case EmrtdPaceMappingChipAuth:
        mapping = "CAM";
        break;
    default:
        break;
    }

    size_t pos = 0;
    pos = str_append(out, out_size, pos, agreement);
    pos = str_append(out, out_size, pos, "-");
    pos = str_append(out, out_size, pos, mapping);
    pos = str_append(out, out_size, pos, "/");
    pos = str_append(out, out_size, pos, emrtd_cipher_name(info->cipher));
    pos = str_append(out, out_size, pos, ", ");
    if(info->curve != NULL) {
        pos = str_append(out, out_size, pos, info->curve->name);
    } else if(info->parameter_id != 0xFF) {
        pos = str_append(out, out_size, pos, "unknown curve");
    } else {
        pos = str_append(out, out_size, pos, "no curve named");
    }
    out[pos] = '\0';
}

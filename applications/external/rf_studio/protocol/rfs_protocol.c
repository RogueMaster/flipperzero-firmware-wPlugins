#include "rfs_protocol.h"

#include <lib/subghz/subghz_protocol_registry.h>
#include <lib/subghz/protocols/raw.h>

SubGhzEnvironment* rfs_protocol_env_alloc(void) {
    SubGhzEnvironment* env = subghz_environment_alloc();
    /* Rainbow tables are read lazily by the decoders that need them. The KeeLoq keystore is
     * deliberately not loaded: RF Studio identifies rolling codes but does not decrypt them. */
    subghz_environment_set_came_atomo_rainbow_table_file_name(env, SUBGHZ_CAME_ATOMO_DIR_NAME);
    subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
        env, SUBGHZ_ALUTECH_AT_4N_DIR_NAME);
    subghz_environment_set_nice_flor_s_rainbow_table_file_name(env, SUBGHZ_NICE_FLOR_S_DIR_NAME);
    subghz_environment_set_protocol_registry(env, &subghz_protocol_registry);
    return env;
}

const SubGhzProtocol* rfs_protocol_find(const char* name) {
    if(!name || !*name) return NULL;
    return subghz_protocol_registry_get_by_name(&subghz_protocol_registry, name);
}

RfsSignalKind rfs_protocol_kind(const SubGhzProtocol* protocol) {
    if(!protocol) return RfsKindUnknown;
    /* BinRAW exists only in some firmware, so it is matched by name, not by enum value. */
    if(strcmp(protocol->name, "BinRAW") == 0) return RfsKindBinRaw;
    switch(protocol->type) {
    case SubGhzProtocolTypeStatic:
        return RfsKindStatic;
    case SubGhzProtocolTypeDynamic:
        return RfsKindDynamic;
    case SubGhzProtocolTypeRAW:
        return RfsKindRaw;
    default:
        return RfsKindUnknown;
    }
}

bool rfs_protocol_can_send(const SubGhzProtocol* protocol) {
    return protocol && protocol->encoder && (protocol->flag & SubGhzProtocolFlag_Send);
}

RfsReplay rfs_protocol_replay(RfsSignalKind kind, const char* protocol_name, bool verified) {
    if(kind == RfsKindDynamic) return RfsReplayRolling;
    const SubGhzProtocol* protocol = rfs_protocol_find(protocol_name);
    if(!rfs_protocol_can_send(protocol)) return RfsReplayUnsupported;
    switch(kind) {
    case RfsKindStatic:
        return RfsReplayYes;
    case RfsKindRaw:
    case RfsKindBinRaw:
        return verified ? RfsReplayRawVerified : RfsReplayRawUnverified;
    default:
        return RfsReplayUnsupported;
    }
}

bool rfs_replay_allowed(RfsReplay replay) {
    return replay == RfsReplayYes || replay == RfsReplayRawUnverified ||
           replay == RfsReplayRawVerified;
}

const char* rfs_kind_name(RfsSignalKind kind) {
    switch(kind) {
    case RfsKindStatic:
        return "Fixed Code";
    case RfsKindDynamic:
        return "Rolling Code";
    case RfsKindRaw:
        return "Raw";
    case RfsKindBinRaw:
        return "BinRAW";
    default:
        return "Unknown";
    }
}

const char* rfs_kind_tag(RfsSignalKind kind) {
    switch(kind) {
    case RfsKindStatic:
        return "F";
    case RfsKindDynamic:
        return "D";
    case RfsKindRaw:
    case RfsKindBinRaw:
        return "R";
    default:
        return "?";
    }
}

const char* rfs_replay_name(RfsReplay replay) {
    switch(replay) {
    case RfsReplayYes:
        return "Replayable";
    case RfsReplayRawUnverified:
        return "Raw, untested";
    case RfsReplayRawVerified:
        return "Raw, verified";
    case RfsReplayRolling:
        return "Not replayable (rolling code)";
    default:
        return "Not supported by firmware";
    }
}

void rfs_protocol_stats(RfsProtocolStats* stats) {
    memset(stats, 0, sizeof(RfsProtocolStats));
    size_t count = subghz_protocol_registry_count(&subghz_protocol_registry);
    for(size_t i = 0; i < count; i++) {
        const SubGhzProtocol* protocol =
            subghz_protocol_registry_get_by_index(&subghz_protocol_registry, i);
        if(!protocol) continue;
        stats->total++;
        switch(rfs_protocol_kind(protocol)) {
        case RfsKindStatic:
            stats->fixed++;
            break;
        case RfsKindDynamic:
            stats->rolling++;
            break;
        case RfsKindRaw:
        case RfsKindBinRaw:
            stats->raw++;
            break;
        default:
            break;
        }
        if(rfs_protocol_can_send(protocol)) stats->sendable++;
    }
}

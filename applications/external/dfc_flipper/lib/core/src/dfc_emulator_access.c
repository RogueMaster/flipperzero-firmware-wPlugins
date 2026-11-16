#include "dfc_emulator_i.h"

#if DFC_ENABLE_EMULATOR

bool dfc_emulator_is_authenticated_with_key(const DfcEmulator* emulator, uint8_t key_no) {
    return (dfc_emulator_native_authentication_active(emulator) && emulator->auth_key_no == key_no)
#if DFC_ENABLE_EV2_SECURE_MESSAGING
           || (emulator->ev2_session_active && emulator->ev2_authenticated_key_no == key_no)
#endif
        ;
}

bool dfc_emulator_access_nibble_allows(const DfcEmulator* emulator, uint8_t nibble) {
    if(nibble == DFC_ACCESS_FREE) return true;
    if(nibble == DFC_ACCESS_DENY) return false;
    return dfc_emulator_is_authenticated_with_key(emulator, nibble);
}

static bool file_access_nibbles(
    const DfcFile* file,
    DfcFileAccess access,
    uint8_t* primary,
    uint8_t* alternate) {
    *alternate = DFC_ACCESS_DENY;
    switch(access) {
    case DfcFileAccessRead:
        *primary = dfc_access_nibble(file->access_rights, 12);
        *alternate = dfc_access_nibble(file->access_rights, 4);
        break;
    case DfcFileAccessWrite:
        *primary = dfc_access_nibble(file->access_rights, 8);
        *alternate = dfc_access_nibble(file->access_rights, 4);
        break;
    case DfcFileAccessReadWrite:
        *primary = dfc_access_nibble(file->access_rights, 4);
        break;
    case DfcFileAccessChange:
        *primary = dfc_access_nibble(file->access_rights, 0);
        break;
    default:
        return false;
    }
    return true;
}

bool dfc_emulator_file_access_allowed(
    const DfcEmulator* emulator,
    const DfcFile* file,
    DfcFileAccess access) {
    uint8_t primary;
    uint8_t alternate;
    if(!file_access_nibbles(file, access, &primary, &alternate)) return false;

    return dfc_emulator_access_nibble_allows(emulator, primary) ||
           dfc_emulator_access_nibble_allows(emulator, alternate);
}

bool dfc_emulator_file_access_is_free(const DfcFile* file, DfcFileAccess access) {
    uint8_t primary;
    uint8_t alternate;
    if(!file_access_nibbles(file, access, &primary, &alternate)) return false;
    return primary == DFC_ACCESS_FREE || alternate == DFC_ACCESS_FREE;
}

uint8_t dfc_emulator_file_effective_comm_settings(
    const DfcEmulator* emulator,
    const DfcFile* file,
    DfcFileAccess access,
    uint8_t configured_comm) {
    bool authenticated = dfc_emulator_native_authentication_active(emulator)
#if DFC_ENABLE_EV2_SECURE_MESSAGING
                         || emulator->ev2_session_active
#endif
        ;
    if(!authenticated) return configured_comm;
    uint8_t primary;
    uint8_t alternate;
    if(!file_access_nibbles(file, access, &primary, &alternate)) return configured_comm;

    bool free_access = primary == DFC_ACCESS_FREE || alternate == DFC_ACCESS_FREE;
    bool authenticated_access =
        (primary < DFC_ACCESS_FREE && dfc_emulator_is_authenticated_with_key(emulator, primary)) ||
        (alternate < DFC_ACCESS_FREE &&
         dfc_emulator_is_authenticated_with_key(emulator, alternate));
    return free_access && !authenticated_access ? DFC_COMM_PLAIN : configured_comm;
}

#endif

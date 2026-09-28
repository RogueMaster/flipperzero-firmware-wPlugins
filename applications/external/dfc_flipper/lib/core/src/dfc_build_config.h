#pragma once

// Select one preset. A build without an explicit preset uses the full EV3 profile.
#define DFC_PROFILE_MINIMAL_EV1 1
#define DFC_PROFILE_FULL_EV1    2
#define DFC_PROFILE_FULL_EV2    3
#define DFC_PROFILE_FULL_EV3    4
#define DFC_PROFILE_MINIMAL_EV3 5

#ifndef DFC_BUILD_PROFILE
#define DFC_BUILD_PROFILE DFC_PROFILE_FULL_EV3
#endif

#if DFC_BUILD_PROFILE < DFC_PROFILE_MINIMAL_EV1 || DFC_BUILD_PROFILE > DFC_PROFILE_MINIMAL_EV3
#error "DFC_BUILD_PROFILE is not a supported profile"
#endif

#define DFC_PROFILE_INCLUDES_FULL_EV1 \
    (DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV1 || DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV2 || \
     DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3)
#define DFC_PROFILE_INCLUDES_EV2 \
    (DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV2 || DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3)
#define DFC_PROFILE_INCLUDES_EV3 \
    (DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3 || DFC_BUILD_PROFILE == DFC_PROFILE_MINIMAL_EV3)

#ifndef DFC_ENABLE_GENERATION_EV1
#define DFC_ENABLE_GENERATION_EV1 1
#endif
#ifndef DFC_ENABLE_GENERATION_EV2
#define DFC_ENABLE_GENERATION_EV2 DFC_PROFILE_INCLUDES_EV2
#endif
#ifndef DFC_ENABLE_GENERATION_EV3
#define DFC_ENABLE_GENERATION_EV3 DFC_PROFILE_INCLUDES_EV3
#endif

#ifndef DFC_ENABLE_STORAGE_2K
#define DFC_ENABLE_STORAGE_2K 1
#endif
#ifndef DFC_ENABLE_STORAGE_4K
#define DFC_ENABLE_STORAGE_4K DFC_PROFILE_INCLUDES_FULL_EV1
#endif
#ifndef DFC_ENABLE_STORAGE_8K
#define DFC_ENABLE_STORAGE_8K DFC_PROFILE_INCLUDES_FULL_EV1
#endif

#ifndef DFC_ENABLE_AUTH_D40
#define DFC_ENABLE_AUTH_D40 1
#endif
#ifndef DFC_ENABLE_AUTH_ISO
#define DFC_ENABLE_AUTH_ISO DFC_PROFILE_INCLUDES_FULL_EV1
#endif
#ifndef DFC_ENABLE_AUTH_AES
#define DFC_ENABLE_AUTH_AES DFC_PROFILE_INCLUDES_FULL_EV1
#endif
#ifndef DFC_ENABLE_EV2_SECURE_MESSAGING
#define DFC_ENABLE_EV2_SECURE_MESSAGING DFC_PROFILE_INCLUDES_EV2
#endif
#ifndef DFC_ENABLE_ISO7816
#define DFC_ENABLE_ISO7816 DFC_PROFILE_INCLUDES_FULL_EV1
#endif
#ifndef DFC_ENABLE_ISO7816_AUTH
#define DFC_ENABLE_ISO7816_AUTH DFC_ENABLE_ISO7816
#endif
#ifndef DFC_ENABLE_SPECIAL_KEY_STATUS
#define DFC_ENABLE_SPECIAL_KEY_STATUS (DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3)
#endif

#ifndef DFC_ENABLE_BACKUP_FILES
#define DFC_ENABLE_BACKUP_FILES DFC_PROFILE_INCLUDES_FULL_EV1
#endif
#ifndef DFC_ENABLE_VALUE_FILES
#define DFC_ENABLE_VALUE_FILES DFC_PROFILE_INCLUDES_FULL_EV1
#endif
#ifndef DFC_ENABLE_RECORD_FILES
#define DFC_ENABLE_RECORD_FILES DFC_PROFILE_INCLUDES_FULL_EV1
#endif
#ifndef DFC_ENABLE_TRANSACTION_MAC
#define DFC_ENABLE_TRANSACTION_MAC DFC_PROFILE_INCLUDES_EV2
#endif
#define DFC_ENABLE_TRANSACTIONAL_DATA_FILES \
    (DFC_ENABLE_BACKUP_FILES || DFC_ENABLE_RECORD_FILES)
#ifndef DFC_ENABLE_SDM
#define DFC_ENABLE_SDM (DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3)
#endif
#ifndef DFC_ENABLE_PROXIMITY_CHECK
#define DFC_ENABLE_PROXIMITY_CHECK DFC_PROFILE_INCLUDES_EV3
#endif
#ifndef DFC_ENABLE_VIRTUAL_CARD
#define DFC_ENABLE_VIRTUAL_CARD DFC_PROFILE_INCLUDES_EV2
#endif
#ifndef DFC_ENABLE_KEY_SETS
#define DFC_ENABLE_KEY_SETS DFC_PROFILE_INCLUDES_EV2
#endif
#ifndef DFC_ENABLE_DELEGATED_APPLICATIONS
#define DFC_ENABLE_DELEGATED_APPLICATIONS DFC_PROFILE_INCLUDES_EV2
#endif
#ifndef DFC_ENABLE_TRANSACTION_TIMER
#define DFC_ENABLE_TRANSACTION_TIMER (DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3)
#endif
#ifndef DFC_ENABLE_APPLICATION_CAPABILITY_DATA
#define DFC_ENABLE_APPLICATION_CAPABILITY_DATA (DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3)
#endif
#ifndef DFC_ENABLE_STATIC_SIGNATURE
#define DFC_ENABLE_STATIC_SIGNATURE DFC_PROFILE_INCLUDES_EV2
#endif

// Select one role. A role chooses which halves of the library a build carries,
// independently of the profile, which chooses the card generation.
//
//   target     the card itself: emulator plus the .dfcb codec it loads and
//              dumps credentials with. No text codec and no reader.
//   host       a reader-side client: reader, command encoder and both
//              credential encodings, so it can compile .dfc to .dfcb before
//              loading a target. No emulator.
//   simulator  everything, for workstations that run the card and drive it.
//
// A build without an explicit role is a simulator, which is every part.
#define DFC_ROLE_TARGET    1
#define DFC_ROLE_HOST      2
#define DFC_ROLE_SIMULATOR 3

#ifndef DFC_BUILD_ROLE
#define DFC_BUILD_ROLE DFC_ROLE_SIMULATOR
#endif

#if DFC_BUILD_ROLE < DFC_ROLE_TARGET || DFC_BUILD_ROLE > DFC_ROLE_SIMULATOR
#error "DFC_BUILD_ROLE is not a supported role"
#endif

#ifndef DFC_ENABLE_EMULATOR
#define DFC_ENABLE_EMULATOR (DFC_BUILD_ROLE != DFC_ROLE_HOST)
#endif
#ifndef DFC_ENABLE_READER
#define DFC_ENABLE_READER (DFC_BUILD_ROLE != DFC_ROLE_TARGET)
#endif

// DFC_ENABLE_BINARY_CODEC predates the split between the two directions of the
// .dfcb codec. Setting it still sets both.
#ifdef DFC_ENABLE_BINARY_CODEC
#ifndef DFC_ENABLE_DER_DECODER
#define DFC_ENABLE_DER_DECODER DFC_ENABLE_BINARY_CODEC
#endif
#ifndef DFC_ENABLE_DER_ENCODER
#define DFC_ENABLE_DER_ENCODER DFC_ENABLE_BINARY_CODEC
#endif
#endif
#ifndef DFC_ENABLE_DER_DECODER
#define DFC_ENABLE_DER_DECODER 1
#endif
// A target that never dumps its credential can drop the encoder.
#ifndef DFC_ENABLE_DER_ENCODER
#define DFC_ENABLE_DER_ENCODER 1
#endif
#ifndef DFC_ENABLE_BINARY_CODEC
#define DFC_ENABLE_BINARY_CODEC (DFC_ENABLE_DER_DECODER || DFC_ENABLE_DER_ENCODER)
#endif
#ifndef DFC_ENABLE_TEXT_CODEC
#define DFC_ENABLE_TEXT_CODEC (DFC_BUILD_ROLE != DFC_ROLE_TARGET)
#endif
// The flat, handle-based interface foreign runtimes bind to. It needs a hosted
// C library, so a build asks for it explicitly.
#ifndef DFC_ENABLE_FFI
#define DFC_ENABLE_FFI 0
#endif

#if DFC_ENABLE_GENERATION_EV2 && !DFC_ENABLE_GENERATION_EV1
#error "EV2 requires EV1 generation support"
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING && !DFC_ENABLE_AUTH_AES
#error "EV2 secure messaging requires AES authentication"
#endif
#if DFC_ENABLE_ISO7816_AUTH && !DFC_ENABLE_ISO7816
#error "ISO 7816 authentication requires ISO 7816 support"
#endif
#if DFC_ENABLE_TRANSACTION_MAC && !DFC_ENABLE_EV2_SECURE_MESSAGING
#error "Transaction MAC requires EV2 secure messaging"
#endif
#if DFC_ENABLE_SDM && !DFC_ENABLE_GENERATION_EV3
#error "Secure dynamic messaging requires EV3 generation support"
#endif
#if DFC_ENABLE_PROXIMITY_CHECK && !DFC_ENABLE_GENERATION_EV3
#error "Proximity check requires EV3 generation support"
#endif
#if DFC_ENABLE_TRANSACTION_TIMER && !DFC_ENABLE_GENERATION_EV3
#error "Transaction timer requires EV3 generation support"
#endif
#if DFC_ENABLE_APPLICATION_CAPABILITY_DATA && !DFC_ENABLE_GENERATION_EV3
#error "Application capability data requires EV3 generation support"
#endif
#if DFC_ENABLE_STATIC_SIGNATURE && !DFC_ENABLE_GENERATION_EV2 && !DFC_ENABLE_GENERATION_EV3
#error "Static signature requires EV2 or EV3 generation support"
#endif
#if DFC_ENABLE_TEXT_CODEC && !(DFC_ENABLE_DER_DECODER && DFC_ENABLE_DER_ENCODER)
#error "The text codec requires both directions of the binary codec"
#endif
#if !DFC_ENABLE_EMULATOR && !DFC_ENABLE_READER
#error "A build needs the emulator, the reader, or both"
#endif
#if DFC_ENABLE_FFI && !DFC_ENABLE_DER_DECODER
#error "The FFI interface requires the binary decoder"
#endif

enum {
    DfcStorage2KByteCount = 2048,
    DfcStorage4KByteCount = 4096,
    DfcStorage8KByteCount = 8192,
};

typedef struct {
    unsigned int generation_ev1 : 1;
    unsigned int generation_ev2 : 1;
    unsigned int generation_ev3 : 1;
    unsigned int storage_2k : 1;
    unsigned int storage_4k : 1;
    unsigned int storage_8k : 1;
    unsigned int ev2_secure_messaging : 1;
    unsigned int iso7816 : 1;
    unsigned int transaction_mac : 1;
    unsigned int secure_dynamic_messaging : 1;
    unsigned int proximity_check : 1;
    unsigned int virtual_card : 1;
    unsigned int key_sets : 1;
    unsigned int delegated_applications : 1;
    unsigned int transaction_timer : 1;
    unsigned int application_capability_data : 1;
    unsigned int static_signature : 1;
    unsigned int auth_d40 : 1;
    unsigned int auth_iso : 1;
    unsigned int auth_aes : 1;
    unsigned int emulator : 1;
    unsigned int reader : 1;
    unsigned int der_decoder : 1;
    unsigned int der_encoder : 1;
    unsigned int text_codec : 1;
} DfcBuildCapabilities;

static inline DfcBuildCapabilities dfc_build_capabilities(void) {
    return (DfcBuildCapabilities){
        .generation_ev1 = DFC_ENABLE_GENERATION_EV1,
        .generation_ev2 = DFC_ENABLE_GENERATION_EV2,
        .generation_ev3 = DFC_ENABLE_GENERATION_EV3,
        .storage_2k = DFC_ENABLE_STORAGE_2K,
        .storage_4k = DFC_ENABLE_STORAGE_4K,
        .storage_8k = DFC_ENABLE_STORAGE_8K,
        .ev2_secure_messaging = DFC_ENABLE_EV2_SECURE_MESSAGING,
        .iso7816 = DFC_ENABLE_ISO7816,
        .transaction_mac = DFC_ENABLE_TRANSACTION_MAC,
        .secure_dynamic_messaging = DFC_ENABLE_SDM,
        .proximity_check = DFC_ENABLE_PROXIMITY_CHECK,
        .virtual_card = DFC_ENABLE_VIRTUAL_CARD,
        .key_sets = DFC_ENABLE_KEY_SETS,
        .delegated_applications = DFC_ENABLE_DELEGATED_APPLICATIONS,
        .transaction_timer = DFC_ENABLE_TRANSACTION_TIMER,
        .application_capability_data = DFC_ENABLE_APPLICATION_CAPABILITY_DATA,
        .static_signature = DFC_ENABLE_STATIC_SIGNATURE,
        .auth_d40 = DFC_ENABLE_AUTH_D40,
        .auth_iso = DFC_ENABLE_AUTH_ISO,
        .auth_aes = DFC_ENABLE_AUTH_AES,
        .emulator = DFC_ENABLE_EMULATOR,
        .reader = DFC_ENABLE_READER,
        .der_decoder = DFC_ENABLE_DER_DECODER,
        .der_encoder = DFC_ENABLE_DER_ENCODER,
        .text_codec = DFC_ENABLE_TEXT_CODEC,
    };
}

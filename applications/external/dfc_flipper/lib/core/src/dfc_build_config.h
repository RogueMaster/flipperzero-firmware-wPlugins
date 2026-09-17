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
#define DFC_ENABLE_STATIC_SIGNATURE (DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3)
#endif

#ifndef DFC_ENABLE_TEXT_CODEC
#define DFC_ENABLE_TEXT_CODEC 1
#endif
#ifndef DFC_ENABLE_BINARY_CODEC
#define DFC_ENABLE_BINARY_CODEC 1
#endif

#if DFC_ENABLE_GENERATION_EV2 && !DFC_ENABLE_GENERATION_EV1
#error "EV2 requires EV1 generation support"
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING && !DFC_ENABLE_AUTH_AES
#error "EV2 secure messaging requires AES authentication"
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
#if DFC_ENABLE_STATIC_SIGNATURE && !DFC_ENABLE_GENERATION_EV3
#error "Static signature requires EV3 generation support"
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
    };
}

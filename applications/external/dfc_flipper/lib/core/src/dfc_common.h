#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "dfc_port.h"
#include "dfc_bytebuf.h"

#include "aes_cmac.h"
#include "des_cmac.h"
#include "dfc_build_config.h"
#include "dfc_crypto.h"

// DESFire native command bytes used by this app
#define DFC_CMD_AUTHENTICATE_LEGACY  0x0A
#define DFC_CMD_AUTHENTICATE_ISO     0x1A
#define DFC_CMD_AUTHENTICATE_AES     0xAA
#define DFC_CMD_AUTHENTICATE_EV2_FIRST 0x71
#define DFC_CMD_AUTHENTICATE_EV2_NON_FIRST 0x77
#define DFC_CMD_ADDITIONAL_FRAME     0xAF
#define DFC_CMD_CHANGE_KEY_SETTINGS  0x54
#define DFC_CMD_GET_VERSION          0x60
#define DFC_CMD_GET_CARD_UID         0x51
#define DFC_CMD_SELECT_APPLICATION   0x5A
#define DFC_CMD_GET_KEY_SETTINGS     0x45
#define DFC_CMD_GET_KEY_VERSION      0x64
#define DFC_CMD_INITIALIZE_KEY_SET   0x56
#define DFC_CMD_FINALIZE_KEY_SET     0x57
#define DFC_CMD_ROLL_KEY_SET         0x55
#define DFC_CMD_GET_APPLICATION_IDS  0x6A
#define DFC_CMD_GET_DF_NAMES         0x6D
#define DFC_CMD_FREE_MEM             0x6E
#define DFC_CMD_CREATE_APPLICATION   0xCA
#define DFC_CMD_DELETE_APPLICATION   0xDA
#define DFC_CMD_FORMAT_PICC          0xFC
#define DFC_CMD_GET_ISO_FILE_IDS     0x61
#define DFC_CMD_GET_FILE_IDS         0x6F
#define DFC_CMD_GET_FILE_SETTINGS    0xF5
#define DFC_CMD_CHANGE_FILE_SETTINGS 0x5F
#define DFC_CMD_CREATE_STD_DATA_FILE 0xCD
#define DFC_CMD_CREATE_BACKUP_DATA_FILE 0xCB
#define DFC_CMD_CREATE_LINEAR_RECORD_FILE 0xC1
#define DFC_CMD_CREATE_CYCLIC_RECORD_FILE 0xC0
#define DFC_CMD_CREATE_VALUE_FILE    0xCC
#define DFC_CMD_DELETE_FILE          0xDF
#define DFC_CMD_CHANGE_KEY           0xC4
#define DFC_CMD_CHANGE_KEY_EV2       0xC6
#define DFC_CMD_CREATE_DELEGATED_APPLICATION 0xC9
#define DFC_CMD_GET_DELEGATED_INFO   0x69
#define DFC_CMD_GET_VALUE            0x6C
#define DFC_CMD_CREDIT               0x0C
#define DFC_CMD_LIMITED_CREDIT       0x1C
#define DFC_CMD_DEBIT                0xDC
#define DFC_CMD_COMMIT_TRANSACTION   0xC7
#define DFC_CMD_ABORT_TRANSACTION    0xA7
#define DFC_CMD_READ_DATA            0xBD
#define DFC_CMD_WRITE_DATA           0x3D
#define DFC_CMD_WRITE_RECORD         0x3B
#define DFC_CMD_READ_RECORDS         0xBB
#define DFC_CMD_UPDATE_RECORD        0xDB
#define DFC_CMD_UPDATE_RECORD_ISO    0xBA
#define DFC_CMD_CLEAR_RECORD_FILE    0xEB
#define DFC_CMD_PREPARE_PROXIMITY_CHECK 0xF0
#define DFC_CMD_PROXIMITY_CHECK         0xF2
#define DFC_CMD_VERIFY_PROXIMITY_CHECK  0xFD
#define DFC_CMD_READ_SIGNATURE          0x3C
#define DFC_CMD_SET_CONFIGURATION       0x5C

// ISO7816-4 framing: readers/drivers commonly probe with a standard SELECT (CLA=0x00,
// INS=0xA4) before or instead of native framing, and some PC/SC paths pass native DESFire
// commands through wrapped in an ISO7816 command envelope (CLA=0x90, INS=<native cmd>,
// P1=P2=0x00, Lc, data, Le=0x00), responding with <data> 0x91 <native status>.
#define DFC_ISO7816_CLA_STANDARD            0x00
#define DFC_ISO7816_CLA_WRAPPER             0x90
#define DFC_ISO7816_INS_SELECT              0xA4
#define DFC_ISO7816_INS_READ_BINARY         0xB0
#define DFC_ISO7816_INS_UPDATE_BINARY       0xD6
#define DFC_ISO7816_INS_READ_RECORD         0xB2
#define DFC_ISO7816_SELECT_BY_FILE_ID        0x00
#define DFC_ISO7816_SELECT_CHILD_DF          0x01
#define DFC_ISO7816_SELECT_CHILD_EF          0x02
#define DFC_ISO7816_SELECT_PARENT            0x03
#define DFC_ISO7816_SELECT_BY_DF_NAME        0x04
#define DFC_ISO7816_INS_EXTERNAL_AUTHENTICATE 0x82
#define DFC_ISO7816_SELECT_PATH_FROM_MF      0x08
#define DFC_ISO7816_SELECT_PATH_FROM_DF      0x09
#define DFC_ISO7816_STATUS_WORD_LENGTH       2
#define DFC_ISO7816_MASTER_FILE_ID            0x3F00
#define DFC_ISO7816_SW_OK_HI                0x90
#define DFC_ISO7816_SW_OK_LO                0x00
#define DFC_ISO7816_SW_NOT_FOUND_HI         0x6A
#define DFC_ISO7816_SW_NOT_FOUND_LO         0x82
#define DFC_ISO7816_SW_WRONG_PARAMETERS_HI  0x6A
#define DFC_ISO7816_SW_WRONG_PARAMETERS_LO  0x86
#define DFC_ISO7816_SW_INS_NOT_SUPPORTED_HI 0x6D
#define DFC_ISO7816_SW_INS_NOT_SUPPORTED_LO 0x00
// DESFire ISO7816 DF name, used to select the PICC/master file via standard
// SELECT-by-DF-name (P1=0x04) regardless of which application AID is currently active.
extern const uint8_t DFC_ISO_AID[7];

#define DFC_STATUS_OK                    0x00
#define DFC_STATUS_NO_CHANGES            0x0C
#define DFC_STATUS_PROXIMITY_KEY_DISABLED 0x0B
#define DFC_STATUS_OUT_OF_EEPROM         0x0E
#define DFC_STATUS_ILLEGAL_COMMAND_CODE  0x1C // command code not supported/recognized
#define DFC_STATUS_INTEGRITY_ERROR       0x1E
#define DFC_STATUS_NO_SUCH_KEY           0x40
#define DFC_STATUS_LENGTH_ERROR          0x7E
#define DFC_STATUS_SPECIAL_SUCCESS       0x90
#define DFC_STATUS_PERMISSION_DENIED     0x9D
#define DFC_STATUS_PARAMETER_ERROR       0x9E
#define DFC_STATUS_APPLICATION_NOT_FOUND 0xA0
#define DFC_STATUS_AUTHENTICATION_ERR    0xAE
#define DFC_STATUS_BOUNDARY_ERROR        0xBE
#define DFC_STATUS_COMMAND_ABORTED       0xCA
#define DFC_STATUS_COUNT_ERROR           0xCE
#define DFC_STATUS_DUPLICATE_ERROR       0xDE
#define DFC_STATUS_FILE_NOT_FOUND        0xF0

#define DFC_CHANGE_KEY_EV2_HEADER_LENGTH 3
#define DFC_CHANGE_KEY_EV2_AES_SHORT_CRYPTOGRAM_LENGTH 24
#define DFC_CHANGE_KEY_EV2_AES_CRYPTOGRAM_LENGTH 32
#define DFC_CHANGE_KEY_EV2_LONG_CRYPTOGRAM_LENGTH 40
#define DFC_CHANGE_KEY_EV2_VERSION_LENGTH 1
#define DFC_CHANGE_KEY_EV2_CRC_LENGTH 4
#define DFC_CHANGE_KEY_EV2_SAME_KEY_CLEAR_LENGTH \
    (DFC_CHANGE_KEY_EV2_HEADER_LENGTH + DFC_AES_KEY_LENGTH + \
     DFC_CHANGE_KEY_EV2_VERSION_LENGTH)
#define DFC_CHANGE_KEY_EV2_OTHER_KEY_CLEAR_LENGTH \
    (DFC_CHANGE_KEY_EV2_SAME_KEY_CLEAR_LENGTH + DFC_CHANGE_KEY_EV2_CRC_LENGTH)
#define DFC_DELEGATED_CREATE_HEADER_LENGTH 11
#define DFC_DELEGATED_INFO_COMMAND_LENGTH 3
#define DFC_DAM_AUTH_KEY_NUMBER 0x10
#define DFC_DAM_MAC_KEY_NUMBER 0x11
#define DFC_DAM_ENCRYPTION_KEY_NUMBER 0x12
#define DFC_DELEGATED_ENCRYPTED_KEY_LENGTH 32
#define DFC_DELEGATED_MAC_LENGTH DFC_WIRE_MAC_LENGTH
#define DFC_DELEGATED_SECOND_FRAME_LENGTH \
    (1 + DFC_DELEGATED_ENCRYPTED_KEY_LENGTH + DFC_DELEGATED_MAC_LENGTH)
#define DFC_DELEGATED_RANDOM_PREFIX_LENGTH 7
#define DFC_DELEGATED_DEFAULT_KEY_LENGTH 24
#define DFC_DELEGATED_DEFAULT_KEY_VERSION_LENGTH 1
#define DFC_DELEGATED_CREATE_MAX_HEADER_LENGTH 34
#define DFC_DELEGATED_AID_OFFSET 1
#define DFC_DELEGATED_SLOT_OFFSET 4
#define DFC_DELEGATED_SLOT_VERSION_OFFSET 6
#define DFC_DELEGATED_QUOTA_OFFSET 7
#define DFC_DELEGATED_KEY_SETTINGS_1_OFFSET 9
#define DFC_DELEGATED_KEY_SETTINGS_2_OFFSET 10
#define DFC_APPLICATION_GENERAL_OVERHEAD_BLOCKS 1
#define DFC_DELEGATED_APPLICATION_OVERHEAD_BLOCKS 1
#define DFC_KEY_STORAGE_INDEX_BLOCKS 1
#define DFC_KEY_STORAGE_KEYS_PER_DATA_BLOCK 4
#define DFC_KEY_STORAGE_UNITS_PER_16_BYTE_KEY 2
#define DFC_KEY_STORAGE_UNITS_PER_24_BYTE_KEY 3
#define DFC_KEY_STORAGE_SECOND_INDEX_THRESHOLD 3
#define DFC_KEY_STORAGE_THIRD_INDEX_THRESHOLD 11

// Key Settings 1 policy bits (EV1 / earlier)
#define DFC_KS1_MASTER_KEY_CHANGEABLE 0x01
#define DFC_KS1_FREE_DIRECTORY_ACCESS 0x02
#define DFC_KS1_FREE_CREATE_DELETE    0x04
#define DFC_KS1_CHANGE_KEY_ACCESS_MASK 0xF0
#define DFC_KS1_CHANGE_KEY_ACCESS_SHIFT 4
#define DFC_CHANGE_KEY_ACCESS_SAME    0x0E
#define DFC_CHANGE_KEY_ACCESS_FROZEN  0x0F

// Access-right nibble values
#define DFC_ACCESS_FREE 0x0E
#define DFC_ACCESS_DENY 0x0F

// EV1 capacity limits
#define DFC_EV1_MAX_APPLICATIONS     28
#define DFC_EV1_MAX_FILES_PER_APP    32
#define DFC_EV1_MAX_FILE_NUMBER      0x1F
#define DFC_EV1_MAX_KEYS_PER_APP     14
#define DFC_MAX_KEY_SETS             16
#define DFC_ADDITIONAL_KEY_SET_COUNT (DFC_MAX_KEY_SETS - 1)
#define DFC_EV1_PICC_STORAGE_BYTES   (8 * 1024)
#define DFC_EV1_MAX_FRAME_PAYLOAD    54

// Key Settings 2 crypto-suite bits (upper nibble) and ISO FID enable (bit 5)
#define DFC_KEY_TYPE_DES_2K3DES 0x00
#define DFC_KEY_TYPE_3K3DES     0x40
#define DFC_KEY_TYPE_AES        0x80
#define DFC_KEY_TYPE_MASK       0xC0
#define DFC_KEY_TYPE_SHIFT      6
#define DFC_NUM_KEYS_MASK       0x0F
#define DFC_KS2_ISO_FILE_IDS    0x20 // bit 5: 2-byte ISO FIDs for files in the app
#define DFC_KS2_EXTENDED_SETTINGS 0x10

// Extended application and key-set fields.
#define DFC_EXTENDED_SETTINGS_KEY_SETS          0x01
#define DFC_KEY_SET_NUMBER_MASK                 0x0F
#define DFC_KEY_SET_SECOND_APPLICATION_MASK     0x80
#define DFC_KEY_SET_RETRIEVAL_MASK              0x80
#define DFC_KEY_SET_NUMBER_PRESENT_MASK         0x40
#define DFC_KEY_NUMBER_MASK                     0x3F
#define DFC_KEY_SET_TYPE_MASK                   0x03
#define DFC_KEY_SET_TYPE_2K3DES                 0x00
#define DFC_KEY_SET_TYPE_3K3DES                 0x01
#define DFC_KEY_SET_TYPE_AES                    0x02
#define DFC_KEY_SET_MINIMUM_COUNT               2
#define DFC_KEY_SET_INITIAL_VERSION             0x00
#define DFC_KEY_SET_MAXIMUM_16_BYTE             0x10
#define DFC_KEY_SET_MAXIMUM_24_BYTE             0x18
#define DFC_CREATE_APPLICATION_KEY_SET_PARAMETER_COUNT 4

// File communication settings
#define DFC_COMM_PLAIN      0x00
#define DFC_COMM_MAC        0x01
#define DFC_COMM_ENCIPHERED 0x03

#define DFC_FILE_TYPE_STANDARD_DATA 0x00
#define DFC_FILE_TYPE_BACKUP_DATA   0x01
#define DFC_FILE_TYPE_VALUE         0x02
#define DFC_FILE_TYPE_LINEAR_RECORD 0x03
#define DFC_FILE_TYPE_CYCLIC_RECORD 0x04
#define DFC_FILE_TYPE_TRANSACTION_MAC 0x05

#define DFC_EV3_4K_FREE_MEMORY_BYTES 5120

#define DFC_WORKER_MAX_BUFFER_SIZE         128
#define DFC_WORKER_CMAC_SIZE               8
#define DFC_SELECT_APPLICATION_FRAME_SIZE  4
#define DFC_READ_DATA_FRAME_SIZE           8
#define DFC_UPDATE_RECORD_HEADER_SIZE      11
#define DFC_WRAPPED_GET_VERSION_FRAME_SIZE 5

#define DFC_APP_EXTENSION        ".dfc"
// Compiled credential octets, the emulator's native input.
#define DFC_BINARY_EXTENSION     ".dfcb"
#define DFC_FILE_NAME_MAX_LENGTH 32

#ifndef DFC_MAX_KEYS
#define DFC_MAX_KEYS 14
#endif
#ifndef DFC_MAX_KEY_LEN
#define DFC_MAX_KEY_LEN 24 // 3K3DES
#endif
#ifndef DFC_MAX_APPS
#define DFC_MAX_APPS DFC_EV1_MAX_APPLICATIONS
#endif
#ifndef DFC_MAX_FILES
#define DFC_MAX_FILES DFC_EV1_MAX_FILES_PER_APP
#endif
// Shared payload pool for standard/backup data files (variable allocation).
// Device builds override this in the firmware Makefile to fit one flash page.
#ifndef DFC_FILE_POOL_SIZE
#define DFC_FILE_POOL_SIZE 8192
#endif
// Largest single CreateStdDataFile size (also used as temporary buffer bound).
#ifndef DFC_MAX_FILE_DATA
#define DFC_MAX_FILE_DATA 2048
#endif
// Shared key-material pool. Every application and the PICC record allocate a
// slice of num_keys * stored key length here, so the worst case is charged once
// for the credential instead of reserved per application.
#ifndef DFC_KEY_POOL_SIZE
#define DFC_KEY_POOL_SIZE 4096
#endif
// Allocation a blank card's first file gets. WriteData bounds-checks against
// the file length and never grows the file, so a zero-length file cannot be
// written to and a blank card would be useless. Kept well under
// DFC_FILE_POOL_SIZE so the card stays useful while leaving room for more
// files.
#ifndef DFC_BLANK_FILE_SIZE
#define DFC_BLANK_FILE_SIZE 256
#endif
#ifndef DFC_DESFIRE_UID_LEN
#define DFC_DESFIRE_UID_LEN 7
#endif
#define DFC_DESFIRE_UID_MAX_LENGTH 10
// A card records a single, a double, or a triple size identifier.
#ifndef DFC_DESFIRE_UID_SHORT_LEN
#define DFC_DESFIRE_UID_SHORT_LEN 4
#endif
#ifndef DFC_DESFIRE_UID_LONG_LEN
#define DFC_DESFIRE_UID_LONG_LEN 10
#endif
// User memory a blank credential advertises. A 2K card is the smallest common
// part, so it is the safest default for something that will be written to.
#ifndef DFC_DEFAULT_CARD_STORAGE
#define DFC_DEFAULT_CARD_STORAGE 2048
#endif
#ifndef DFC_PICC_ATS_MAX
#define DFC_PICC_ATS_MAX 20
#endif
#define DFC_AES_KEY_LENGTH 16
#define DFC_AES_CMAC_LENGTH 16
#define DFC_WIRE_MAC_LENGTH 8
#define DFC_VIRTUAL_CARD_CHALLENGE_LENGTH DFC_AES_KEY_LENGTH
#define DFC_VIRTUAL_CARD_CLEAR_DATA_LENGTH DFC_AES_KEY_LENGTH
#define DFC_VIRTUAL_CARD_CAPABILITY_LENGTH 2
#define DFC_VIRTUAL_CARD_MAX_INSTALLATION_ID_LENGTH 16
#define DFC_VIRTUAL_CARD_UID_MAX_LENGTH 10
#define DFC_SDM_ENABLED_MASK 0x40
#define DFC_SDM_UID_MIRROR_MASK 0x80
#define DFC_SDM_COUNTER_MIRROR_MASK 0x40
#define DFC_SDM_COUNTER_LIMIT_MASK 0x20
#define DFC_SDM_ENCRYPTED_FILE_MASK 0x10
#define DFC_SDM_ASCII_ENCODING_MASK 0x01
#define DFC_SDM_FREE_ACCESS 0x0E
#define DFC_SDM_DENIED_ACCESS 0x0F
#define DFC_SDM_OFFSET_LENGTH 3
#define DFC_SDM_OPTIONS_OFFSET 0
#define DFC_SDM_ACCESS_RIGHTS_OFFSET 1
#define DFC_SDM_BASE_SETTINGS_LENGTH 3
#define DFC_SDM_META_READ_SHIFT 12
#define DFC_SDM_FILE_READ_SHIFT 8
#define DFC_SDM_ACCESS_MASK 0x0F
#define DFC_SDM_PICC_UID_TAG_MASK 0x80
#define DFC_SDM_HEX_EXPANSION 2
#define DFC_SDM_PICC_DATA_BLOCK_LENGTH DFC_AES_KEY_LENGTH
#define DFC_SDM_HIDDEN_COUNTER_OFFSET 0xFFFFFFu
#define DFC_CMD_GET_FILE_COUNTERS 0xF6
#define DFC_CMD_CREATE_TRANSACTION_MAC_FILE 0xCE
#define DFC_CMD_COMMIT_READER_ID 0xC8
#define DFC_CMD_NOTIFY_TRANSACTION_SUCCESS 0xEE
#define DFC_TRANSACTION_MAC_FILE_LENGTH 12
#define DFC_TRANSACTION_READER_ID_LENGTH DFC_AES_KEY_LENGTH
#define DFC_TRANSACTION_INPUT_MAX_LENGTH 128
#define DFC_TRANSACTION_COMMIT_RETURN_MAC_OPTION 0x01
#define DFC_TRANSACTION_MAC_DERIVATION_LABEL 0x5A
#define DFC_TRANSACTION_ENCRYPTION_DERIVATION_LABEL 0xA5
#define DFC_DERIVATION_COUNTER_HIGH 0x00
#define DFC_DERIVATION_COUNTER_LOW 0x01
#define DFC_DERIVED_AES_KEY_LENGTH_HIGH 0x00
#define DFC_DERIVED_AES_KEY_LENGTH_LOW 0x80
#define DFC_TRANSACTION_VECTOR_COUNTER_OFFSET 5
#define DFC_TRANSACTION_VECTOR_UID_OFFSET 9
#define DFC_SDM_MAC_LABEL_HIGH 0x3C
#define DFC_SDM_MAC_LABEL_LOW 0xC3
#define DFC_SDM_ENCRYPTION_LABEL_HIGH 0xC3
#define DFC_SDM_ENCRYPTION_LABEL_LOW 0x3C
#define DFC_UINT32_BYTE_COUNT 4
#define DFC_UINT24_BYTE_COUNT 3
#define DFC_BITS_PER_BYTE 8
#define DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_HI 0x69
#define DFC_ISO7816_SW_CONDITIONS_NOT_SATISFIED_LO 0x85
#define DFC_ISO7816_SW_WRONG_LENGTH_HI 0x67
#define DFC_ISO7816_SW_WRONG_LENGTH_LO 0x00
#define DFC_FORMAT_VERSION 4
#define DFC_PROXIMITY_RANDOM_LENGTH 8
#define DFC_PROXIMITY_PUBLISHED_TIME_LENGTH 2
#define DFC_PROXIMITY_TRANSCRIPT_MAX (DFC_PROXIMITY_RANDOM_LENGTH * 2)
#define DFC_PROXIMITY_BITRATE_PRESENT 0x01
#define DFC_STATIC_SIGNATURE_LENGTH 56
#define DFC_EV2_RANDOM_LENGTH 16
#define DFC_EV2_TRANSACTION_IDENTIFIER_LENGTH 4
#define DFC_EV2_CAPABILITY_LENGTH 6
#define DFC_EV2_AUTHENTICATION_RESPONSE_LENGTH 32
#define DFC_EV2_SESSION_VECTOR_LENGTH 32
#define DFC_EV2_COUNTER_LENGTH 2
#define DFC_EV2_ENCRYPTION_LABEL_HIGH 0xA5
#define DFC_EV2_ENCRYPTION_LABEL_LOW 0x5A
#define DFC_EV2_MAC_LABEL_HIGH 0x5A
#define DFC_EV2_MAC_LABEL_LOW 0xA5
#define DFC_EV2_DERIVATION_COUNTER_HIGH 0x00
#define DFC_EV2_DERIVATION_COUNTER_LOW 0x01
#define DFC_EV2_DERIVATION_LENGTH_HIGH 0x00
#define DFC_EV2_DERIVATION_LENGTH_LOW 0x80
#define DFC_CONFIGURATION_APPLICATION_CAPABILITIES 0x05
#define DFC_TRANSACTION_TIMER_EXPIRY_MILLISECONDS 1000
#define DFC_SET_CONFIGURATION_OPTION_OFFSET 1
#define DFC_SET_CONFIGURATION_DATA_OFFSET 2
#define DFC_APPLICATION_CAPABILITY_DATA_LENGTH 10
#define DFC_APPLICATION_CARD_CAPABILITY_OFFSET 4
#define DFC_TRANSACTION_TIMER_CAPABILITY_OFFSET 5
#define DFC_TRANSACTION_TIMER_SHORT 0x01
#define DFC_SET_CONFIGURATION_CAPABILITY_COMMAND_LENGTH \
    (DFC_SET_CONFIGURATION_DATA_OFFSET + DFC_APPLICATION_CAPABILITY_DATA_LENGTH)
#define DFC_MASTER_KEY_NUMBER 0
#ifndef DFC_DESFIRE_UID_FIRST_BYTE
#define DFC_DESFIRE_UID_FIRST_BYTE 0x04
#endif
// Random ID: anticollision presents a single-size NFCID1 whose first octet marks
// the identifier as generated rather than allocated.
#ifndef DFC_RANDOM_UID_LEN
#define DFC_RANDOM_UID_LEN 4
#endif
#ifndef DFC_RANDOM_UID_FIRST_BYTE
#define DFC_RANDOM_UID_FIRST_BYTE 0x08
#endif

// Active DESFire authentication session, used to secure ReadData/WriteData
// after a successful Authenticate(D40/ISO/AES) exchange.
typedef struct {
    uint8_t cipher; // DFC_CMD_AUTHENTICATE_LEGACY / _ISO / _AES
    uint8_t key[DFC_MAX_KEY_LEN];
    size_t key_len;
    uint8_t session_key[DFC_MAX_KEY_LEN];
    uint8_t iv[16];
} DfcAuthParameters;

size_t dfc_key_len_for_cipher(uint8_t cipher);
size_t dfc_block_size_for_cipher(uint8_t cipher);

void dfc_log_buffer(char* TAG, char* prefix, uint8_t* buffer, size_t buffer_len);
bool dfc_desfire_uid_is_detectable(const uint8_t* uid, size_t uid_len);
const char* dfc_authentication_mode_to_string(uint8_t auth_command);
bool dfc_authentication_mode_from_string(const char* value, uint8_t* auth_command);
bool dfc_authentication_mode_matches_key_settings(uint8_t auth_command, uint8_t key_settings_2);
void dfc_encode_standard_data_file_settings(
    uint8_t file_type,
    uint8_t communication_settings,
    uint16_t access_rights,
    size_t file_size,
    uint8_t output[7]);
void dfc_build_select_application_frame(
    const uint8_t aid[3],
    uint8_t output[DFC_SELECT_APPLICATION_FRAME_SIZE]);
void dfc_build_read_data_frame(
    uint8_t file_number,
    uint32_t offset,
    uint32_t length,
    uint8_t output[DFC_READ_DATA_FRAME_SIZE]);
void dfc_build_wrapped_get_version_frame(uint8_t output[DFC_WRAPPED_GET_VERSION_FRAME_SIZE]);
bool dfc_wrap_native_response_as_iso7816(
    const uint8_t* native_response,
    size_t native_response_len,
    size_t prefix_len,
    uint8_t* output,
    size_t output_capacity,
    size_t* output_len);

// Rotate an N-byte buffer left by one byte (used in the RndA/RndB challenge rotation).
void dfc_rotate_left(uint8_t* buffer, size_t len);

void dfc_worker_aes_cbc_decrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear);
void dfc_worker_aes_cbc_encrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[16],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted);

void dfc_worker_des_cbc_decrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    size_t length,
    const uint8_t* encrypted,
    uint8_t* clear);
void dfc_worker_des_cbc_encrypt(
    const uint8_t* key,
    size_t key_len,
    uint8_t iv[8],
    size_t length,
    const uint8_t* clear,
    uint8_t* encrypted);

// Session-key derivation from RndA/RndB per DESFire authentication cipher.
// key_len is the authentication key's length (8=DES, 16=2K3DES or AES, 24=3K3DES);
// for cipher==DFC_CMD_AUTHENTICATE_AES key_len is implicitly 16.
// out_len receives the resulting session key length.
//
// `key` is required because length alone does not determine the form. A 16-octet
// key whose halves are equal is the same key twice, so it is single DES, and a
// genuine card then derives the 8-octet session key rather than the 2K3DES one.
// That case is not obscure: every factory key is sixteen zero octets, so it is
// what a reader meets on any application that has not been personalised.
// Measured against a genuine EV1 4K.
void dfc_derive_session_key(
    uint8_t cipher,
    const uint8_t* key,
    size_t key_len,
    const uint8_t* rnd_a,
    const uint8_t* rnd_b,
    uint8_t* session_key,
    size_t* out_len);

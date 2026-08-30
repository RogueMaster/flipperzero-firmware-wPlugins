/* These headers carry C linkage guards, so the table looks its symbols up the
 * way the app defines them rather than with C++ mangling. */
#include "keys.h"
#include "seos_common.h"
#include "seos_protocol.h"
#include "seos_sio_collect.h"
#include "seos_sm_command.h"
#include "seos_sm_event_ui.h"
#include "secure_messaging.h"

/* The app's own functions and objects, exposed so a plugin can resolve them.
 *
 * This is the whole surface the BLE stacks reach back through. Crypto stays
 * on this side of the line deliberately: mbedTLS is linked into the app, and
 * a plugin that called it would have to carry its own copy.
 */
static constexpr auto app_api_table = sort(create_array_t<sym_entry>(
    API_METHOD(seos_emulator_select_aid, void, (BitBuffer*, const uint8_t*, size_t)),
    API_METHOD(
        seos_emulator_select_adf,
        bool,
        (const uint8_t*, size_t, AuthParameters*, SeosCredential*, BitBuffer*)),
    API_METHOD(seos_emulator_general_authenticate_1, void, (BitBuffer*, AuthParameters)),
    API_METHOD(
        seos_emulator_general_authenticate_2,
        bool,
        (const uint8_t*, size_t, SeosCredential*, AuthParameters*, BitBuffer*)),
    API_METHOD(seos_parse_select_aid, bool, (const uint8_t*, size_t, const uint8_t**, size_t*)),
    API_METHOD(seos_parse_select_adf, bool, (const uint8_t*, size_t, const uint8_t**, size_t*)),
    API_METHOD(seos_emulator_shill_authenticate, void, (BitBuffer*)),
    API_METHOD(seos_is_general_authenticate_1, bool, (const uint8_t*, size_t)),
    API_METHOD(seos_is_general_authenticate_2, bool, (const uint8_t*, size_t)),
    API_METHOD(seos_build_general_authenticate_1, void, (uint8_t, uint8_t*)),
    API_METHOD(
        seos_reader_select_adf_response,
        bool,
        (BitBuffer*, size_t, SeosCredential*, AuthParameters*)),
    API_METHOD(seos_parse_sio_response, bool, (const uint8_t*, size_t, uint8_t*, size_t, size_t*)),
    API_METHOD(
        seos_sio_collect_begin,
        void,
        (SeosSioCollector*, BitBuffer*, const uint8_t*, size_t)),
    API_METHOD(
        seos_sio_collect_step,
        SeosSioCollectResult,
        (SeosSioCollector*, const uint8_t*, size_t, BitBuffer*)),
    API_METHOD(seos_parse_ga1_response, bool, (const uint8_t*, size_t, uint8_t*, size_t)),
    API_METHOD(seos_parse_ga2_response, bool, (const uint8_t*, size_t, const uint8_t**, size_t*)),
    API_METHOD(seos_reader_generate_cryptogram, void, (SeosCredential*, AuthParameters*, uint8_t*)),
    API_METHOD(seos_reader_verify_cryptogram, bool, (AuthParameters*, const uint8_t*)),
    API_METHOD(secure_messaging_alloc, SecureMessaging*, (AuthParameters*)),
    API_METHOD(secure_messaging_free, void, (SecureMessaging*)),
    API_METHOD(
        secure_messaging_wrap_apdu,
        bool,
        (SecureMessaging*, uint8_t*, size_t, uint8_t*, size_t, bool, BitBuffer*)),
    API_METHOD(secure_messaging_unwrap_rapdu, bool, (SecureMessaging*, BitBuffer*)),
    API_METHOD(
        seos_sm_command_handle,
        bool,
        (SecureMessaging*,
         SeosCredential*,
         const uint8_t*,
         size_t,
         size_t,
         BitBuffer*,
         SeosSmEventCallback,
         void*)),
    API_METHOD(seos_sm_command_get_response, void, (SecureMessaging*, size_t, uint8_t, BitBuffer*)),
    API_METHOD(seos_sm_command_matches, bool, (const uint8_t*, size_t)),
    API_METHOD(seos_sm_append_status, void, (BitBuffer*, uint16_t)),
    API_METHOD(seos_sm_event_to_view_dispatcher, void, (void*, SeosSmEvent)),
    API_METHOD(seos_worker_random_nonce, void, (uint8_t*, size_t)),
    API_METHOD(seos_log_bitbuffer, void, (char*, char*, BitBuffer*)),
    API_METHOD(seos_log_buffer, void, (char*, char*, uint8_t*, size_t)),
    API_VARIABLE(SEOS_ADF_OID, uint8_t[32]),
    API_VARIABLE(SEOS_ADF_OID_LEN, size_t),
    API_VARIABLE(SEOS_SM_HEADER, const uint8_t[4]),
    API_VARIABLE(SEOS_GET_RESPONSE, const uint8_t[SEOS_GET_RESPONSE_LEN]),
    API_VARIABLE(SEOS_SW_SUCCESS, const uint8_t[2]),
    API_VARIABLE(SEOS_SW_FILE_NOT_FOUND, const uint8_t[2])));

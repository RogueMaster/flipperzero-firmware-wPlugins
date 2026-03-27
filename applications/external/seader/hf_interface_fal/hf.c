#include "hf_interface.h"

#include "../protocol/picopass_poller.h"
#include "../protocol/rfal_picopass.h"

#include <flipper_application/flipper_application.h>
#include <lib/bit_lib/bit_lib.h>
#include <lib/nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <lib/nfc/protocols/mf_classic/mf_classic_poller.h>
#include <nfc/helpers/iso13239_crc.h>

#define TAG "PluginHF"
#ifdef HF_HARDEN_DIAG
#define HF_DIAG_D(...) FURI_LOG_D(TAG, __VA_ARGS__)
#define HF_DIAG_I(...) FURI_LOG_I(TAG, __VA_ARGS__)
#else
#define HF_DIAG_D(...) \
    do {               \
    } while(0)
#define HF_DIAG_I(...) \
    do {               \
    } while(0)
#endif

#define HF_PLUGIN_POLLER_MAX_FWT         (200000U)
#define HF_PLUGIN_POLLER_MAX_BUFFER_SIZE (258U)
#define HF_PLUGIN_MAX_ATS_SIZE           33U

// ATS bit definitions
#define ISO14443_4A_ATS_T0_TA1 (1U << 4)
#define ISO14443_4A_ATS_T0_TB1 (1U << 5)
#define ISO14443_4A_ATS_T0_TC1 (1U << 6)

typedef struct {
    const PluginHfHostApi* api;
    void* host_ctx;
    Nfc* nfc;
    NfcDevice* nfc_device;
    NfcPoller* poller;
    Iso14443_4aPoller* iso14443_4a_poller;
    MfClassicPoller* mfc_poller;
    SeaderCredentialType active_type;
} PluginHfContext;

static const uint8_t plugin_hf_update_block2[] = {RFAL_PICOPASS_CMD_UPDATE, 0x02};
static const uint8_t plugin_hf_select_seos_app[] =
    {0x00, 0xa4, 0x04, 0x00, 0x0a, 0xa0, 0x00, 0x00, 0x04, 0x40, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00};
static const uint8_t plugin_hf_select_desfire_app_no_le[] =
    {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x00};
static const uint8_t plugin_hf_file_not_found[] = {0x6a, 0x82};

static bool plugin_hf_validate_host_api(const PluginHfHostApi* api) {
    if(!api) {
        FURI_LOG_E(TAG, "Missing HF host API");
        return false;
    }

#define HF_REQUIRE_API(field)                             \
    do {                                                  \
        if(!(api->field)) {                               \
            FURI_LOG_E(TAG, "Missing host API: " #field); \
            return false;                                 \
        }                                                 \
    } while(false)

    HF_REQUIRE_API(notify_card_detected);
    HF_REQUIRE_API(notify_worker_exit);
    HF_REQUIRE_API(sam_can_accept_card);
    HF_REQUIRE_API(send_card_detected);
    HF_REQUIRE_API(send_nfc_rx);
    HF_REQUIRE_API(run_conversation);
    HF_REQUIRE_API(set_stage);
    HF_REQUIRE_API(get_stage);
    HF_REQUIRE_API(set_credential_type);
    HF_REQUIRE_API(get_credential_type);
    HF_REQUIRE_API(get_desfire_ev2);
    HF_REQUIRE_API(set_desfire_ev2);
    HF_REQUIRE_API(append_picopass_sio);
    HF_REQUIRE_API(set_14a_sio);
    HF_REQUIRE_API(get_nfc);
    HF_REQUIRE_API(get_nfc_device);
    HF_REQUIRE_API(picopass_detect);
    HF_REQUIRE_API(picopass_start);
    HF_REQUIRE_API(picopass_stop);
    HF_REQUIRE_API(picopass_get_csn);
    HF_REQUIRE_API(picopass_transmit);

#undef HF_REQUIRE_API
    return true;
}

static PluginHfContext* plugin_hf_require_ctx(void* plugin_ctx) {
    PluginHfContext* ctx = plugin_ctx;
    furi_check(ctx);
    furi_check(ctx->api);
    furi_check(ctx->host_ctx);
    furi_check(ctx->nfc);
    furi_check(ctx->nfc_device);
    return ctx;
}

static void plugin_hf_cleanup_pollers(PluginHfContext* ctx) {
    ctx = plugin_hf_require_ctx(ctx);
    if(ctx->poller) {
        nfc_poller_stop(ctx->poller);
        nfc_poller_free(ctx->poller);
        ctx->poller = NULL;
    }
    ctx->iso14443_4a_poller = NULL;
    ctx->mfc_poller = NULL;
    if(ctx->api->picopass_stop) {
        ctx->api->picopass_stop(ctx->host_ctx);
    }
}

static void plugin_hf_set_read_error(PluginHfContext* ctx, const char* text) {
    ctx = plugin_hf_require_ctx(ctx);
    if(ctx->api->set_read_error) {
        ctx->api->set_read_error(ctx->host_ctx, text);
    }
}

static void plugin_hf_add_detected_type(
    SeaderCredentialType* detected_types,
    size_t* detected_type_count,
    size_t detected_capacity,
    SeaderCredentialType type) {
    for(size_t i = 0; i < *detected_type_count; i++) {
        if(detected_types[i] == type) {
            return;
        }
    }

    if(*detected_type_count < detected_capacity) {
        detected_types[*detected_type_count] = type;
        (*detected_type_count)++;
    }
}

static PicopassError plugin_hf_fake_epurse_update(BitBuffer* tx_buffer, BitBuffer* rx_buffer) {
    const uint8_t* buffer = bit_buffer_get_data(tx_buffer);
    uint8_t fake_response[8];
    memset(fake_response, 0, sizeof(fake_response));
    memcpy(fake_response + 0, buffer + 6, 4);
    memcpy(fake_response + 4, buffer + 2, 4);

    bit_buffer_append_bytes(rx_buffer, fake_response, sizeof(fake_response));
    iso13239_crc_append(Iso13239CrcTypePicopass, rx_buffer);

    return PicopassErrorNone;
}

static void
    plugin_hf_capture_sio(PluginHfContext* ctx, BitBuffer* tx_buffer, BitBuffer* rx_buffer) {
    ctx = plugin_hf_require_ctx(ctx);
    furi_check(tx_buffer);
    furi_check(rx_buffer);
    const uint8_t* buffer = bit_buffer_get_data(tx_buffer);
    size_t len = bit_buffer_get_size_bytes(tx_buffer);
    const uint8_t* rx_buffer_data = bit_buffer_get_data(rx_buffer);
    if(!buffer || !rx_buffer_data || len == 0U) return;

    if(ctx->api->get_credential_type(ctx->host_ctx) == SeaderCredentialTypePicopass) {
        if(buffer[0] == RFAL_PICOPASS_CMD_READ4) {
            uint8_t block_num = buffer[1];
            ctx->api->append_picopass_sio(
                ctx->host_ctx, block_num, rx_buffer_data, PICOPASS_BLOCK_LEN * 4);
        }
    } else if(ctx->api->get_credential_type(ctx->host_ctx) == SeaderCredentialType14A) {
        uint8_t desfire_read[] = {0x90, 0xbd, 0x00, 0x00, 0x07, 0x0f, 0x00, 0x00, 0x00};
        if(len == 13 && memcmp(buffer, desfire_read, sizeof(desfire_read)) == 0 &&
           rx_buffer_data[0] == 0x30) {
            size_t sio_len = bit_buffer_get_size_bytes(rx_buffer) - 2;
            ctx->api->set_14a_sio(ctx->host_ctx, rx_buffer_data, sio_len);
        }
    }
}

static void plugin_hf_iso15693_transmit(PluginHfContext* ctx, uint8_t* buffer, size_t len) {
    ctx = plugin_hf_require_ctx(ctx);
    if(!buffer || len == 0U) {
        FURI_LOG_W(TAG, "Skip picopass transmit invalid input");
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
        return;
    }
    BitBuffer* tx_buffer = bit_buffer_alloc(len);
    BitBuffer* rx_buffer = bit_buffer_alloc(HF_PLUGIN_POLLER_MAX_BUFFER_SIZE);
    uint8_t rx_data[HF_PLUGIN_POLLER_MAX_BUFFER_SIZE];
    size_t rx_len = 0U;
    if(!tx_buffer || !rx_buffer) {
        FURI_LOG_E(TAG, "Failed to allocate picopass buffers");
        if(tx_buffer) bit_buffer_free(tx_buffer);
        if(rx_buffer) bit_buffer_free(rx_buffer);
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
        return;
    }

    do {
        bit_buffer_append_bytes(tx_buffer, buffer, len);

        if(memcmp(buffer, plugin_hf_update_block2, sizeof(plugin_hf_update_block2)) == 0) {
            if(plugin_hf_fake_epurse_update(tx_buffer, rx_buffer) != PicopassErrorNone) {
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                break;
            }
        } else {
            if(!ctx->api->picopass_transmit || !ctx->api->picopass_transmit(
                                                   ctx->host_ctx,
                                                   buffer,
                                                   len,
                                                   rx_data,
                                                   sizeof(rx_data),
                                                   &rx_len,
                                                   HF_PLUGIN_POLLER_MAX_FWT)) {
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                break;
            }
            bit_buffer_append_bytes(rx_buffer, rx_data, rx_len);
        }

        plugin_hf_capture_sio(ctx, tx_buffer, rx_buffer);
        ctx->api->send_nfc_rx(
            ctx->host_ctx,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));
    } while(false);

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

static void plugin_hf_iso14443a_transmit(
    PluginHfContext* ctx,
    uint8_t* buffer,
    size_t len,
    uint16_t timeout,
    uint8_t format[3]) {
    UNUSED(timeout);
    UNUSED(format);

    ctx = plugin_hf_require_ctx(ctx);
    if(!buffer || len == 0U || !ctx->iso14443_4a_poller) {
        FURI_LOG_W(TAG, "Skip 14A transmit invalid state");
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
        return;
    }

    BitBuffer* tx_buffer = bit_buffer_alloc(len + 1U);
    BitBuffer* rx_buffer = bit_buffer_alloc(HF_PLUGIN_POLLER_MAX_BUFFER_SIZE);
    if(!tx_buffer || !rx_buffer) {
        FURI_LOG_E(TAG, "Failed to allocate 14A buffers");
        if(tx_buffer) bit_buffer_free(tx_buffer);
        if(rx_buffer) bit_buffer_free(rx_buffer);
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
        return;
    }

    do {
        bit_buffer_append_bytes(tx_buffer, buffer, len);

        if(ctx->api->get_desfire_ev2(ctx->host_ctx) &&
           sizeof(plugin_hf_select_desfire_app_no_le) == len &&
           memcmp(buffer, plugin_hf_select_desfire_app_no_le, len) == 0) {
            bit_buffer_append_byte(tx_buffer, 0x00);
        }

        Iso14443_4aError error =
            iso14443_4a_poller_send_block(ctx->iso14443_4a_poller, tx_buffer, rx_buffer);
        if(error != Iso14443_4aErrorNone) {
            FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
            ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
            break;
        }

        if(sizeof(plugin_hf_select_seos_app) == len &&
           memcmp(buffer, plugin_hf_select_seos_app, len) == 0 &&
           bit_buffer_get_size_bytes(rx_buffer) == 38) {
            const uint8_t ev2_select_reply_prefix[] = {0x6F, 0x22, 0x85, 0x20};
            const uint8_t* rapdu = bit_buffer_get_data(rx_buffer);
            if(memcmp(ev2_select_reply_prefix, rapdu, sizeof(ev2_select_reply_prefix)) == 0) {
                ctx->api->set_desfire_ev2(ctx->host_ctx, true);
                bit_buffer_reset(rx_buffer);
                bit_buffer_append_bytes(
                    rx_buffer, plugin_hf_file_not_found, sizeof(plugin_hf_file_not_found));
            }
        }

        plugin_hf_capture_sio(ctx, tx_buffer, rx_buffer);
        ctx->api->send_nfc_rx(
            ctx->host_ctx,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));
    } while(false);

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

static void plugin_hf_mfc_transmit(
    PluginHfContext* ctx,
    uint8_t* buffer,
    size_t len,
    uint16_t timeout,
    uint8_t format[3]) {
    UNUSED(timeout);

    ctx = plugin_hf_require_ctx(ctx);
    if(!buffer || len == 0U || !ctx->mfc_poller) {
        FURI_LOG_W(TAG, "Skip MFC transmit invalid state");
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
        return;
    }

    BitBuffer* tx_buffer = bit_buffer_alloc(len);
    BitBuffer* rx_buffer = bit_buffer_alloc(HF_PLUGIN_POLLER_MAX_BUFFER_SIZE);
    if(!tx_buffer || !rx_buffer) {
        FURI_LOG_E(TAG, "Failed to allocate MFC buffers");
        if(tx_buffer) bit_buffer_free(tx_buffer);
        if(rx_buffer) bit_buffer_free(rx_buffer);
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
        return;
    }

    do {
        if(format[0] == 0x00 && format[1] == 0xC0 && format[2] == 0x00) {
            bit_buffer_append_bytes(tx_buffer, buffer, len);
            MfClassicError error =
                mf_classic_poller_send_frame(ctx->mfc_poller, tx_buffer, rx_buffer, 60000);
            if(error != MfClassicErrorNone) {
                FURI_LOG_W(TAG, "mf_classic_poller_send_frame error %d", error);
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                break;
            }
        } else if(
            (format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x40) ||
            (format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x24) ||
            (format[0] == 0x00 && format[1] == 0x00 && format[2] == 0x44)) {
            uint8_t tx_parity = 0;
            uint8_t len_without_parity = len - 1;

            for(size_t i = 0; i < len; i++) {
                bit_lib_reverse_bits(buffer + i, 0, 8);
            }

            for(size_t i = 0; i < len_without_parity; i++) {
                bool val = bit_lib_get_bit(buffer + i + 1, i);
                bit_lib_set_bit(&tx_parity, i, val);
            }

            for(size_t i = 0; i < len_without_parity; i++) {
                buffer[i] = (buffer[i] << i) | (buffer[i + 1] >> (8 - i));
            }
            bit_buffer_append_bytes(tx_buffer, buffer, len_without_parity);

            for(size_t i = 0; i < len_without_parity; i++) {
                bit_lib_reverse_bits(buffer + i, 0, 8);
                bit_buffer_set_byte_with_parity(
                    tx_buffer, i, buffer[i], bit_lib_get_bit(&tx_parity, i));
            }

            MfClassicError error = mf_classic_poller_send_custom_parity_frame(
                ctx->mfc_poller, tx_buffer, rx_buffer, 60000);
            if(error != MfClassicErrorNone) {
                if(error == MfClassicErrorTimeout &&
                   ctx->api->get_credential_type(ctx->host_ctx) ==
                       SeaderCredentialTypeMifareClassic) {
                    plugin_hf_set_read_error(
                        ctx, "Protected read timed out.\nNo supported data\nor wrong key.");
                }
                FURI_LOG_W(TAG, "mf_classic_poller_send_custom_parity_frame error %d", error);
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                break;
            }

            size_t length = bit_buffer_get_size_bytes(rx_buffer);
            const uint8_t* rx_parity = bit_buffer_get_parity(rx_buffer);
            uint8_t with_parity[HF_PLUGIN_POLLER_MAX_BUFFER_SIZE];
            memset(with_parity, 0, sizeof(with_parity));

            for(size_t i = 0; i < length; i++) {
                uint8_t b = bit_buffer_get_byte(rx_buffer, i);
                bit_lib_reverse_bits(&b, 0, 8);
                bit_buffer_set_byte(rx_buffer, i, b);
            }

            length = length + (length / 8) + 1;
            uint8_t parts = 1 + length / 9;
            for(size_t p = 0; p < parts; p++) {
                uint8_t doffset = p * 9;
                uint8_t soffset = p * 8;

                for(size_t i = 0; i < 9; i++) {
                    with_parity[i + doffset] = bit_buffer_get_byte(rx_buffer, i + soffset) >> i;
                    if(i > 0) {
                        with_parity[i + doffset] |= bit_buffer_get_byte(rx_buffer, i + soffset - 1)
                                                    << (9 - i);
                    }
                    if(i > 0) {
                        bool val = bit_lib_get_bit(rx_parity, i - 1);
                        bit_lib_set_bit(with_parity + i, i - 1, val);
                    }
                }
            }

            for(size_t i = 0; i < length; i++) {
                bit_lib_reverse_bits(with_parity + i, 0, 8);
            }

            bit_buffer_copy_bytes(rx_buffer, with_parity, length);
        } else {
            FURI_LOG_W(TAG, "Unhandled MFC format");
        }

        ctx->api->send_nfc_rx(
            ctx->host_ctx,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));
    } while(false);

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

static NfcCommand plugin_hf_poller_callback_iso14443_4a(NfcGenericEvent event, void* context) {
    PluginHfContext* ctx = plugin_hf_require_ctx(context);
    NfcCommand ret = NfcCommandContinue;
    const Iso14443_4aPollerEvent* iso_event = event.event_data;
    if(event.protocol != NfcProtocolIso14443_4a || !iso_event) {
        FURI_LOG_W(TAG, "14A callback invalid event");
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
        return NfcCommandStop;
    }
    PluginHfStage stage = ctx->api->get_stage(ctx->host_ctx);
    ctx->iso14443_4a_poller = event.instance;

    if(iso_event->type == Iso14443_4aPollerEventTypeReady) {
        HF_DIAG_D("14A ready stage=%d", stage);
        if(stage == PluginHfStageCardDetect) {
            ctx->api->notify_card_detected(ctx->host_ctx);
            if(!ctx->api->sam_can_accept_card(ctx->host_ctx)) {
                return NfcCommandContinue;
            }

            furi_check(ctx->poller);
            furi_check(ctx->nfc_device);
            const void* poller_data = nfc_poller_get_data(ctx->poller);
            if(!poller_data) {
                FURI_LOG_E(TAG, "14A ready without poller data");
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                return NfcCommandStop;
            }
            nfc_device_set_data(ctx->nfc_device, NfcProtocolIso14443_4a, poller_data);

            size_t uid_len = 0;
            const uint8_t* uid = nfc_device_get_uid(ctx->nfc_device, &uid_len);
            const Iso14443_4aData* iso_data =
                nfc_device_get_data(ctx->nfc_device, NfcProtocolIso14443_4a);
            if(!uid || !iso_data) {
                FURI_LOG_E(TAG, "14A data unavailable uid=%p iso=%p", (void*)uid, (void*)iso_data);
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                return NfcCommandStop;
            }
            const Iso14443_3aData* iso3a = iso14443_4a_get_base_data(iso_data);
            if(!iso3a) {
                FURI_LOG_E(TAG, "14A base data unavailable");
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                return NfcCommandStop;
            }

            uint32_t t1_tk_size = 0;
            if(iso_data->ats_data.t1_tk != NULL) {
                t1_tk_size = simple_array_get_count(iso_data->ats_data.t1_tk);
                if(t1_tk_size > 0xFF) {
                    t1_tk_size = 0;
                }
            }

            uint8_t ats_len = 0;
            uint8_t ats[HF_PLUGIN_MAX_ATS_SIZE] = {0};
            if(iso_data->ats_data.tl > 1) {
                if(sizeof(ats) < 4U + t1_tk_size) {
                    FURI_LOG_E(TAG, "ATS buffer too small: %u", (unsigned)(4U + t1_tk_size));
                    ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                    return NfcCommandStop;
                }
                ats[ats_len++] = iso_data->ats_data.t0;
                if(iso_data->ats_data.t0 & ISO14443_4A_ATS_T0_TA1)
                    ats[ats_len++] = iso_data->ats_data.ta_1;
                if(iso_data->ats_data.t0 & ISO14443_4A_ATS_T0_TB1)
                    ats[ats_len++] = iso_data->ats_data.tb_1;
                if(iso_data->ats_data.t0 & ISO14443_4A_ATS_T0_TC1)
                    ats[ats_len++] = iso_data->ats_data.tc_1;
                if(t1_tk_size != 0) {
                    memcpy(
                        ats + ats_len,
                        simple_array_cget_data(iso_data->ats_data.t1_tk),
                        t1_tk_size);
                    ats_len += t1_tk_size;
                }
            }

            ctx->api->send_card_detected(
                ctx->host_ctx, iso14443_3a_get_sak(iso3a), uid, uid_len, ats, ats_len);
            FURI_LOG_D(TAG, "14A cardDetected delivered uid_len=%u ats_len=%u", uid_len, ats_len);
            ctx->api->set_stage(ctx->host_ctx, PluginHfStageConversation);
        } else if(stage == PluginHfStageConversation) {
            FURI_LOG_D(TAG, "14A enter conversation");
            ctx->api->run_conversation(ctx->host_ctx);
            stage = ctx->api->get_stage(ctx->host_ctx);
            if(stage == PluginHfStageComplete) {
                ret = NfcCommandStop;
            } else if(stage == PluginHfStageFail) {
                ctx->api->notify_worker_exit(ctx->host_ctx);
                ret = NfcCommandStop;
            }
        } else if(stage == PluginHfStageComplete) {
            ret = NfcCommandStop;
        } else if(stage == PluginHfStageFail) {
            ctx->api->notify_worker_exit(ctx->host_ctx);
            ret = NfcCommandStop;
        }
    } else if(iso_event->type == Iso14443_4aPollerEventTypeError) {
        Iso14443_4aPollerEventData* data = iso_event->data;
        if(data->error == Iso14443_4aErrorProtocol) {
            ret = NfcCommandStop;
        }
    }

    return ret;
}

static NfcCommand plugin_hf_poller_callback_mfc(NfcGenericEvent event, void* context) {
    PluginHfContext* ctx = plugin_hf_require_ctx(context);
    NfcCommand ret = NfcCommandContinue;
    MfClassicPollerEvent* mfc_event = event.event_data;
    if(event.protocol != NfcProtocolMfClassic || !mfc_event) {
        FURI_LOG_W(TAG, "MFC callback invalid event");
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
        return NfcCommandStop;
    }
    PluginHfStage stage = ctx->api->get_stage(ctx->host_ctx);
    ctx->mfc_poller = event.instance;

    if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        HF_DIAG_D("MFC success stage=%d", stage);
        if(stage == PluginHfStageCardDetect) {
            ctx->api->notify_card_detected(ctx->host_ctx);
            if(!ctx->api->sam_can_accept_card(ctx->host_ctx)) {
                return NfcCommandContinue;
            }

            furi_check(ctx->poller);
            const MfClassicData* mfc_data = nfc_poller_get_data(ctx->poller);
            if(!mfc_data || !mfc_data->iso14443_3a_data) {
                FURI_LOG_E(TAG, "MFC data unavailable");
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                return NfcCommandStop;
            }
            size_t uid_len = 0;
            const uint8_t* uid = mf_classic_get_uid(mfc_data, &uid_len);
            if(!uid) {
                FURI_LOG_E(TAG, "MFC uid unavailable");
                ctx->api->set_stage(ctx->host_ctx, PluginHfStageFail);
                return NfcCommandStop;
            }
            ctx->api->send_card_detected(
                ctx->host_ctx,
                iso14443_3a_get_sak(mfc_data->iso14443_3a_data),
                uid,
                uid_len,
                NULL,
                0);
            FURI_LOG_D(TAG, "MFC cardDetected delivered uid_len=%u", uid_len);
            ctx->api->set_stage(ctx->host_ctx, PluginHfStageConversation);
        } else if(stage == PluginHfStageConversation) {
            FURI_LOG_D(TAG, "MFC enter conversation");
            ctx->api->run_conversation(ctx->host_ctx);
            stage = ctx->api->get_stage(ctx->host_ctx);
            if(stage == PluginHfStageComplete) {
                ret = NfcCommandStop;
            } else if(stage == PluginHfStageFail) {
                ctx->api->notify_worker_exit(ctx->host_ctx);
                ret = NfcCommandStop;
            }
        } else if(stage == PluginHfStageComplete) {
            ret = NfcCommandStop;
        } else if(stage == PluginHfStageFail) {
            ctx->api->notify_worker_exit(ctx->host_ctx);
            ret = NfcCommandStop;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        ctx->api->notify_worker_exit(ctx->host_ctx);
        ret = NfcCommandStop;
    }

    return ret;
}

static NfcCommand plugin_hf_poller_callback_picopass(PicopassPollerEvent event, void* context) {
    PluginHfContext* ctx = plugin_hf_require_ctx(context);
    NfcCommand ret = NfcCommandContinue;
    PluginHfStage stage = ctx->api->get_stage(ctx->host_ctx);

    if(event.type == PicopassPollerEventTypeCardDetected) {
        HF_DIAG_D("Picopass card detected");
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageCardDetect);
    } else if(event.type == PicopassPollerEventTypeSuccess) {
        HF_DIAG_D("Picopass success stage=%d", stage);
        if(stage == PluginHfStageCardDetect) {
            ctx->api->notify_card_detected(ctx->host_ctx);
            if(!ctx->api->sam_can_accept_card(ctx->host_ctx)) {
                return NfcCommandContinue;
            }
            uint8_t* csn = ctx->api->picopass_get_csn(ctx->host_ctx);
            furi_check(csn);
            ctx->api->send_card_detected(
                ctx->host_ctx, 0, csn, sizeof(PicopassSerialNum), NULL, 0);
            FURI_LOG_D(TAG, "Picopass cardDetected delivered");
            ctx->api->set_stage(ctx->host_ctx, PluginHfStageConversation);
        } else if(stage == PluginHfStageConversation) {
            FURI_LOG_D(TAG, "Picopass enter conversation");
            ctx->api->run_conversation(ctx->host_ctx);
            stage = ctx->api->get_stage(ctx->host_ctx);
            if(stage == PluginHfStageComplete) {
                ret = NfcCommandStop;
            } else if(stage == PluginHfStageFail) {
                ctx->api->notify_worker_exit(ctx->host_ctx);
                ret = NfcCommandStop;
            }
        } else if(stage == PluginHfStageComplete) {
            ret = NfcCommandStop;
        } else if(stage == PluginHfStageFail) {
            ctx->api->notify_worker_exit(ctx->host_ctx);
            ret = NfcCommandStop;
        }
    } else if(event.type == PicopassPollerEventTypeFail) {
        ret = NfcCommandStop;
    }

    return ret;
}

static void* plugin_hf_alloc(const PluginHfHostApi* api, void* host_ctx) {
    PluginHfContext* ctx = calloc(1, sizeof(PluginHfContext));
    if(!ctx) {
        FURI_LOG_E(TAG, "Failed to allocate plugin context");
        return NULL;
    }
    if(!host_ctx) {
        FURI_LOG_E(TAG, "Missing HF host context");
        free(ctx);
        return NULL;
    }
    if(!plugin_hf_validate_host_api(api)) {
        free(ctx);
        return NULL;
    }
    ctx->api = api;
    ctx->host_ctx = host_ctx;
    ctx->nfc = api->get_nfc ? api->get_nfc(host_ctx) : NULL;
    ctx->nfc_device = api->get_nfc_device ? api->get_nfc_device(host_ctx) : NULL;
    if(!ctx->nfc || !ctx->nfc_device) {
        FURI_LOG_E(
            TAG,
            "Host NFC objects unavailable nfc=%p device=%p",
            (void*)ctx->nfc,
            (void*)ctx->nfc_device);
        free(ctx);
        return NULL;
    }
    return ctx;
}

static void plugin_hf_free(void* plugin_ctx) {
    PluginHfContext* ctx = plugin_hf_require_ctx(plugin_ctx);
    plugin_hf_cleanup_pollers(ctx);
    free(ctx);
}

static size_t plugin_hf_detect_supported_types(
    void* plugin_ctx,
    SeaderCredentialType* detected_types,
    size_t detected_capacity) {
    PluginHfContext* ctx = plugin_hf_require_ctx(plugin_ctx);
    furi_check(detected_types);
    furi_check(detected_capacity > 0U);
    size_t detected_type_count = 0;
    HF_DIAG_D("Detect supported HF types");
    NfcPoller* poller_detect = nfc_poller_alloc(ctx->nfc, NfcProtocolIso14443_4a);
    if(!poller_detect) {
        FURI_LOG_W(TAG, "Failed to allocate 14A detect poller");
    } else if(nfc_poller_detect(poller_detect)) {
        plugin_hf_add_detected_type(
            detected_types, &detected_type_count, detected_capacity, SeaderCredentialType14A);
    }
    if(poller_detect) nfc_poller_free(poller_detect);

    poller_detect = nfc_poller_alloc(ctx->nfc, NfcProtocolMfClassic);
    if(!poller_detect) {
        FURI_LOG_W(TAG, "Failed to allocate MFC detect poller");
    } else if(nfc_poller_detect(poller_detect)) {
        plugin_hf_add_detected_type(
            detected_types,
            &detected_type_count,
            detected_capacity,
            SeaderCredentialTypeMifareClassic);
    }
    if(poller_detect) nfc_poller_free(poller_detect);

    if(ctx->api->picopass_detect && ctx->api->picopass_detect(ctx->host_ctx)) {
        plugin_hf_add_detected_type(
            detected_types, &detected_type_count, detected_capacity, SeaderCredentialTypePicopass);
    }

    return detected_type_count;
}

static bool plugin_hf_start_read_for_type(void* plugin_ctx, SeaderCredentialType type) {
    PluginHfContext* ctx = plugin_hf_require_ctx(plugin_ctx);
    NfcPoller* poller_detect = NULL;

    plugin_hf_cleanup_pollers(ctx);
    ctx->active_type = type;
    HF_DIAG_I("Start read type=%d", type);

    if(type == SeaderCredentialType14A) {
        poller_detect = nfc_poller_alloc(ctx->nfc, NfcProtocolIso14443_4a);
        if(!poller_detect) {
            FURI_LOG_E(TAG, "Failed to allocate 14A detect poller");
            return false;
        }
        if(!nfc_poller_detect(poller_detect)) {
            nfc_poller_free(poller_detect);
            return false;
        }
        nfc_poller_free(poller_detect);
        ctx->poller = nfc_poller_alloc(ctx->nfc, NfcProtocolIso14443_4a);
        if(!ctx->poller) {
            FURI_LOG_E(TAG, "Failed to allocate 14A poller");
            return false;
        }
        ctx->api->set_credential_type(ctx->host_ctx, SeaderCredentialType14A);
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageCardDetect);
        nfc_poller_start(ctx->poller, plugin_hf_poller_callback_iso14443_4a, ctx);
        return true;
    } else if(type == SeaderCredentialTypeMifareClassic) {
        poller_detect = nfc_poller_alloc(ctx->nfc, NfcProtocolMfClassic);
        if(!poller_detect) {
            FURI_LOG_E(TAG, "Failed to allocate MFC detect poller");
            return false;
        }
        if(!nfc_poller_detect(poller_detect)) {
            nfc_poller_free(poller_detect);
            return false;
        }
        nfc_poller_free(poller_detect);
        ctx->poller = nfc_poller_alloc(ctx->nfc, NfcProtocolMfClassic);
        if(!ctx->poller) {
            FURI_LOG_E(TAG, "Failed to allocate MFC poller");
            return false;
        }
        ctx->api->set_credential_type(ctx->host_ctx, SeaderCredentialTypeMifareClassic);
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageCardDetect);
        nfc_poller_start(ctx->poller, plugin_hf_poller_callback_mfc, ctx);
        return true;
    } else if(type == SeaderCredentialTypePicopass) {
        if(!ctx->api->picopass_detect || !ctx->api->picopass_detect(ctx->host_ctx)) {
            return false;
        }
        ctx->api->set_credential_type(ctx->host_ctx, SeaderCredentialTypePicopass);
        ctx->api->set_stage(ctx->host_ctx, PluginHfStageCardDetect);
        return ctx->api->picopass_start &&
               ctx->api->picopass_start(ctx->host_ctx, plugin_hf_poller_callback_picopass, ctx);
    }

    return false;
}

static void plugin_hf_stop(void* plugin_ctx) {
    PluginHfContext* ctx = plugin_hf_require_ctx(plugin_ctx);
    plugin_hf_cleanup_pollers(ctx);
    ctx->active_type = SeaderCredentialTypeNone;
}

static bool plugin_hf_handle_action(void* plugin_ctx, const PluginHfAction* action) {
    PluginHfContext* ctx = plugin_hf_require_ctx(plugin_ctx);
    furi_check(action);
    HF_DIAG_D("Handle action type=%d len=%u", action->type, action->len);

    if(action->type == PluginHfActionTypePicopassTx) {
        if(ctx->active_type != SeaderCredentialTypePicopass) return false;
        plugin_hf_iso15693_transmit(ctx, action->data, action->len);
        return true;
    } else if(action->type == PluginHfActionTypeMfClassicTx) {
        if(!ctx->poller) return false;
        plugin_hf_mfc_transmit(
            ctx, action->data, action->len, action->timeout, (uint8_t*)action->format);
        return true;
    } else if(action->type == PluginHfActionTypeIso14443Tx) {
        if(!ctx->poller) return false;
        plugin_hf_iso14443a_transmit(
            ctx, action->data, action->len, action->timeout, (uint8_t*)action->format);
        return true;
    }

    FURI_LOG_W(TAG, "Unhandled HF action %d", action->type);
    return false;
}

static const PluginHf plugin_hf = {
    .name = "Plugin HF",
    .alloc = plugin_hf_alloc,
    .free = plugin_hf_free,
    .detect_supported_types = plugin_hf_detect_supported_types,
    .start_read_for_type = plugin_hf_start_read_for_type,
    .stop = plugin_hf_stop,
    .handle_action = plugin_hf_handle_action,
};

static const FlipperAppPluginDescriptor plugin_hf_descriptor = {
    .appid = HF_PLUGIN_APP_ID,
    .ep_api_version = HF_PLUGIN_API_VERSION,
    .entry_point = &plugin_hf,
};

const FlipperAppPluginDescriptor* plugin_hf_ep(void) {
    return &plugin_hf_descriptor;
}

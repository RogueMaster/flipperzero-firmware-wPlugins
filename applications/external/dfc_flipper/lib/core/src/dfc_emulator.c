#include "dfc_emulator_i.h"

DfcEmulator* dfc_emulator_alloc(DfcCredential* credential) {
    if(!credential) return NULL;

    DfcEmulator* emulator =
        dfc_platform_alloc(sizeof(DfcEmulator), DfcAllocEmulator);
    if(!emulator) return NULL;
    memset(emulator, 0, sizeof(DfcEmulator));

    emulator->credential = credential;
    emulator->tx_buffer = dfc_bytebuf_alloc(DFC_WORKER_MAX_BUFFER_SIZE);
    if(!emulator->tx_buffer) {
        dfc_platform_free(emulator);
        return NULL;
    }
    emulator->selected_application = DfcEmulatorSelectedApplicationPicc;

    return emulator;
}

void dfc_emulator_free(DfcEmulator* emulator) {
    DFC_ASSERT(emulator);

    if(emulator->secure_messaging) {
        dfc_secure_messaging_free(emulator->secure_messaging);
    }

    if(emulator->tx_buffer) {
        dfc_bytebuf_free(emulator->tx_buffer);
    }
    dfc_platform_free(emulator);
}

DfcApplication* dfc_emulator_current_app(DfcEmulator* emulator) {
    if(emulator->selected_application != DfcEmulatorSelectedApplicationApp) return NULL;
    return dfc_credential_get_application(emulator->credential, emulator->selected_app_index);
}

// Which Authenticate variants this level answers. This is the engine's only
// decision point about acceptable authentication modes, and so the only place a
// secure-messaging disable flag could take effect.
//
// credential->picc_sm_disable is deliberately not consulted. The format carries
// the octet and both codecs preserve it, but section 1.1 assigns it no bit
// layout, so there is no defined mapping from any of its bits to a cipher this
// function could subtract. Acting on a guessed layout would refuse
// authentications a real card accepts, which is worse than carrying the octet
// inert. It is therefore stored, round-tripped and reported, and takes effect
// nowhere until the format defines what the bits mean.
bool dfc_emulator_accepts_auth_cipher(DfcEmulator* emulator, uint8_t cipher) {
    uint8_t key_settings_2;
    DfcApplication* app = dfc_emulator_current_app(emulator);
    if(app) {
        key_settings_2 = app->key_settings_2;
    } else {
        key_settings_2 = emulator->credential->picc_key_settings_2;
    }

    switch(key_settings_2 & DFC_KEY_TYPE_MASK) {
    case DFC_KEY_TYPE_AES:
        return cipher == DFC_CMD_AUTHENTICATE_AES;
    case DFC_KEY_TYPE_3K3DES:
        return cipher == DFC_CMD_AUTHENTICATE_ISO;
    case DFC_KEY_TYPE_DES_2K3DES:
    default:
        // Single DES / 2K3DES accept both legacy D40 and ISO mutual auth.
        return cipher == DFC_CMD_AUTHENTICATE_LEGACY || cipher == DFC_CMD_AUTHENTICATE_ISO;
    }
}

size_t dfc_emulator_key_len(DfcEmulator* emulator) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    return app ? app->key_len : emulator->credential->picc_key_len;
}

size_t dfc_emulator_num_keys(DfcEmulator* emulator) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    // Every object has a master key whether or not the credential records one:
    // an absent entry is the factory default, not an absent slot.
    if(!app) return 1;
    return app->num_keys ? app->num_keys : 1;
}

uint8_t* dfc_emulator_key(DfcEmulator* emulator, uint8_t key_no) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    // dfc_emulator_num_keys reports 1 at PICC level, so only slot 0 is reachable.
    return dfc_credential_key(emulator->credential, app, app ? key_no : 0);
}

uint8_t* dfc_emulator_key_version(DfcEmulator* emulator, uint8_t key_no) {
    DfcApplication* app = dfc_emulator_current_app(emulator);
    return app ? &app->key_versions[key_no] : &emulator->credential->picc_key_versions[0];
}

static void dfc_emulator_clear_pending_value_transactions(DfcEmulator* emulator) {
    for(size_t i = 0; i < emulator->credential->num_files; i++) {
        DfcFile* file = &emulator->credential->files[i];
        if(file->type != DFC_FILE_TYPE_VALUE) continue;
        file->value_pending = false;
        file->value_pending_delta = 0;
    }
}

void dfc_emulator_reset_session(DfcEmulator* emulator) {
    emulator->awaiting_step2 = false;
    emulator->auth_cipher = 0;
    emulator->auth_key_no = 0;
    emulator->get_version_frame = 0;
    emulator->pending_chain_len = 0;
    emulator->pending_chain_offset = 0;
#if DFC_ENABLE_DELEGATED_APPLICATIONS
    emulator->delegated_creation_pending = false;
    emulator->delegated_creation_header_length = 0;
    memset(
        emulator->delegated_creation_header,
        0,
        sizeof(emulator->delegated_creation_header));
#endif
#if DFC_ENABLE_PROXIMITY_CHECK
    emulator->proximity_active = false;
    emulator->proximity_verified = false;
    emulator->proximity_offset = 0;
    emulator->proximity_published_len = 0;
    emulator->proximity_transcript_len = 0;
    memset(emulator->proximity_random, 0, sizeof(emulator->proximity_random));
    memset(emulator->proximity_published, 0, sizeof(emulator->proximity_published));
    memset(emulator->proximity_transcript, 0, sizeof(emulator->proximity_transcript));
#endif
#if DFC_ENABLE_EV2_SECURE_MESSAGING
    emulator->ev2_authentication_pending = false;
    emulator->ev2_authentication_non_first = false;
    emulator->ev2_session_active = false;
    emulator->ev2_authenticated_key_no = 0;
    emulator->ev2_command_counter = 0;
    memset(emulator->ev2_static_key, 0, sizeof(emulator->ev2_static_key));
    memset(emulator->ev2_random_a, 0, sizeof(emulator->ev2_random_a));
    memset(emulator->ev2_random_b, 0, sizeof(emulator->ev2_random_b));
    memset(
        emulator->ev2_transaction_identifier,
        0,
        sizeof(emulator->ev2_transaction_identifier));
    memset(emulator->ev2_card_capabilities, 0, sizeof(emulator->ev2_card_capabilities));
    memset(emulator->ev2_reader_capabilities, 0, sizeof(emulator->ev2_reader_capabilities));
    memset(
        emulator->ev2_session_encryption_key,
        0,
        sizeof(emulator->ev2_session_encryption_key));
    memset(emulator->ev2_session_mac_key, 0, sizeof(emulator->ev2_session_mac_key));
#endif
#if DFC_ENABLE_SDM
    emulator->sdm_read_cache_valid = false;
    emulator->sdm_read_cache_len = 0;
#endif
    dfc_emulator_clear_pending_value_transactions(emulator);
    memset(emulator->rnd_a, 0, sizeof(emulator->rnd_a));
    memset(emulator->rnd_b, 0, sizeof(emulator->rnd_b));
    memset(emulator->enc_rnd_b, 0, sizeof(emulator->enc_rnd_b));
    if(emulator->secure_messaging) {
        dfc_secure_messaging_free(emulator->secure_messaging);
        emulator->secure_messaging = NULL;
    }
}

void dfc_emulator_reset_activation(DfcEmulator* emulator) {
    DFC_ASSERT(emulator);

    DfcCredential* credential = emulator->credential;
    DfcByteBuf* tx_buffer = emulator->tx_buffer;
    if(emulator->secure_messaging) {
        dfc_secure_messaging_free(emulator->secure_messaging);
    }

    memset(emulator, 0, sizeof(*emulator));
    emulator->credential = credential;
    emulator->tx_buffer = tx_buffer;
    emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
    if(tx_buffer) {
        dfc_bytebuf_reset(tx_buffer);
    }
}

void dfc_emulator_advance_time(DfcEmulator* emulator, uint32_t elapsed_milliseconds) {
#if DFC_ENABLE_TRANSACTION_TIMER
    if(!emulator || !emulator->transaction_timer_enabled) return;
    if(UINT32_MAX - emulator->transaction_timer_elapsed_milliseconds < elapsed_milliseconds)
        emulator->transaction_timer_elapsed_milliseconds = UINT32_MAX;
    else
        emulator->transaction_timer_elapsed_milliseconds += elapsed_milliseconds;
    if(emulator->transaction_timer_elapsed_milliseconds >=
       DFC_TRANSACTION_TIMER_EXPIRY_MILLISECONDS) {
        dfc_emulator_reset_session(emulator);
        emulator->selected_application = DfcEmulatorSelectedApplicationPicc;
        emulator->selected_app_index = 0;
        emulator->transaction_timer_enabled = false;
    }
#else
    DFC_UNUSED(emulator);
    DFC_UNUSED(elapsed_milliseconds);
#endif
}

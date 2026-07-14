#include "tonuino_nfc.h"
#include <string.h>

bool tonuino_read_card(TonuinoApp* app) {
    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        return false;
    }

    MfClassicKey default_key = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    MfClassicBlock block;
    MfClassicError error;
    bool success = false;

    memset(&block, 0, sizeof(MfClassicBlock));
    memset(&app->card_data, 0, sizeof(TonuinoCard));

    uint8_t blocks_to_try[] = {4, 1, 5, 6};

    for(int block_idx = 0; block_idx < 4; block_idx++) {
        uint8_t block_num = blocks_to_try[block_idx];

        for(int retry = 0; retry < 20; retry++) {
            error = mf_classic_poller_sync_read_block(
                nfc, block_num, &default_key, MfClassicKeyTypeA, &block);

            if(error == MfClassicErrorNone) {
                if(block.data[0] == TONUINO_BOX_ID_0 && block.data[1] == TONUINO_BOX_ID_1 &&
                   block.data[2] == TONUINO_BOX_ID_2 && block.data[3] == TONUINO_BOX_ID_3) {
                    if(block.data[5] <= 99 && block.data[6] >= 1 &&
                       block.data[6] <= ModeRepeatLast) {
                        memcpy(&app->card_data.box_id[0], &block.data[0], 4);
                        app->card_data.version = block.data[4];
                        app->card_data.folder = block.data[5];
                        app->card_data.mode = block.data[6];
                        app->card_data.special1 = block.data[7];
                        app->card_data.special2 = block.data[8];
                        memcpy(&app->card_data.reserved[0], &block.data[9], 7);
                        success = true;
                        break;
                    }
                }
            }

            furi_delay_ms(200);
        }

        if(success) break;
    }

    nfc_free(nfc);
    return success;
}

bool tonuino_write_card(TonuinoApp* app) {
    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        return false;
    }

    uint8_t card_data[TONUINO_CARD_SIZE];

    card_data[0] = app->card_data.box_id[0];
    card_data[1] = app->card_data.box_id[1];
    card_data[2] = app->card_data.box_id[2];
    card_data[3] = app->card_data.box_id[3];
    card_data[4] = app->card_data.version;
    card_data[5] = app->card_data.folder;
    card_data[6] = app->card_data.mode;
    card_data[7] = app->card_data.special1;
    card_data[8] = app->card_data.special2;
    card_data[9] = 0x00;
    card_data[10] = 0x00;
    card_data[11] = 0x00;
    card_data[12] = 0x00;
    card_data[13] = 0x00;
    card_data[14] = 0x00;
    card_data[15] = 0x00;

    MfClassicKey default_key = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    MfClassicBlock block;
    memcpy(block.data, card_data, TONUINO_CARD_SIZE);

    MfClassicError error;
    bool success = false;

    for(int retry = 0; retry < 20; retry++) {
        error =
            mf_classic_poller_sync_write_block(nfc, 4, &default_key, MfClassicKeyTypeA, &block);

        if(error == MfClassicErrorNone) {
            success = true;
            break;
        }

        furi_delay_ms(200);
    }

    nfc_free(nfc);
    return success;
}

int32_t tonuino_read_card_worker(void* context) {
    TonuinoNfcThreadContext* ctx = context;
    TonuinoApp* app = ctx->app;

    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        ctx->success = false;
        return 0;
    }

    MfClassicKey default_key = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    MfClassicBlock block;
    MfClassicError error;
    bool success = false;

    memset(&block, 0, sizeof(MfClassicBlock));
    memset(&app->card_data, 0, sizeof(TonuinoCard));

    uint8_t blocks_to_try[] = {4, 1, 5, 6};

    for(int block_idx = 0; block_idx < 4 && !ctx->cancel_flag; block_idx++) {
        uint8_t block_num = blocks_to_try[block_idx];

        for(int retry = 0; retry < 20 && !ctx->cancel_flag; retry++) {
            error = mf_classic_poller_sync_read_block(
                nfc, block_num, &default_key, MfClassicKeyTypeA, &block);

            if(error == MfClassicErrorNone) {
                if(block.data[0] == TONUINO_BOX_ID_0 && block.data[1] == TONUINO_BOX_ID_1 &&
                   block.data[2] == TONUINO_BOX_ID_2 && block.data[3] == TONUINO_BOX_ID_3) {
                    if(block.data[5] <= 99 && block.data[6] >= 1 &&
                       block.data[6] <= ModeRepeatLast) {
                        memcpy(&app->card_data.box_id[0], &block.data[0], 4);
                        app->card_data.version = block.data[4];
                        app->card_data.folder = block.data[5];
                        app->card_data.mode = block.data[6];
                        app->card_data.special1 = block.data[7];
                        app->card_data.special2 = block.data[8];
                        memcpy(&app->card_data.reserved[0], &block.data[9], 7);
                        success = true;
                        break;
                    }
                }
            }

            if(!ctx->cancel_flag) {
                furi_delay_ms(200);
            }
        }

        if(success) break;
    }

    nfc_free(nfc);
    ctx->success = success;
    return 0;
}

int32_t tonuino_write_card_worker(void* context) {
    TonuinoNfcThreadContext* ctx = context;
    TonuinoApp* app = ctx->app;

    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        ctx->success = false;
        return 0;
    }

    uint8_t card_data[TONUINO_CARD_SIZE];

    card_data[0] = app->card_data.box_id[0];
    card_data[1] = app->card_data.box_id[1];
    card_data[2] = app->card_data.box_id[2];
    card_data[3] = app->card_data.box_id[3];
    card_data[4] = app->card_data.version;
    card_data[5] = app->card_data.folder;
    card_data[6] = app->card_data.mode;
    card_data[7] = app->card_data.special1;
    card_data[8] = app->card_data.special2;
    card_data[9] = 0x00;
    card_data[10] = 0x00;
    card_data[11] = 0x00;
    card_data[12] = 0x00;
    card_data[13] = 0x00;
    card_data[14] = 0x00;
    card_data[15] = 0x00;

    MfClassicKey default_key = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    MfClassicBlock block;
    memcpy(block.data, card_data, TONUINO_CARD_SIZE);

    MfClassicError error;
    bool success = false;

    for(int retry = 0; retry < 20 && !ctx->cancel_flag; retry++) {
        error =
            mf_classic_poller_sync_write_block(nfc, 4, &default_key, MfClassicKeyTypeA, &block);

        if(error == MfClassicErrorNone) {
            success = true;
            break;
        }

        if(!ctx->cancel_flag) {
            furi_delay_ms(200);
        }
    }

    nfc_free(nfc);
    ctx->success = success;
    return 0;
}

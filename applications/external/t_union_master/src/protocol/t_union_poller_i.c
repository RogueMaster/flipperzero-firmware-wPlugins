#include "t_union_poller_i.h"

#define T_UNION_TAG_AID         0x4F
#define T_UNION_TAG_APPL_LABEL  0x50
#define T_UNION_TAG_DF_NAME     0x84
#define T_UNION_TAG_PRIORITY    0x87
#define T_UNION_TAG_APP_VERSION 0x9F08
#define T_UNION_TAG_APP_META    0x9F0C

TUnionPollerError t_union_poller_process_error(Iso14443_4aError error) {
    switch(error) {
    case Iso14443_4aErrorNone:
        return TUnionPollerErrorNone;
    case Iso14443_4aErrorNotPresent:
        return TUnionPollerErrorNotPresent;
    case Iso14443_4aErrorTimeout:
        return TUnionPollerErrorTimeout;
    default:
        return TUnionPollerErrorProtocol;
    }
}

static void t_union_trace(TUnionPoller* instance, const char* message) {
    if(furi_log_get_level() == FuriLogLevelTrace) {
        FURI_LOG_T(TAG, "%s", message);

        printf("TX: ");
        size_t size = bit_buffer_get_size_bytes(instance->tx_buffer);
        for(size_t i = 0; i < size; i++) {
            printf("%02X ", bit_buffer_get_byte(instance->tx_buffer, i));
        }

        printf("\r\nRX: ");
        size = bit_buffer_get_size_bytes(instance->rx_buffer);
        for(size_t i = 0; i < size; i++) {
            printf("%02X ", bit_buffer_get_byte(instance->rx_buffer, i));
        }
        printf("\r\n");
    }
}

uint64_t bytes_to_num_bcd(const uint8_t* src, uint8_t len) {
    furi_assert(src);
    furi_assert(len <= 9);

    uint64_t res = 0;
    uint8_t nibble_1, nibble_2;

    for(uint8_t i = 0; i < len; i++) {
        nibble_1 = src[i] / 16;
        nibble_2 = src[i] % 16;

        res *= 10;
        res += nibble_1;

        res *= 10;
        res += nibble_2;
    }

    return res;
}

uint64_t bytes_to_num_be(const uint8_t* src, uint8_t len) {
    furi_assert(src);
    furi_assert(len <= 8);

    uint64_t res = 0;
    while(len--) {
        res = (res << 8) | (*src);
        src++;
    }
    return res;
}

void bytes_to_str_b16(const uint8_t* src, char* dist, uint8_t len) {
    furi_assert(src);
    uint8_t temp;

    for(uint8_t i = 0; i < len; i++) {
        temp = (src[i] >> 4) & 0x0F;
        dist[i * 2] = (temp < 0xA ? '0' : 'A' - 0xA) + temp;
        temp = src[i] & 0x0F;
        dist[i * 2 + 1] = (temp < 0xA ? '0' : 'A' - 0xA) + temp;
    }
    dist[len * 2] = '\0';
}

static void t_union_decode_app_meta(TUnionMessage* msg, const uint8_t* buff) {
    // 卡类型
    msg->type = bytes_to_num_bcd(&buff[0x00], sizeof(uint8_t));
    // 发卡地
    bytes_to_str_b16(&buff[0x02], msg->city_id, 2);
    // 卡号
    bytes_to_str_b16(&buff[0x0A], msg->card_number, 10);
    for(uint8_t i = 0; i < 20; i++)
        msg->card_number[i] = msg->card_number[i + 1];

    // 签发日期
    msg->iss_year = bytes_to_num_bcd(&buff[0x14 + 0], sizeof(msg->iss_year));
    msg->iss_month = bytes_to_num_bcd(&buff[0x14 + 2], sizeof(msg->iss_month));
    msg->iss_day = bytes_to_num_bcd(&buff[0x14 + 3], sizeof(msg->iss_day));
    // 到期日期
    msg->exp_year = bytes_to_num_bcd(&buff[0x18 + 0], sizeof(msg->exp_year));
    msg->exp_month = bytes_to_num_bcd(&buff[0x18 + 2], sizeof(msg->exp_month));
    msg->exp_day = bytes_to_num_bcd(&buff[0x18 + 3], sizeof(msg->exp_day));
}

static void t_union_decode_balance(TUnionMessage* msg, const uint8_t* buff) {
    // 余额
    msg->balance = bytes_to_num_be(&buff[0x00], sizeof(msg->balance));
}

static void
    t_union_decode_transaction(TUnionMessage* msg, const uint8_t* buff, uint8_t record_num) {
    TUnionTransaction* transaction = &msg->transactions[record_num];

    // 交易序号
    transaction->seqense = bytes_to_num_be(&buff[0x00], sizeof(transaction->seqense));
    // 交易金额
    transaction->money = bytes_to_num_be(&buff[0x05], sizeof(transaction->money));
    // 交易类型
    transaction->type = bytes_to_num_be(&buff[0x09], sizeof(transaction->type));
    // 交易终端id
    bytes_to_str_b16(&buff[0x0A], transaction->terminal_id, 6);
    // 交易时间
    transaction->year = bytes_to_num_bcd(&buff[0x10 + 0], sizeof(transaction->year));
    transaction->month = bytes_to_num_bcd(&buff[0x10 + 2], sizeof(transaction->month));
    transaction->day = bytes_to_num_bcd(&buff[0x10 + 3], sizeof(transaction->day));
    transaction->hour = bytes_to_num_bcd(&buff[0x10 + 4], sizeof(transaction->hour));
    transaction->minute = bytes_to_num_bcd(&buff[0x10 + 5], sizeof(transaction->minute));
    transaction->second = bytes_to_num_bcd(&buff[0x10 + 6], sizeof(transaction->second));

    if(transaction->type) msg->transaction_cnt++;
}

static void t_union_decode_travel(TUnionMessage* msg, const uint8_t* buff, uint8_t record_num) {
    TUnionTravel* travel = &msg->travels[record_num];

    // 交易类型
    travel->type = bytes_to_num_be(&buff[0x00], sizeof(travel->type));
    // 交易终端id
    bytes_to_str_b16(&buff[0x01], travel->terminal_id, 8);
    // 交易子类型
    travel->sub_type = bytes_to_num_be(&buff[0x09], sizeof(travel->sub_type));
    // 站台id
    bytes_to_str_b16(&buff[0x0A], travel->station_id, 7);
    // 交易金额
    travel->money = bytes_to_num_be(&buff[0x11], sizeof(travel->money));
    // 交易后余额
    travel->balance = bytes_to_num_be(&buff[0x15], sizeof(travel->balance));
    // 交易时间
    travel->year = bytes_to_num_bcd(&buff[0x19 + 0], sizeof(travel->year));
    travel->month = bytes_to_num_bcd(&buff[0x19 + 2], sizeof(travel->month));
    travel->day = bytes_to_num_bcd(&buff[0x19 + 3], sizeof(travel->day));
    travel->hour = bytes_to_num_bcd(&buff[0x19 + 4], sizeof(travel->hour));
    travel->minute = bytes_to_num_bcd(&buff[0x19 + 5], sizeof(travel->minute));
    travel->second = bytes_to_num_bcd(&buff[0x19 + 6], sizeof(travel->second));
    // 城市id
    bytes_to_str_b16(&buff[0x20], travel->city_id, 2);
    // 机构id
    bytes_to_str_b16(&buff[0x22], travel->institution_id, 8);

    if(travel->type) msg->travel_cnt++;
}

static bool t_union_response_error(const uint8_t* buff, uint16_t len) {
    uint8_t i = 0;
    uint8_t first_byte = buff[i];

    if((len == 2) && ((first_byte >> 4) == 6)) {
        FURI_LOG_T(TAG, " Error/warning code: %02X %02X", buff[i], buff[i + 1]);
        return true;
    }
    return false;
}

static bool
    t_union_parse_tag(const uint8_t* buff, uint16_t len, uint16_t* t, uint8_t* tl, uint8_t* off) {
    uint8_t i = *off;
    uint16_t tag = 0;
    uint8_t first_byte = 0;
    uint8_t tlen = 0;
    bool success = false;

    first_byte = buff[i];

    if(t_union_response_error(buff, len)) return success;

    if((first_byte & 31) == 31) { // 2-byte tag
        tag = buff[i] << 8 | buff[i + 1];
        i++;
        FURI_LOG_T(TAG, " 2-byte TLV EMV tag: %x", tag);
    } else {
        tag = buff[i];
        FURI_LOG_T(TAG, " 1-byte TLV EMV tag: %x", tag);
    }
    i++;
    tlen = buff[i];
    if((tlen & 128) == 128) { // long length value
        i++;
        tlen = buff[i];
        FURI_LOG_T(TAG, " 2-byte TLV length: %d", tlen);
    } else {
        FURI_LOG_T(TAG, " 1-byte TLV length: %d", tlen);
    }
    i++;

    *off = i;
    *t = tag;
    *tl = tlen;
    success = true;
    return success;
}

static bool t_union_decode_tlv_tag(
    const uint8_t* buff,
    uint16_t tag,
    uint8_t tlen,
    TunionApplication* application) {
    bool success = false;

    switch(tag) {
    case T_UNION_TAG_AID:
        application->aid_len = tlen;
        memcpy(application->aid, buff, tlen);
        success = true;
        FURI_LOG_T(TAG, "found TAG_AID %X: ", tag);
        for(size_t x = 0; x < tlen; x++) {
            FURI_LOG_RAW_T("%02X ", application->aid[x]);
        }
        FURI_LOG_RAW_T("\r\n");
        break;
    case T_UNION_TAG_APPL_LABEL:
        memcpy(application->appl, buff, tlen);
        application->appl[tlen] = '\0';
        success = true;
        FURI_LOG_T(TAG, "found TAG_APPL_LABEL %x: %s", tag, application->appl);
        break;
    case T_UNION_TAG_DF_NAME:
        success = true;
        FURI_LOG_T(TAG, "found TAG_DF_NAME %X", tag);
        break;
    case T_UNION_TAG_PRIORITY:
        memcpy(&application->priority, buff, tlen);
        success = true;
        FURI_LOG_T(TAG, "found TAG_APP_PRIORITY %X: %d", tag, application->priority);
        break;
    case T_UNION_TAG_APP_VERSION:
        memcpy(&application->message->app_version, buff, tlen);
        success = true;
        FURI_LOG_T(TAG, "found TAG_APP_VERSION %X: %d", tag, application->message->app_version);
        break;
    case T_UNION_TAG_APP_META:
        success = true;
        t_union_decode_app_meta(application->message, buff);
        FURI_LOG_T(TAG, "found TAG_APP_META %X:", tag);
        for(size_t x = 0; x < tlen; x++) {
            FURI_LOG_RAW_T("%02X ", buff[x]);
        }
        FURI_LOG_RAW_T("\r\n");
        break;
    }
    return success;
}

static bool
    t_union_decode_response_tlv(const uint8_t* buff, uint8_t len, TunionApplication* application) {
    uint8_t i = 0;
    uint16_t tag = 0;
    uint8_t first_byte = 0;
    uint8_t tlen = 0;
    bool success = false;

    while(i < len) {
        first_byte = buff[i];

        success = t_union_parse_tag(buff, len, &tag, &tlen, &i);
        if(!success) return success;

        if((first_byte & 32) == 32) { // "Constructed" -- contains more TLV data to parse
            FURI_LOG_T(TAG, "Constructed TLV %x", tag);
            if(!t_union_decode_response_tlv(&buff[i], tlen, application)) {
                FURI_LOG_T(TAG, "Failed to decode response for %x", tag);
                // return false;
            } else {
                success = true;
            }
        } else {
            t_union_decode_tlv_tag(&buff[i], tag, tlen, application);
        }
        i += tlen;
    }
    return success;
}

TUnionPollerError
    t_union_poller_sesect_application(TUnionPoller* instance, const uint8_t* aid, uint8_t aid_len) {
    TUnionPollerError err = TUnionPollerErrorNone;

    const uint8_t send_packet[] = {
        0x00,
        0xA4, // SELECT application
        0x04,
        0x00 // P1:By name, P2:First or only occurence
    };

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    bit_buffer_copy_bytes(instance->tx_buffer, send_packet, sizeof(send_packet));

    bit_buffer_append_byte(instance->tx_buffer, aid_len);
    bit_buffer_append_bytes(instance->tx_buffer, aid, aid_len);
    bit_buffer_append_byte(instance->tx_buffer, 0x00);

    do {
        FURI_LOG_T(TAG, "Send Select Cmd");

        Iso14443_4aError iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
            instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);

        t_union_trace(instance, "Cmd packet:");

        if(iso14443_4a_error != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "Failed to read PAN or PDOL");
            err = t_union_poller_process_error(iso14443_4a_error);
            break;
        }

        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);
        size_t buff_len = bit_buffer_get_size_bytes(instance->rx_buffer);

        if(!t_union_decode_response_tlv(buff, buff_len, &instance->application)) {
            err = TUnionPollerErrorProtocol;
            FURI_LOG_E(TAG, "Failed to parse application");
            break;
        }

    } while(false);

    return err;
}

// 读取余额
TUnionPollerError t_union_poller_read_balance(TUnionPoller* instance) {
    TUnionPollerError err = TUnionPollerErrorNone;

    const uint8_t send_packet[] = {
        0x80, // CLA INS
        0x5C,
        0x00, // P1 P2
        0x02,
        0x04 //Le
    };

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    bit_buffer_copy_bytes(instance->tx_buffer, send_packet, sizeof(send_packet));

    do {
        FURI_LOG_T(TAG, "Send Read Balance Cmd");

        Iso14443_4aError iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
            instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);

        t_union_trace(instance, "Cmd packet:");

        if(iso14443_4a_error != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "Failed to read PAN or PDOL");
            err = t_union_poller_process_error(iso14443_4a_error);
            break;
        }

        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);
        size_t buff_len = bit_buffer_get_size_bytes(instance->rx_buffer);

        if(t_union_response_error(buff, buff_len) || buff_len != T_UNION_BALANCE_RESP_PACKET_LEN) {
            err = TUnionPollerErrorProtocol;
            FURI_LOG_E(TAG, "Failed to Read Balance");
            break;
        }
        t_union_decode_balance(instance->application.message, buff);

    } while(false);

    return err;
}

// 读SFI文件记录
TUnionPollerError
    t_union_poller_read_sfi_record(TUnionPoller* instance, uint8_t sfi, uint8_t record_num) {
    TUnionPollerError err = TUnionPollerErrorNone;

    uint8_t sfi_param = (sfi << 3) | (1 << 2);
    const uint8_t send_packet[] = {
        0x00, // CLA INS
        0xB2,
        record_num, // P1 P2
        sfi_param,
        0x00 //Le
    };

    bit_buffer_reset(instance->tx_buffer);
    bit_buffer_reset(instance->rx_buffer);

    bit_buffer_copy_bytes(instance->tx_buffer, send_packet, sizeof(send_packet));

    do {
        FURI_LOG_T(TAG, "Read SFI=%02X record %d", sfi, record_num);

        Iso14443_4aError iso14443_4a_error = iso14443_4a_poller_send_block_pwt_ext(
            instance->iso14443_4a_poller, instance->tx_buffer, instance->rx_buffer);

        t_union_trace(instance, "Cmd packet:");

        if(iso14443_4a_error != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "Failed to read SFI 0x%X record %d", sfi, record_num);
            err = t_union_poller_process_error(iso14443_4a_error);
            break;
        }

        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);
        size_t buff_len = bit_buffer_get_size_bytes(instance->rx_buffer);

        if(t_union_response_error(buff, buff_len)) {
            err = TUnionPollerErrorProtocol;
            FURI_LOG_E(TAG, "Failed to Read SFI");
            break;
        }

    } while(false);

    return err;
}

// 读取支付记录文件
TUnionPollerError t_union_poller_read_transactions(TUnionPoller* instance) {
    TUnionPollerError err = TUnionPollerErrorNone;

    for(uint8_t record_num = 0; record_num < T_UNION_RECORD_TRANSACTIONS_MAX; record_num++) {
        err = t_union_poller_read_sfi_record(
            instance, T_UNION_RECORD_TRANSACTIONS_SFI, record_num + 1);
        if(err != TUnionPollerErrorNone) break;

        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);
        size_t buff_len = bit_buffer_get_size_bytes(instance->rx_buffer);

        if(buff_len != T_UNION_RECORD_TRANSACTIONS_RESP_PACKET_LEN) {
            err = TUnionPollerErrorProtocol;
            FURI_LOG_E(TAG, "Failed to Read SFI");
            break;
        }

        t_union_decode_transaction(instance->application.message, buff, record_num);
    }
    return err;
}

// 读取行程记录文件
TUnionPollerError t_union_poller_read_travels(TUnionPoller* instance) {
    TUnionPollerError err = TUnionPollerErrorNone;

    for(uint8_t record_num = 0; record_num < T_UNION_RECORD_TRAVELS_MAX; record_num++) {
        err = t_union_poller_read_sfi_record(instance, T_UNION_RECORD_TRAVELS_SFI, record_num + 1);
        if(err != TUnionPollerErrorNone) break;

        const uint8_t* buff = bit_buffer_get_data(instance->rx_buffer);
        size_t buff_len = bit_buffer_get_size_bytes(instance->rx_buffer);

        if(buff_len != T_UNION_RECORD_TRAVELS_RESP_PACKET_LEN) {
            err = TUnionPollerErrorProtocol;
            FURI_LOG_E(TAG, "Failed to Read SFI");
            break;
        }

        t_union_decode_travel(instance->application.message, buff, record_num);
    }
    return err;
}

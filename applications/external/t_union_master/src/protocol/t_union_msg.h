#pragma once

#include <furi.h>

#define T_UNION_BALANCE_RESP_PACKET_LEN 6

#define T_UNION_RECORD_TRANSACTIONS_SFI             0x18
#define T_UNION_RECORD_TRANSACTIONS_MAX             10
#define T_UNION_RECORD_TRANSACTIONS_RESP_PACKET_LEN 25

#define T_UNION_RECORD_TRAVELS_SFI             0x1E
#define T_UNION_RECORD_TRAVELS_MAX             30
#define T_UNION_RECORD_TRAVELS_RESP_PACKET_LEN 50

// 交易记录
typedef struct {
    uint16_t seqense; // 交易序号
    uint32_t money; // 交易金额
    uint8_t type; // 交易类型 2:充值 9:复合支付
    char terminal_id[12 + 1]; // 终端id

    uint16_t year; // 年
    uint8_t month; // 月
    uint8_t day; // 日
    uint8_t hour; // 时
    uint8_t minute; // 分
    uint8_t second; // 秒
} TUnionTransaction;

// 行程记录
typedef struct {
    uint8_t type; // 主类型 3:进站 4:出站 6:单次
    char terminal_id[16 + 1]; // 终端id
    uint8_t sub_type; // 子类型
    char station_id[14 + 1]; // 站台id
    uint32_t money; // 交易金额
    uint32_t balance; // 余额

    uint16_t year; // 年
    uint8_t month; //月
    uint8_t day; //日
    uint8_t hour; //时
    uint8_t minute; //分
    uint8_t second; //秒

    char city_id[4 + 1]; // 城市id

    char institution_id[16 + 1]; // 流水id
} TUnionTravel;

// 卡种
typedef enum {
    TunionCardTypeNone = 0,
    TunionCardTypeNormal, // 普通卡
    TunionCardTypeStudent, // 学生卡
    TunionCardTypeElder, // 老人卡
    TunionCardTypeTest, // 测试卡
    TunionCardTypeSoldier, // 军人卡
} TunionCardType;

typedef struct {
    uint8_t app_version; // 应用版本
    TunionCardType type; // 卡种
    char city_id[4 + 1]; // 发卡地城市id
    char card_number[20 + 1]; // 卡号

    uint16_t iss_year; // 签发年
    uint8_t iss_month; // 签发月
    uint8_t iss_day; // 签发日

    uint16_t exp_year; // 到期年
    uint8_t exp_month; // 到期月
    uint8_t exp_day; // 到期日

    uint32_t balance; // 余额

    TUnionTransaction transactions[T_UNION_RECORD_TRANSACTIONS_MAX]; // 交易记录
    size_t transaction_cnt;
    TUnionTravel travels[T_UNION_RECORD_TRAVELS_MAX]; // 行程记录
    size_t travel_cnt;
} TUnionMessage;

TUnionMessage* t_union_msg_alloc();
void t_union_msg_free(TUnionMessage* msg);
void t_union_msg_reset(TUnionMessage* msg);

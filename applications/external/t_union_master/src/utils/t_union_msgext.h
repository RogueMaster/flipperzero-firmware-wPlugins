#pragma once

#include <furi.h>
#include "../protocol/t_union_msg.h"
#include <storage/storage.h>

typedef enum {
    LineTypeUnknown = 0,
    LineTypeMetro, // 地铁
    LineTypeBRT, // 快速公交
    LineTypeTram, // 有轨电车
    LineTypeTrain, // 火车
    LineTypeBUS, // 公交
} LineType;

typedef struct {
    FuriString* city_name; // 城市名
    FuriString* line_name; // 线路名
    FuriString* station_name; // 站台名
    LineType line_type; // 线路类型
    bool transfer; // 换乘标识
    bool night; // 夜间标识
    bool roaming; // 漫游标识
} TUnionTravelExt;

// 扩展数据
typedef struct {
    FuriString* type_name; // 卡种名称
    FuriString* city_name; // 发卡地城市名
    FuriString* card_name; // 卡名称

    TUnionTravelExt travels_ext[T_UNION_RECORD_TRAVELS_MAX]; // 行程记录
} TUnionMessageExt;

TUnionMessageExt* t_union_msgext_alloc();
void t_union_msgext_free(TUnionMessageExt* msg_ext);
void t_union_msgext_reset(TUnionMessageExt* msg_ext);

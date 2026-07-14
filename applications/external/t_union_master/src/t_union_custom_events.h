#pragma once

typedef enum {
    TUM_CustomEventReserved = 100,

    TUM_CustomEventViewExit,

    TUM_CustomEventWorkerSuccess, //读取成功
    TUM_CustomEventWorkerFail, // 读取失败
    TUM_CustomEventWorkerDetected, // 已识别卡片

    TUM_CustomEventSwitchToBaseinfo, // 跳转基本信息界面
    TUM_CustomEventSwitchToTransaction, // 跳转为交易记录列表
    TUM_CustomEventSwitchToTravel, // 跳转为行程记录列表
    TUM_CustomEventSwitchToTransactionDetail, // 跳转为交易记录详情
    TUM_CustomEventSwitchToTravelDetail, // 跳转为行程记录详情
} TUM_CustomEvent;

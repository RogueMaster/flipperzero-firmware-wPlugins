/**
 * @file fn_commands.h
 * @brief Коды команд ФН, используемые приложением (Протокол v1.2.05).
 *
 * Коды команд известны только слою proto и fn_ops.c: UI и сцены их не видят.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Коды команд ККТ.
 * Раздел 4.2 Протокола v1.2.05 (сервисные команды).
 */
typedef enum {
    FN_CMDGetFNStatus = 0x30, /**< 4.2.1 — статус ФН: этап, смена, флаги, SN */
    FN_CMDGetFNEndDate = 0x32, /**< 4.2.3 — срок действия ФН */
    FN_CMDGetFNFwVersion = 0x33, /**< 4.2.4 — версия ПО ФН */
    FN_CMDGetFNSessionInfo = 0x10, /**< 4.2.1 — номер смены и чека */
    FN_CMDGetFFD = 0x3A, /**< 4.2.7 — версия ФФД */
    FN_CMDGetFNEndDays = 0x3B, /**< 4.2.8 — дней до окончания срока */
    FN_CMDGetFNMemoryInfo = 0x3D, /**< 4.2.10 — ресурс памяти ФН */
    FN_CMDFlashMGM = 0x60, /**< расширение проекта — сброс МГМ */
} FN_CMD;

/** Параметр команды 60h: сброс МГМ (0x16). */
#define FN_CMD_FLASH_MGM_PARAM 0x16u

#ifdef __cplusplus
}
#endif

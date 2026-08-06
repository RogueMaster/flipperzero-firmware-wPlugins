#pragma once

#include <cc1101_regs.h>

/* Пресет CC1101 для TPMS: 2-FSK, 20 kBaud, девиация 28.56 кГц,
 * полоса приёма 325 кГц. Взят из ProtoView (custom_presets.h,
 * protoview_subghz_tpms1_fsk_async_regs) — там он проверен на реальных
 * датчиках Renault.
 *
 * Формат для furi_hal_subghz_load_custom_preset / FuriHalSubGhzPresetCustom:
 * плоский массив пар "регистр, значение", терминатор 0x00 0x00, затем
 * 8 байт PA table.
 */
static const uint8_t tpms_fsk_preset[] = {
    /* GDO0 отдаёт демодулированные данные асинхронно */
    CC1101_IOCFG0,
    0x0D,

    /* Синтезатор частоты: IF = 26 МГц / 2^10 * 6 = 152.34 кГц */
    CC1101_FSCTRL1,
    0x06,

    /* Пакетный движок: асинхронный режим, без whitening */
    CC1101_PKTCTRL0,
    0x32,
    CC1101_PKTCTRL1,
    0x04,

    /* Модем */
    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x02,
    CC1101_MDMCFG2,
    0x04, /* 2-FSK, без преамбулы/sync — их разбираем сами */
    CC1101_MDMCFG3,
    0x93, /* 20 kBaud */
    CC1101_MDMCFG4,
    0x59, /* полоса 325 кГц */
    CC1101_DEVIATN,
    0x41, /* девиация 28.56 кГц */

    /* Автокалибровка при переходе idle -> rx/tx */
    CC1101_MCSM0,
    0x18,

    /* Компенсация смещения частоты */
    CC1101_FOCCFG,
    0x16,

    /* АРУ */
    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x07,

    /* Wake on radio */
    CC1101_WORCTRL,
    0xFB,

    /* Фронтенд */
    CC1101_FREND0,
    0x10,
    CC1101_FREND1,
    0x56,

    /* Конец списка регистров */
    0x00,
    0x00,

    /* PA table (в RX не используется) */
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

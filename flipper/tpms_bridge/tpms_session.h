#pragma once

#include "tpms_renault.h"

#include <stdbool.h>
#include <stdint.h>

/** Приёмная сессия: владеет радиомодулем и декодером.
 *
 * Разбор данных не крутится в отдельном потоке — его двигает владелец
 * вызовами tpms_session_pump(). Так один и тот же код работает и из
 * потока CLI-команды (где доступен stdout), и из локального потока
 * приёма на самом Flipper.
 */
typedef struct TpmsSession TpmsSession;

/** Кадр с сошедшейся CRC. */
typedef void (*TpmsSessionFrameCallback)(const TpmsRenaultFrame* frame, void* context);

/** Сырой интервал (режим диагностики). */
typedef void (*TpmsSessionRawCallback)(bool level, uint32_t duration, void* context);

TpmsSession* tpms_session_alloc(void);
void tpms_session_free(TpmsSession* session);

void tpms_session_set_frame_callback(
    TpmsSession* session,
    TpmsSessionFrameCallback callback,
    void* context);

/** Включить выдачу сырых интервалов. NULL — выключить. */
void tpms_session_set_raw_callback(
    TpmsSession* session,
    TpmsSessionRawCallback callback,
    void* context);

bool tpms_session_start(TpmsSession* session, uint32_t frequency);
void tpms_session_stop(TpmsSession* session);

/** Разобрать накопленное. Возвращает число обработанных интервалов. */
size_t tpms_session_pump(TpmsSession* session, uint32_t timeout_ms);

/** Импульс LF-поля 125 кГц, не прерывая приём.
 *
 * Датчик отвечает практически сразу после активации, поэтому глушить
 * приёмник на время импульса нельзя — разбор продолжается всё это время.
 */
void tpms_session_wake_pulse(TpmsSession* session, uint32_t duration_ms);

float tpms_session_get_rssi(TpmsSession* session);

/** Сколько интервалов потеряно из-за переполнения буфера. */
uint32_t tpms_session_get_overruns(TpmsSession* session);

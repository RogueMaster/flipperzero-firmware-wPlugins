/**
 * @file fn_info.h
 * @brief Агрегат FnInfo — все данные экрана «Информация об ФН».
 */
#pragma once

#include "fn_ident.h"
#include "fn_status.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Снимок данных ФН для экрана информации. */
typedef struct {
    FnStatus status;
    FnFfdInfo ffd;
    FnFwInfo fw;
    FnSession session;
    bool has_ffd;
    bool has_fw;
    bool has_session;
    bool response_has_warning;
} FnInfo;

/** Выделить обнуленный FnInfo (serial[0] = '\0'). */
FnInfo* fn_info_alloc(void);

void fn_info_free(FnInfo* info);

/** Сбросить все поля перед новой операцией. */
void fn_info_reset(FnInfo* info);

#ifdef __cplusplus
}
#endif

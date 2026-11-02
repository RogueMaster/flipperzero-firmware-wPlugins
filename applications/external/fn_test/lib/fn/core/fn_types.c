#include "fn_types.h"

#include <stddef.h>

bool fn_date_is_valid(const FnDate* date) {
    if(!date) return false;
    return date->month >= 1 && date->month <= 12 && date->day >= 1 && date->day <= 31;
}

bool fn_datetime_is_valid(const FnDateTime* date_time) {
    if(!date_time) return false;
    if(!fn_date_is_valid(&date_time->date)) return false;
    return date_time->hour <= 23 && date_time->minute <= 59;
}

const char* fn_stage_text(FnStage stage) {
    switch(stage) {
    case FnStageReadyRegistration:
        return "Готов к регистрации";
    case FnStageRegistered:
        return "Зарегистрирован";
    case FnStageArchiveClosed:
        return "Архив закрыт, ждет передачи в ОФД";
    case FnStageArchiveClosedFinal:
        return "Архив ФН закрыт";
    case FnStageUnknown:
    default:
        return "Неизвестный этап";
    }
}

const char* fn_ffd_text(FnFfd ffd) {
    switch(ffd) {
    case FnFfdNone:
        return "Не активизирован";
    case FnFfd1_05:
        return "1.05";
    case FnFfd1_1:
        return "1.1";
    case FnFfd1_2:
        return "1.2";
    case FnFfdUnknown:
    default:
        return "Неизвестно";
    }
}

const char* fn_doc_text(FnDocCode doc) {
    switch(doc) {
    case FnDocNone:
        return "Документа нет";
    case FnDocRegistration:
        return "Отчет о регистрации";
    case FnDocSessionOpen:
        return "Отчет об открытии смены";
    case FnDocReceipt:
        return "Кассовый чек";
    case FnDocSessionClose:
        return "Отчет о закрытии смены";
    case FnDocFnClose:
        return "Отчет о закрытии ФН";
    case FnDocStrictReport:
        return "Бланк строгой отчетности";
    case FnDocRegChangeFnReplace:
        return "Изменение регистрации при замене ФН";
    case FnDocRegChange:
        return "Изменение параметров регистрации";
    case FnDocReceiptCorrection:
        return "Кассовый чек коррекции";
    case FnDocStrictReportCorrection:
        return "БСО коррекции";
    case FnDocCurrentSettlements:
        return "Отчет о текущем состоянии расчетов";
    case FnDocUnknown:
    default:
        return "Неизвестный документ";
    }
}

const char* fn_warn_flag_text(uint8_t mask) {
    switch(mask) {
    case FnWarnReplaceSoon:
        return "Срочная замена ФН (до 3 дней)";
    case FnWarnResourceSoon:
        return "Исчерпание ресурса ФН (до 30 дней)";
    case FnWarnMemoryFull:
        return "Память ФН заполнена на 99%";
    case FnWarnOfdTimeout:
        return "Превышено время ожидания ОФД";
    case FnWarnOfdFormatLogic:
        return "Отказ по форматно-логическому контролю";
    case FnWarnNeedSetup:
        return "Требуется настройка ККТ";
    case FnWarnOfdCancelled:
        return "ОФД аннулирован";
    default:
        return NULL;
    }
}

uint8_t fn_warn_flags_unknown(uint8_t flags) {
    return flags & FnWarnReserved;
}

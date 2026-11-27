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

const char* fn_stage_text(FnStage stage, FnLang lang) {
    const bool ru = (lang == FnLangRu);

    switch(stage) {
    case FnStageReadyRegistration:
        return ru ? "Готов к регистрации" : "Ready for registration";
    case FnStageRegistered:
        return ru ? "Зарегистрирован" : "Registered";
    case FnStageArchiveClosed:
        return ru ? "Архив закрыт, ждет передачи в ОФД" : "Archive closed, waiting for OFD";
    case FnStageArchiveClosedFinal:
        return ru ? "Архив ФН закрыт" : "FN archive closed";
    case FnStageUnknown:
    default:
        return ru ? "Неизвестный этап" : "Unknown stage";
    }
}

const char* fn_ffd_text(FnFfd ffd, FnLang lang) {
    const bool ru = (lang == FnLangRu);

    switch(ffd) {
    case FnFfdNone:
        return ru ? "Не активизирован" : "Not activated";
    case FnFfd1_05:
        return "1.05";
    case FnFfd1_1:
        return "1.1";
    case FnFfd1_2:
        return "1.2";
    case FnFfdUnknown:
    default:
        return ru ? "Неизвестно" : "Unknown";
    }
}

const char* fn_doc_text(FnDocCode doc, FnLang lang) {
    const bool ru = (lang == FnLangRu);

    switch(doc) {
    case FnDocNone:
        return ru ? "Документа нет" : "No document";
    case FnDocRegistration:
        return ru ? "Отчет о регистрации" : "Registration report";
    case FnDocSessionOpen:
        return ru ? "Отчет об открытии смены" : "Shift open report";
    case FnDocReceipt:
        return ru ? "Кассовый чек" : "Receipt";
    case FnDocSessionClose:
        return ru ? "Отчет о закрытии смены" : "Shift close report";
    case FnDocFnClose:
        return ru ? "Отчет о закрытии ФН" : "FN close report";
    case FnDocStrictReport:
        return ru ? "Бланк строгой отчетности" : "Strict reporting form";
    case FnDocRegChangeFnReplace:
        return ru ? "Изменение регистрации при замене ФН" : "Registration change on FN replacement";
    case FnDocRegChange:
        return ru ? "Изменение параметров регистрации" : "Registration parameters change";
    case FnDocReceiptCorrection:
        return ru ? "Кассовый чек коррекции" : "Receipt correction";
    case FnDocStrictReportCorrection:
        return ru ? "БСО коррекции" : "Strict form correction";
    case FnDocCurrentSettlements:
        return ru ? "Отчет о текущем состоянии расчетов" : "Current settlements report";
    case FnDocUnknown:
    default:
        return ru ? "Неизвестный документ" : "Unknown document";
    }
}

const char* fn_warn_flag_text(uint8_t mask, FnLang lang) {
    const bool ru = (lang == FnLangRu);

    switch(mask) {
    case FnWarnReplaceSoon:
        return ru ? "Срочная замена ФН (до 3 дней)" : "FN replacement soon (up to 3 days)";
    case FnWarnResourceSoon:
        return ru ? "Исчерпание ресурса ФН (до 30 дней)" : "FN resource running out (up to 30 days)";
    case FnWarnMemoryFull:
        return ru ? "Память ФН заполнена на 99%" : "FN memory is 99% full";
    case FnWarnOfdTimeout:
        return ru ? "Превышено время ожидания ОФД" : "OFD timeout";
    case FnWarnOfdFormatLogic:
        return ru ? "Отказ по форматно-логическому контролю" : "Format-logic check failure";
    case FnWarnNeedSetup:
        return ru ? "Требуется настройка ККТ" : "Cash register setup required";
    case FnWarnOfdCancelled:
        return ru ? "ОФД аннулирован" : "OFD cancelled";
    default:
        return NULL;
    }
}

uint8_t fn_warn_flags_unknown(uint8_t flags) {
    return flags & FnWarnReserved;
}

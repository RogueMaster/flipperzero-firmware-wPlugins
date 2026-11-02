#include "fn_format.h"

#include "fn_strings.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define FN_FORMAT_NUMBER_LEN 24
#define FN_FORMAT_DATE_LEN   24
/* Значения с единицей измерения: «5000 документов», «8192 КБ», «360 дней». */
#define FN_FORMAT_VALUE_LEN  FN_TEXT_LINE_LEN
/* Буфер под длинные значения: максимум исходной строки до переноса. */
#define FN_FORMAT_TEXT_BUF   (FN_TEXT_LINE_LEN * 2)

/** Добавить секцию; при переполнении модели — NULL. */
static FnTextSection* format_section(FnTextModel* model, const char* title) {
    if(!model || model->section_count >= FN_TEXT_MAX_SECTIONS) return NULL;

    FnTextSection* section = &model->sections[model->section_count];
    memset(section, 0, sizeof(*section));
    section->title = title;
    model->section_count++;
    return section;
}

/** Дописать готовый фрагмент в секцию, не разрывая UTF-8. */
static void
    format_add_raw(FnTextSection* section, FnTextStyle style, const char* text, size_t len) {
    if(!section || !text || section->line_count >= FN_TEXT_MAX_LINES) return;

    FnTextLine* line = &section->lines[section->line_count];
    memset(line, 0, sizeof(*line));

    if(len > sizeof(line->text) - 1) len = sizeof(line->text) - 1;
    while(len > 0 && ((unsigned char)text[len] & 0xC0) == 0x80) {
        len--;
    }
    memcpy(line->text, text, len);
    line->text[len] = '\0';

    line->emphasis = style;
    section->line_count++;
}

/**
 * Разложить строку по строкам модели: по словам, при отсутствии пробела —
 * по байтовой границе UTF-8 (без потери символов, FR-025).
 */
static void format_wrapped(FnTextSection* section, FnTextStyle style, const char* text) {
    const size_t max = FN_TEXT_LINE_LEN - 1;

    while(text && *text) {
        const size_t len = strlen(text);
        if(len <= max) {
            format_add_raw(section, style, text, len);
            return;
        }

        size_t cut = max;
        while(cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) {
            cut--;
        }

        size_t split = cut;
        while(split > 0 && text[split] != ' ') {
            split--;
        }
        if(split == 0) split = cut;

        format_add_raw(section, style, text, split);

        text += split;
        while(*text == ' ')
            text++;
    }
}

/** Добавить строку секции с форматированием и переносом длинных значений. */
static void format_line(FnTextSection* section, FnTextStyle style, const char* fmt, ...) {
    if(!section || !fmt || section->line_count >= FN_TEXT_MAX_LINES) return;

    char text[FN_FORMAT_TEXT_BUF];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    format_wrapped(section, style, text);
}

/** Строка «Подпись: значение»; NULL-значение показывается как «нет данных». */
static void format_kv(FnTextSection* section, const char* label, const char* value) {
    format_line(section, FnTextStyleNormal, "%s: %s", label, value ? value : FN_STR_NO_DATA);
}

/** Текущий документ для ОФД и признак получения данных. */
static void format_current_doc(FnTextSection* section, const FnStatus* status) {
    format_line(section, FnTextStyleNormal, "%s", FN_STR_DOC_HEADER);

    if(status->current_doc == FnDocUnknown) {
        format_line(
            section, FnTextStyleNormal, FN_STR_DOC_UNKNOWN_FMT, (unsigned)status->current_doc_raw);
    } else {
        format_line(
            section, FnTextStyleNormal, FN_STR_INDENT_FMT, fn_doc_text(status->current_doc));
    }

    /* Для «документа нет» признак получения данных не имеет смысла. */
    if(status->current_doc != FnDocNone) {
        format_line(
            section,
            FnTextStyleNormal,
            FN_STR_INDENT_FMT,
            status->doc_data_received ? FN_STR_DOC_DATA_RECEIVED : FN_STR_DOC_NO_DATA);
    }
}

/** Номер смены и чека — только для этапа 2 (FR-027). */
static void format_session(FnTextSection* section, const FnInfo* info) {
    char value[FN_FORMAT_NUMBER_LEN];

    if(info->has_session) {
        snprintf(value, sizeof(value), "%u", (unsigned)info->session.shift_number);
        format_kv(section, FN_STR_SHIFT, value);

        snprintf(value, sizeof(value), "%u", (unsigned)info->session.receipt_number);
        format_kv(section, FN_STR_RECEIPT, value);
    } else {
        format_kv(section, FN_STR_SHIFT, FN_STR_NO_DATA);
        format_kv(section, FN_STR_RECEIPT, FN_STR_NO_DATA);
    }
}

/** Номер и дата последнего фискального документа — для этапов 2–3. */
static void format_last_doc(FnTextSection* section, const FnStatus* status) {
    char value[FN_FORMAT_DATE_LEN];

    if(status->has_last_doc_number) {
        snprintf(value, sizeof(value), "%lu", (unsigned long)status->last_doc_number);
        format_kv(section, FN_STR_LAST_DOC, value);
    } else {
        format_kv(section, FN_STR_LAST_DOC, FN_STR_NO_DATA);
    }

    if(status->has_last_doc) {
        const FnDateTime* dt = &status->last_doc_dt;
        snprintf(
            value,
            sizeof(value),
            FN_STR_DATE_FMT,
            (unsigned)dt->date.year,
            (unsigned)dt->date.month,
            (unsigned)dt->date.day,
            (unsigned)dt->hour,
            (unsigned)dt->minute);
        format_kv(section, FN_STR_LAST_DOC_DATE, value);
    } else {
        format_kv(section, FN_STR_LAST_DOC_DATE, FN_STR_NO_DATA);
    }
}

/** Этапы, для которых существуют документы и выводится блок «Последний ФД». */
static bool format_stage_has_last_doc(FnStage stage) {
    return stage == FnStageRegistered || stage == FnStageArchiveClosed;
}

static void format_section_status(FnTextModel* model, const FnInfo* info) {
    const FnStatus* status = &info->status;
    FnTextSection* section = format_section(model, FN_STR_SECTION_FN_STATUS);
    if(!section) return;

    if(status->has_serial) {
        format_kv(section, FN_STR_SN, status->serial);
    } else {
        format_kv(section, FN_STR_SN, FN_STR_NO_DATA);
    }

    format_kv(section, FN_STR_STAGE, fn_stage_text(status->stage));
    format_kv(
        section,
        FN_STR_SESSION,
        status->session_open ? FN_STR_SESSION_OPEN : FN_STR_SESSION_CLOSED);

    format_current_doc(section, status);

    if(status->stage == FnStageRegistered) {
        format_session(section, info);
    }
    if(format_stage_has_last_doc(status->stage)) {
        format_last_doc(section, status);
    }
}

/** Блок предупреждений: только при ненулевых флагах (contracts/ui.md §8.1). */
static void format_section_warnings(FnTextModel* model, const FnStatus* status) {
    if(status->warn_flags == 0) return;

    FnTextSection* section = format_section(model, FN_STR_WARNINGS);
    if(!section) return;

    for(uint8_t bit = 0x01; bit != 0; bit <<= 1) {
        if((status->warn_flags & bit) == 0) continue;
        const char* text = fn_warn_flag_text(bit);
        if(text) {
            format_line(section, FnTextStyleWarning, "%s", text);
        }
    }

    const uint8_t unknown = fn_warn_flags_unknown(status->warn_flags);
    if(unknown != 0) {
        format_line(section, FnTextStyleWarning, FN_STR_OTHER_WARNINGS, (unsigned)unknown);
    }
}

static void format_section_ffd(FnTextModel* model, const FnInfo* info) {
    FnTextSection* section = format_section(model, FN_STR_SECTION_FFD);
    if(!section) return;

    if(!info->has_ffd) {
        format_line(section, FnTextStyleNormal, "%s", FN_STR_NO_DATA);
        return;
    }

    format_kv(section, FN_STR_FFD_CURRENT, fn_ffd_text(info->ffd.current));
    format_kv(section, FN_STR_FFD_MAX, fn_ffd_text(info->ffd.max));
}

static void format_section_fw(FnTextModel* model, const FnInfo* info) {
    FnTextSection* section = format_section(model, FN_STR_SECTION_FW);
    if(!section) return;

    if(!info->has_fw) {
        format_line(section, FnTextStyleNormal, "%s", FN_STR_NO_DATA);
        return;
    }

    format_kv(section, FN_STR_FW, info->fw.version[0] ? info->fw.version : FN_STR_NO_DATA);
    format_kv(section, FN_STR_MGM, info->fw.is_mgm ? FN_STR_MGM_YES : FN_STR_MGM_NO);
}

void fn_format_info(const FnInfo* info, FnTextModel* model) {
    if(!info || !model) return;

    memset(model, 0, sizeof(*model));

    format_section_status(model, info);
    format_section_warnings(model, &info->status);
    format_section_ffd(model, info);
    format_section_fw(model, info);
}

/** Согласование слова «день» с числом: 1 день, 2 дня, 360 дней. */
static const char* format_days_word(unsigned days) {
    const unsigned mod100 = days % 100;
    if(mod100 >= 11 && mod100 <= 14) return FN_STR_DAY_MANY;

    switch(days % 10) {
    case 1:
        return FN_STR_DAY_ONE;
    case 2:
    case 3:
    case 4:
        return FN_STR_DAY_FEW;
    default:
        return FN_STR_DAY_MANY;
    }
}

/** «Срок действия (32h)»: дата окончания и счетчики регистраций (FR-010). */
static void format_section_life_date(FnTextModel* model, const FnLifeInfo* life) {
    FnTextSection* section = format_section(model, FN_STR_SECTION_LIFE_DATE);
    if(!section) return;

    char value[FN_FORMAT_NUMBER_LEN];

    if(!life->has_date) {
        format_kv(section, FN_STR_LIFE_END_DATE, FN_STR_NO_DATA);
        format_kv(section, FN_STR_REG_DONE, FN_STR_NO_DATA);
        format_kv(section, FN_STR_REG_LEFT, FN_STR_NO_DATA);
        return;
    }

    /* Нулевая дата — ФН ещё не активизирован, а не «нет данных». */
    if(life->date.end_date.year == 0) {
        format_kv(section, FN_STR_LIFE_END_DATE, FN_STR_LIFE_NOT_ACTIVATED);
    } else {
        const FnDate* date = &life->date.end_date;
        snprintf(
            value,
            sizeof(value),
            FN_STR_SHORT_DATE_FMT,
            (unsigned)date->day,
            (unsigned)date->month,
            (unsigned)date->year);
        format_kv(section, FN_STR_LIFE_END_DATE, value);
    }

    snprintf(value, sizeof(value), "%u", (unsigned)life->date.reg_done);
    format_kv(section, FN_STR_REG_DONE, value);

    snprintf(value, sizeof(value), "%u", (unsigned)life->date.reg_remaining);
    format_kv(section, FN_STR_REG_LEFT, value);
}

/** «Остаток срока (3Bh)»: показывается всегда, нулевой остаток — «нет данных». */
static void format_section_life_days(FnTextModel* model, const FnLifeInfo* life) {
    FnTextSection* section = format_section(model, FN_STR_SECTION_LIFE_DAYS);
    if(!section) return;

    if(!life->has_days || !life->days.has_days) {
        format_line(section, FnTextStyleNormal, "%s", FN_STR_NO_DATA);
        return;
    }

    char value[FN_FORMAT_VALUE_LEN];
    snprintf(
        value,
        sizeof(value),
        FN_STR_DAYS_FMT,
        (unsigned)life->days.days,
        format_days_word(life->days.days));
    format_line(section, FnTextStyleNormal, "%s", value);
}

/** «Ресурс памяти (3Dh)»: 5-летний, 30-дневный и (при наличии) процент маркировки. */
static void format_section_memory(FnTextModel* model, const FnLifeInfo* life) {
    FnTextSection* section = format_section(model, FN_STR_SECTION_MEMORY);
    if(!section) return;

    if(!life->has_memory) {
        format_line(section, FnTextStyleNormal, "%s", FN_STR_NO_DATA);
        return;
    }

    char value[FN_FORMAT_VALUE_LEN];
    snprintf(
        value,
        sizeof(value),
        FN_STR_FIVE_YEAR_FMT,
        (unsigned long)life->memory.five_year_documents,
        FN_STR_DOCUMENTS);
    format_kv(section, FN_STR_MEM_FIVE_YEAR, value);

    snprintf(
        value,
        sizeof(value),
        FN_STR_THIRTY_DAY_FMT,
        (unsigned long)life->memory.thirty_day_kb,
        FN_STR_KB);
    format_kv(section, FN_STR_MEM_THIRTY_DAY, value);

    /* Девятого байта нет — строки о маркировке нет вовсе (FR-028). */
    if(life->memory.has_marking) {
        format_line(
            section, FnTextStyleNormal, FN_STR_MARKING_FMT, (unsigned)life->memory.marking_percent);
    }
}

void fn_format_life(const FnLifeInfo* life, FnTextModel* model) {
    if(!life || !model) return;

    memset(model, 0, sizeof(*model));

    /* Блоки 32h/3Bh/3Dh выводятся всегда, независимо от ФФД и этапа. */
    format_section_life_date(model, life);
    format_section_life_days(model, life);
    format_section_memory(model, life);
}

/** Отказ ФН: русский заголовок, подсказка и код ответа (FR-014, FR-029). */
static void format_result_refused(FnTextSection* section, FNError error) {
    if(!fn_error_is_known(error)) {
        format_line(section, FnTextStyleWarning, FN_STR_UNKNOWN_CODE_FMT, (unsigned)error);
        return;
    }

    format_line(section, FnTextStyleWarning, "%s", fn_error_title(error));
    const char* hint = fn_error_hint(error);
    if(hint && hint[0] != '\0') {
        format_line(section, FnTextStyleNormal, "%s", hint);
    }
    format_line(section, FnTextStyleNormal, FN_STR_ERROR_CODE_FMT, (unsigned)error);
}

void fn_format_result(const FnOpResult* result, FnResultReason reason, FnTextModel* model) {
    if(!model) return;

    memset(model, 0, sizeof(*model));

    FnTextSection* section = format_section(model, FN_STR_RESULT_TITLE);
    if(!section) return;

    if(reason == FnResultReasonNotMgm) {
        format_line(section, FnTextStyleWarning, "%s", FN_STR_NOT_MGM);
        return;
    }

    if(!result) {
        format_line(section, FnTextStyleNormal, "%s", FN_STR_NO_DATA);
        return;
    }

    switch(result->status) {
    case FnOpStatusOk:
        /* Успех показывается только для изменяющей команды. */
        if(result->kind == FnOpFlashMgm) {
            format_line(section, FnTextStyleNormal, "%s", FN_STR_RESULT_OK);
        } else {
            format_line(section, FnTextStyleNormal, "%s", FN_STR_NO_DATA);
        }
        break;
    case FnOpStatusFnRefused:
        format_result_refused(section, result->error);
        break;
    case FnOpStatusNoResponse:
        format_line(section, FnTextStyleWarning, "%s", FN_STR_NO_RESPONSE);
        format_line(section, FnTextStyleNormal, "%s", FN_STR_NO_RESPONSE_HINT);
        break;
    case FnOpStatusBadFrame:
        format_line(section, FnTextStyleWarning, "%s", FN_STR_BAD_FRAME);
        break;
    case FnOpStatusBusy:
        format_line(section, FnTextStyleWarning, "%s", FN_STR_BUSY);
        break;
    case FnOpStatusInternalError:
        /* Для очистки МГМ внутренняя ошибка — единственная защита от отправки
         * команды в ФН, не подтверждённый как МГМ (FR-024). */
        format_line(
            section,
            FnTextStyleWarning,
            "%s",
            (result->kind == FnOpFlashMgm) ? FN_STR_NOT_MGM : FN_STR_PORT_BUSY);
        break;
    case FnOpStatusCancelled:
    default:
        /* Отменённая операция не показывает результат (пользователь уже в меню). */
        format_line(section, FnTextStyleNormal, "%s", FN_STR_NO_DATA);
        break;
    }
}

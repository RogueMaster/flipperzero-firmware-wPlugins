#include "fn_error.h"

#include <stddef.h>

typedef struct {
    FNError code;
    const char* title;
    const char* hint;
} FNErrorEntry;

static const FNErrorEntry fn_error_table[] = {
    {FNOk, "Успешно", ""},
    {FNErrorUnknownCMD, "Неизвестная команда", "Проверьте формат посылки"},
    {FNErrorWrongState, "Другое состояние ФН", "Команда недоступна на этом этапе"},
    {FNErrorFailure, "Отказ ФН", "Запросите детали командой 35h"},
    {FNErrorFailureKS, "Отказ КС", "Запросите детали командой 35h"},
    {FNErrorCMDParamsNotMatchFNEndDate,
     "Параметры не соответствуют сроку ФН",
     "Измените параметры команды"},
    {FNErrorWrongDateTime, "Некорректная дата или время", "Проверьте дату и время"},
    {FNErrorNoRequestedData, "Нет запрошенных данных", "Данных нет в архиве ФН"},
    {FNErrorWrongParamValue, "Некорректное значение параметров", "Проверьте значения"},
    {FNErrorWrongCommandFfd11, "Команда не разрешена", "Только для ФФД 1.1"},
    {FNErrorForbiddenAttribute, "Неразрешенные реквизиты", "Проверьте реквизиты"},
    {FNErrorDuplicateData, "Дублирование данных", "Реквизит уже передан"},
    {FNErrorNoDataForAccounting, "Нет данных для учета", "Дополните данные документа"},
    {FNErrorTooManyPositions, "Превышено число позиций", "Сократите документ"},
    {FNErrorTlvTooBig, "Превышены размеры TLV", "Сократите TLV-данные"},
    {FNErrorNoTransport, "Нет связи с ОФД", "Установите соединение с ОФД"},
    {FNErrorResourceExhausted, "Исчерпан ресурс ФН", "Требуется закрытие ФН"},
    {FNErrorResourceLimited, "Ограничение ресурса ФН", "Проверьте флаги предупреждений"},
    {FNErrorSessionTooLong, "Превышена длительность смены", "Закройте смену"},
    {FNErrorWrongTimeInterval,
     "Неверный интервал между документами",
     "Проверьте время на ККТ"},
    {FNErrorWrongAttribute, "Некорректный реквизит", "Проверьте реквизит"},
    {FNErrorAttributeNotRegistered,
     "Реквизит не соответствует регистрации",
     "Проверьте настройки регистрации"},
    {FNErrorWrongAnswer, "Некорректный ответ", "Ответ не может быть принят"},
    {FNErrorKmUpdateNegative, "Отрицательный ответ обновления ключей", ""},
    {FNErrorKmUpdateUnknown, "Неизвестный ответ обновления ключей", ""},
    {FNErrorKmUpdateRepeat, "Требуется повтор обновления ключей", "Повторите обновление"},
    {FNErrorMarkingForbidden, "Маркировка запрещена", "ФН не активирован для маркировки"},
    {FNErrorMarkingSequence, "Неверная последовательность команд", ""},
    {FNErrorMarkingBlocked, "Маркировка заблокирована", "Переполнена область уведомлений"},
    {FNErrorKmTableOverflow, "Переполнена таблица проверки КМ", "Не более 128 КМ в чеке"},
    {FNErrorKmKeysExpired, "Истек период обновления ключей", "Обновите ключи проверки КМ"},
    {FNErrorTlvMissingAttribute, "В TLV нет нужных реквизитов", "Проверьте состав TLV"},
    {FNErrorKmNotChecked, "КМ ранее не проверялся", ""},
};

#define FN_ERROR_TABLE_SIZE (sizeof(fn_error_table) / sizeof(fn_error_table[0]))

static const FNErrorEntry* fn_error_find(FNError code) {
    for(size_t i = 0; i < FN_ERROR_TABLE_SIZE; ++i) {
        if(fn_error_table[i].code == code) return &fn_error_table[i];
    }
    return NULL;
}

bool fn_error_is_known(FNError code) {
    return fn_error_find(code) != NULL;
}

const char* fn_error_title(FNError code) {
    const FNErrorEntry* entry = fn_error_find(code);
    return entry ? entry->title : "Неизвестный код ответа ФН";
}

const char* fn_error_hint(FNError code) {
    const FNErrorEntry* entry = fn_error_find(code);
    return (entry && entry->hint) ? entry->hint : "";
}

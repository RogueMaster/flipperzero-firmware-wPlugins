/**
 * @file fn_strings.h
 * @brief Единый каталог пользовательских строк (английский и русский).
 *
 * Файл не включает gui/, furi и fn_view_*: его проверяет host-тест
 * tests/host/test_strings.c и check_layers.sh.
 *
 * Все строки выбираются по идентификатору и языку:
 *     fn_str(app->lang, FnStrMenuFnInfo)
 * Макросы FN_STR_* оставлены как русские псевдонимы для host-тестов.
 */
#pragma once

#include "../lib/fn/core/fn_lang.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* Главное меню. */
    FnStrMenuFnInfo = 0,
    FnStrMenuLifeInfo,
    FnStrMenuFlashMgm,
    FnStrMenuWiring,
    FnStrMenuAbout,
    FnStrMenuLanguage,

    /* Заголовки экранов. */
    FnStrAppTitle,
    FnStrInfoTitle,
    FnStrLifeTitle,
    FnStrResultTitle,
    FnStrWiringTitle,
    FnStrAboutTitle,
    FnStrConfirmTitle,
    FnStrLanguageTitle,

    /* Заголовки секций. */
    FnStrSectionFnStatus,
    FnStrSectionFfd,
    FnStrSectionFw,
    FnStrSectionLifeDate,
    FnStrSectionLifeDays,
    FnStrSectionMemory,

    /* Подписи полей. */
    FnStrSn,
    FnStrStage,
    FnStrSession,
    FnStrSessionOpen,
    FnStrSessionClosed,
    FnStrDoc,
    FnStrDocHeader,
    FnStrFfd,
    FnStrFw,
    FnStrMgm,
    FnStrShift,
    FnStrReceipt,
    FnStrLastDoc,
    FnStrLastDocDate,
    FnStrFfdCurrent,
    FnStrFfdMax,
    FnStrWarnings,
    FnStrOtherWarnings,

    /* Поля экрана «Фаза жизни ФН». */
    FnStrLifeEndDate,
    FnStrLifeNotActivated,
    FnStrRegDone,
    FnStrRegLeft,
    FnStrMemFiveYear,
    FnStrMemThirtyDay,
    FnStrDocuments,
    FnStrKb,
    FnStrMarkingFmt,

    /* Значения. */
    FnStrYes,
    FnStrNo,
    FnStrNoData,
    FnStrDocNone,
    FnStrDocDataReceived,
    FnStrDocNoData,
    FnStrMgmYes,
    FnStrMgmNo,
    FnStrIndentFmt,
    FnStrDocUnknownFmt,
    FnStrDateFmt,
    FnStrShortDateFmt,
    FnStrDaysFmt,
    FnStrDayOne,
    FnStrDayFew,
    FnStrDayMany,
    FnStrFiveYearFmt,
    FnStrThirtyDayFmt,

    /* Ожидание. */
    FnStrWaitDetect,
    FnStrWaitDetectHint,
    FnStrWaitRead,
    FnStrWaitReadHint,
    FnStrWaitFlash,
    FnStrWaitFlashHint,

    /* Результаты и ошибки. */
    FnStrResultOk,
    FnStrNoResponse,
    FnStrNoResponseHint,
    FnStrBadFrame,
    FnStrNotMgm,
    FnStrUnknownCodeFmt,
    FnStrErrorCodeFmt,
    FnStrBusy,
    FnStrPortBusy,

    /* Подтверждение. */
    FnStrConfirmBody,
    FnStrConfirmAccept,
    FnStrConfirmCancel,

    /* О приложении. */
    FnStrAboutApp,
    FnStrAboutAuthor,
    FnStrAboutVersionFmt,
    FnStrAboutUrl1,
    FnStrAboutUrl2,
    FnStrAboutDescription1,
    FnStrAboutDescription2,
    FnStrAboutDescription3,
    FnStrAboutDescription4,

    /* Выбор языка. */
    FnStrLanguageHint,
    FnStrLanguageEnglish,
    FnStrLanguageRussian,

    FnStrCount,
} FnStrId;

/** Строка каталога на выбранном языке. Никогда не NULL. */
const char* fn_str(FnLang lang, FnStrId id);

/* Русские псевдонимы: используются host-тестами и как справочник. */
#define FN_STR_MENU_FN_INFO   fn_str(FnLangRu, FnStrMenuFnInfo)
#define FN_STR_MENU_LIFE_INFO fn_str(FnLangRu, FnStrMenuLifeInfo)
#define FN_STR_MENU_FLASH_MGM fn_str(FnLangRu, FnStrMenuFlashMgm)
#define FN_STR_MENU_WIRING    fn_str(FnLangRu, FnStrMenuWiring)
#define FN_STR_MENU_ABOUT     fn_str(FnLangRu, FnStrMenuAbout)
#define FN_STR_MENU_LANGUAGE  fn_str(FnLangRu, FnStrMenuLanguage)

#define FN_STR_APP_TITLE      fn_str(FnLangRu, FnStrAppTitle)
#define FN_STR_INFO_TITLE     fn_str(FnLangRu, FnStrInfoTitle)
#define FN_STR_LIFE_TITLE     fn_str(FnLangRu, FnStrLifeTitle)
#define FN_STR_RESULT_TITLE   fn_str(FnLangRu, FnStrResultTitle)
#define FN_STR_WIRING_TITLE   fn_str(FnLangRu, FnStrWiringTitle)
#define FN_STR_ABOUT_TITLE    fn_str(FnLangRu, FnStrAboutTitle)
#define FN_STR_CONFIRM_TITLE  fn_str(FnLangRu, FnStrConfirmTitle)
#define FN_STR_LANGUAGE_TITLE fn_str(FnLangRu, FnStrLanguageTitle)

#define FN_STR_SECTION_FN_STATUS fn_str(FnLangRu, FnStrSectionFnStatus)
#define FN_STR_SECTION_FFD       fn_str(FnLangRu, FnStrSectionFfd)
#define FN_STR_SECTION_FW        fn_str(FnLangRu, FnStrSectionFw)
#define FN_STR_SECTION_LIFE_DATE fn_str(FnLangRu, FnStrSectionLifeDate)
#define FN_STR_SECTION_LIFE_DAYS fn_str(FnLangRu, FnStrSectionLifeDays)
#define FN_STR_SECTION_MEMORY    fn_str(FnLangRu, FnStrSectionMemory)

#define FN_STR_SN             fn_str(FnLangRu, FnStrSn)
#define FN_STR_STAGE          fn_str(FnLangRu, FnStrStage)
#define FN_STR_SESSION        fn_str(FnLangRu, FnStrSession)
#define FN_STR_SESSION_OPEN   fn_str(FnLangRu, FnStrSessionOpen)
#define FN_STR_SESSION_CLOSED fn_str(FnLangRu, FnStrSessionClosed)
#define FN_STR_DOC            fn_str(FnLangRu, FnStrDoc)
#define FN_STR_DOC_HEADER     fn_str(FnLangRu, FnStrDocHeader)
#define FN_STR_FFD            fn_str(FnLangRu, FnStrFfd)
#define FN_STR_FW             fn_str(FnLangRu, FnStrFw)
#define FN_STR_MGM            fn_str(FnLangRu, FnStrMgm)
#define FN_STR_SHIFT          fn_str(FnLangRu, FnStrShift)
#define FN_STR_RECEIPT        fn_str(FnLangRu, FnStrReceipt)
#define FN_STR_LAST_DOC       fn_str(FnLangRu, FnStrLastDoc)
#define FN_STR_LAST_DOC_DATE  fn_str(FnLangRu, FnStrLastDocDate)
#define FN_STR_FFD_CURRENT    fn_str(FnLangRu, FnStrFfdCurrent)
#define FN_STR_FFD_MAX        fn_str(FnLangRu, FnStrFfdMax)
#define FN_STR_WARNINGS       fn_str(FnLangRu, FnStrWarnings)
#define FN_STR_OTHER_WARNINGS fn_str(FnLangRu, FnStrOtherWarnings)

#define FN_STR_LIFE_END_DATE      fn_str(FnLangRu, FnStrLifeEndDate)
#define FN_STR_LIFE_NOT_ACTIVATED fn_str(FnLangRu, FnStrLifeNotActivated)
#define FN_STR_REG_DONE           fn_str(FnLangRu, FnStrRegDone)
#define FN_STR_REG_LEFT           fn_str(FnLangRu, FnStrRegLeft)
#define FN_STR_MEM_FIVE_YEAR      fn_str(FnLangRu, FnStrMemFiveYear)
#define FN_STR_MEM_THIRTY_DAY     fn_str(FnLangRu, FnStrMemThirtyDay)
#define FN_STR_DOCUMENTS          fn_str(FnLangRu, FnStrDocuments)
#define FN_STR_KB                 fn_str(FnLangRu, FnStrKb)
#define FN_STR_MARKING_FMT        fn_str(FnLangRu, FnStrMarkingFmt)

#define FN_STR_YES               fn_str(FnLangRu, FnStrYes)
#define FN_STR_NO                fn_str(FnLangRu, FnStrNo)
#define FN_STR_NO_DATA           fn_str(FnLangRu, FnStrNoData)
#define FN_STR_DOC_NONE          fn_str(FnLangRu, FnStrDocNone)
#define FN_STR_DOC_DATA_RECEIVED fn_str(FnLangRu, FnStrDocDataReceived)
#define FN_STR_DOC_NO_DATA       fn_str(FnLangRu, FnStrDocNoData)
#define FN_STR_MGM_YES           fn_str(FnLangRu, FnStrMgmYes)
#define FN_STR_MGM_NO            fn_str(FnLangRu, FnStrMgmNo)
#define FN_STR_INDENT_FMT        fn_str(FnLangRu, FnStrIndentFmt)
#define FN_STR_DOC_UNKNOWN_FMT   fn_str(FnLangRu, FnStrDocUnknownFmt)
#define FN_STR_DATE_FMT          fn_str(FnLangRu, FnStrDateFmt)
#define FN_STR_SHORT_DATE_FMT    fn_str(FnLangRu, FnStrShortDateFmt)
#define FN_STR_DAYS_FMT          fn_str(FnLangRu, FnStrDaysFmt)
#define FN_STR_DAY_ONE           fn_str(FnLangRu, FnStrDayOne)
#define FN_STR_DAY_FEW           fn_str(FnLangRu, FnStrDayFew)
#define FN_STR_DAY_MANY          fn_str(FnLangRu, FnStrDayMany)
#define FN_STR_FIVE_YEAR_FMT     fn_str(FnLangRu, FnStrFiveYearFmt)
#define FN_STR_THIRTY_DAY_FMT    fn_str(FnLangRu, FnStrThirtyDayFmt)

#define FN_STR_WAIT_DETECT      fn_str(FnLangRu, FnStrWaitDetect)
#define FN_STR_WAIT_DETECT_HINT fn_str(FnLangRu, FnStrWaitDetectHint)
#define FN_STR_WAIT_READ        fn_str(FnLangRu, FnStrWaitRead)
#define FN_STR_WAIT_READ_HINT   fn_str(FnLangRu, FnStrWaitReadHint)
#define FN_STR_WAIT_FLASH       fn_str(FnLangRu, FnStrWaitFlash)
#define FN_STR_WAIT_FLASH_HINT  fn_str(FnLangRu, FnStrWaitFlashHint)

#define FN_STR_RESULT_OK        fn_str(FnLangRu, FnStrResultOk)
#define FN_STR_NO_RESPONSE      fn_str(FnLangRu, FnStrNoResponse)
#define FN_STR_NO_RESPONSE_HINT fn_str(FnLangRu, FnStrNoResponseHint)
#define FN_STR_BAD_FRAME        fn_str(FnLangRu, FnStrBadFrame)
#define FN_STR_NOT_MGM          fn_str(FnLangRu, FnStrNotMgm)
#define FN_STR_UNKNOWN_CODE_FMT fn_str(FnLangRu, FnStrUnknownCodeFmt)
#define FN_STR_ERROR_CODE_FMT   fn_str(FnLangRu, FnStrErrorCodeFmt)
#define FN_STR_BUSY             fn_str(FnLangRu, FnStrBusy)
#define FN_STR_PORT_BUSY        fn_str(FnLangRu, FnStrPortBusy)

#define FN_STR_CONFIRM_BODY   fn_str(FnLangRu, FnStrConfirmBody)
#define FN_STR_CONFIRM_ACCEPT fn_str(FnLangRu, FnStrConfirmAccept)
#define FN_STR_CONFIRM_CANCEL fn_str(FnLangRu, FnStrConfirmCancel)

#define FN_STR_ABOUT_APP           fn_str(FnLangRu, FnStrAboutApp)
#define FN_STR_ABOUT_AUTHOR        fn_str(FnLangRu, FnStrAboutAuthor)
#define FN_STR_ABOUT_VERSION_FMT   fn_str(FnLangRu, FnStrAboutVersionFmt)
#define FN_STR_ABOUT_URL_1         fn_str(FnLangRu, FnStrAboutUrl1)
#define FN_STR_ABOUT_URL_2         fn_str(FnLangRu, FnStrAboutUrl2)
#define FN_STR_ABOUT_DESCRIPTION_1 fn_str(FnLangRu, FnStrAboutDescription1)
#define FN_STR_ABOUT_DESCRIPTION_2 fn_str(FnLangRu, FnStrAboutDescription2)
#define FN_STR_ABOUT_DESCRIPTION_3 fn_str(FnLangRu, FnStrAboutDescription3)
#define FN_STR_ABOUT_DESCRIPTION_4 fn_str(FnLangRu, FnStrAboutDescription4)

#define FN_STR_LANGUAGE_HINT    fn_str(FnLangRu, FnStrLanguageHint)
#define FN_STR_LANGUAGE_ENGLISH fn_str(FnLangRu, FnStrLanguageEnglish)
#define FN_STR_LANGUAGE_RUSSIAN fn_str(FnLangRu, FnStrLanguageRussian)

#ifdef __cplusplus
}
#endif

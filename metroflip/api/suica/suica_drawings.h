#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include "metroflip_i.h"
#include <flipper_application.h>
#include "../metroflip/metroflip_api.h"
#include "suica_assets.h"

#include <lib/nfc/protocols/felica/felica.h>
#include <lib/nfc/protocols/felica/felica_poller.h>
#include <lib/bit_lib/bit_lib.h>
#include "suica_font.h"

#define SUICA_STATION_LIST_PATH     APP_ASSETS_PATH("suica/")
#define SUICA_IC_TYPE_CODE          0x31
#define SERVICE_CODE_HISTORY_IN_LE  (0x090FU)
#define SERVICE_CODE_TAPS_LOG_IN_LE (0x108FU)
#define SERVICE_CODE_OCTOPUS_IN_LE  (0x0117U)
#define BLOCK_COUNT                 1
#define HISTORY_VIEW_PAGE_NUM       3

#define TERMINAL_NULL                     0x02
#define TERMINAL_FARE_ADJUST_MACHINE      0x03
#define TERMINAL_PORTABLE                 0x04
#define TERMINAL_BUS                      0x05
#define TERMINAL_CARD_VENDING_MACHINE_1   0x07
#define TERMINAL_TICKET_VENDING_MACHINE   0x08
#define TERMINAL_QUICK_REFILL_MACHINE     0x09
#define TERMINAL_CARD_VENDING_MACHINE_2   0x12
#define TERMINAL_TICKET_MACHINE_TYPE_1    0x13
#define TERMINAL_PASMO_ISSUE_MACHINE      0x14
#define TERMINAL_COMMUTER_PASS_VENDING    0x15
#define TERMINAL_AUTO_TICKET_GATE         0x16
#define TERMINAL_SIMPLE_TICKET_GATE       0x17
#define TERMINAL_STATION_EQUIPMENT        0x18
#define TERMINAL_GREEN_WINDOW             0x19
#define TERMINAL_TICKET_OFFICE            0x1A
#define TERMINAL_MOBILE_PHONE             0x1B
#define TERMINAL_CONNECT_ADJUST_MACHINE   0x1C
#define TERMINAL_TRANSFER_ADJUST_MACHINE  0x1D
#define TERMINAL_KANSAI_DEPOSIT_MACHINE   0x1F
#define TERMINAL_WINDOW_TERMINAL_MEITETSU 0x20
#define TERMINAL_CHECKOUT_MACHINE         0x21
#define TERMINAL_KOTODEN_TICKET_GATE      0x22
#define TERMINAL_IN_CAR_SUPP_MACHINE      0x24
#define TERMINAL_SETAMARU_VENDING         0x34
#define TERMINAL_SETAMARU_IN_CAR_MACHINE  0x35
#define TERMINAL_IN_CAR_MACHINE           0x36
#define TERMINAL_VIEW_ALTTE_ATM_TYPE_1    0x46
#define TERMINAL_VIEW_ALTTE_ATM_TYPE_2    0x48
#define TERMINAL_POS                      0xC7
#define TERMINAL_VENDING_MACHINE          0xC8

#define PROCESSING_AUTO_GATE_FARE               0x01
#define PROCESSING_TOP_UP                       0x02
#define PROCESSING_TICKET_PURCHASE              0x03
#define PROCESSING_TICKET_SETTLEMENT            0x04
#define PROCESSING_OVER_TRAVEL_ADJUST           0x05
#define PROCESSING_MANNED_GATE_EXIT             0x06
#define PROCESSING_NEW_ISSUE                    0x07
#define PROCESSING_MANNED_GATE_DEDUCT           0x08
#define PROCESSING_BUS_PITAPA_FLAT              0x0D
#define PROCESSING_BUS_IRUCA_GENERAL            0x0F
#define PROCESSING_REISSUE                      0x11
#define PROCESSING_EXIT_PAYMENT                 0x13
#define PROCESSING_AUTO_TOP_UP                  0x14
#define PROCESSING_BUS_TOP_UP                   0x1F
#define PROCESSING_SPECIAL_BUS_TICKET           0x23
#define PROCESSING_MERCHANDISE_POS              0x46
#define PROCESSING_BONUS_TOP_UP                 0x48
#define PROCESSING_REGISTER_TOP_UP              0x49
#define PROCESSING_CANCEL_POS                   0x4A
#define PROCESSING_ENTRY_AND_POS                0x4B
#define PROCESSING_THIRD_PARTY_PAYMENT          0x84
#define PROCESSING_THIRD_PARTY_ADMISSION        0x85
#define PROCESSING_POS_WITH_PART_CASH           0xC6
#define PROCESSING_ENTRY_AND_POS_WITH_PART_CASH 0xCB

#define ARROW_ANIMATION_FRAME_MS 350

typedef enum {
    SuicaTrainRideEntry,
    SuicaTrainRideExit,
} SuicaTrainRideType;

typedef enum {
    SuicaRedrawScreen,
} SuicaCustomEvent;

static void suica_draw_train_page_1(
    Canvas* canvas,
    SuicaHistory history,
    SuicaHistoryViewModel* model,
    SuicaHistoryType history_type) {
    // Separator
    canvas_draw_icon(canvas, 0, 37, &I_Suica_DashLine);

    // Arrow
    uint8_t arrow_bits[4] = {0b1000, 0b0100, 0b0010, 0b0001};

    // Arrow
    if(model->animator_tick > 3) {
        // 4 steps of animation
        model->animator_tick = 0;
    }
    uint8_t current_arrow_bits = arrow_bits[model->animator_tick];
    canvas_draw_icon(
        canvas,
        110,
        19,
        (current_arrow_bits & 0b1000) ? &I_Suica_FilledArrowDown : &I_Suica_EmptyArrowDown);
    canvas_draw_icon(
        canvas,
        110,
        29,
        (current_arrow_bits & 0b0100) ? &I_Suica_FilledArrowDown : &I_Suica_EmptyArrowDown);
    canvas_draw_icon(
        canvas,
        110,
        39,
        (current_arrow_bits & 0b0010) ? &I_Suica_FilledArrowDown : &I_Suica_EmptyArrowDown);
    canvas_draw_icon(
        canvas,
        110,
        49,
        (current_arrow_bits & 0b0001) ? &I_Suica_FilledArrowDown : &I_Suica_EmptyArrowDown);

    // Entry logo
    switch(history.entry_line.type) {
    case SuicaKeikyu:
        canvas_draw_icon(canvas, 2, 11, &I_Suica_KeikyuLogo);
        break;
    case SuicaJREast:
    case SuicaJRCentral:
    case SuicaJRWest:
    case SuicaJRKyushu:
        canvas_draw_icon(canvas, 1, 12, &I_Suica_JRLogo);
        break;
    case SuicaMobile:
        canvas_draw_icon(canvas, 4, 15, &I_Suica_MobileLogo);
        break;
    case SuicaTokyoMetro:
        canvas_draw_icon(canvas, 2, 12, &I_Suica_TokyoMetroLogo);
        break;
    case SuicaToei:
        canvas_draw_icon(canvas, 4, 11, &I_Suica_ToeiLogo);
        break;
    case SuicaTWR:
        canvas_draw_icon(canvas, 0, 12, &I_Suica_TWRLogo);
        break;
    case SuicaYurikamome:
        canvas_draw_icon(canvas, 0, 12, &I_Suica_YurikamomeLogo);
        break;
    case SuicaTokyoMonorail:
        canvas_draw_icon(canvas, 0, 11, &I_Suica_TokyoMonorailLogo);
        break;
    case SuicaTokyu:
        canvas_draw_icon(canvas, 3, 11, &I_Suica_TokyuLogo);
        break;
    case SuicaOsakaMetro:
        canvas_draw_icon(canvas, 0, 11, &I_Suica_OsakaMetroLogo);
        break;
    case SuicaRailwayTypeMax:
        canvas_draw_icon(canvas, 5, 11, &I_Suica_QuestionMarkSmall);
        break;
    default:
        break;
    }

    // Entry Text
    if(history.entry_line.type == SuicaMobile) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 28, 28, "Mobile Suica");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 26, 23, history.entry_line.long_name);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 34, furi_string_get_cstr(history.entry_station.name));
    }

    switch(history_type) {
    case SuicaHistoryTrain:
        // Exit Train
        switch(history.exit_line.type) {
        case SuicaKeikyu:
            canvas_draw_icon(canvas, 2, 39, &I_Suica_KeikyuLogo);
            break;
        case SuicaJREast:
        case SuicaJRCentral:
        case SuicaJRWest:
        case SuicaJRKyushu:
            canvas_draw_icon(canvas, 1, 40, &I_Suica_JRLogo);
            break;
        case SuicaTokyoMetro:
            canvas_draw_icon(canvas, 2, 40, &I_Suica_TokyoMetroLogo);
            break;
        case SuicaToei:
            canvas_draw_icon(canvas, 4, 39, &I_Suica_ToeiLogo);
            break;
        case SuicaTWR:
            canvas_draw_icon(canvas, 0, 40, &I_Suica_TWRLogo);
            break;
        case SuicaYurikamome:
            canvas_draw_icon(canvas, 0, 40, &I_Suica_YurikamomeLogo);
            break;
        case SuicaTokyoMonorail:
            canvas_draw_icon(canvas, 0, 39, &I_Suica_TokyoMonorailLogo);
            break;
        case SuicaTokyu:
            canvas_draw_icon(canvas, 3, 39, &I_Suica_TokyuLogo);
            break;
        case SuicaOsakaMetro:
            canvas_draw_icon(canvas, 0, 39, &I_Suica_OsakaMetroLogo);
            break;
        case SuicaRailwayTypeMax:
            canvas_draw_icon(canvas, 5, 39, &I_Suica_QuestionMarkSmall);
            break;
        default:
            break;
        }

        // Exit Text
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 26, 51, history.exit_line.long_name);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 62, furi_string_get_cstr(history.exit_station.name));
        break;

    case SuicaHistoryHappyBirthday:
        // Birthday
        canvas_draw_icon(canvas, 5, 42, &I_Suica_CrackingEgg);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 28, 56, "Card issued");
        break;
    case SuicaHistoryTopUp:
        // Top Up
        switch(model->animator_tick) {
        case 0:
            canvas_draw_icon(canvas, 1, 39, &I_Suica_MoneyStack1);
            break;
        case 1:
            canvas_draw_icon(canvas, 1, 39, &I_Suica_MoneyStack2);
            break;
        case 2:
            canvas_draw_icon(canvas, 1, 39, &I_Suica_MoneyStack3);
            break;
        case 3:
            canvas_draw_icon(canvas, 1, 39, &I_Suica_MoneyStack4);
            break;
        default:
            break;
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 26, 55, "Top Up");
        break;
    default:
        canvas_draw_str(canvas, 5, 56, "Error: draw train incorrectly called");
        break;
    }
}

static void
    suica_draw_train_page_2(Canvas* canvas, SuicaHistory history, SuicaHistoryViewModel* model) {
    FuriString* buffer = furi_string_alloc();

    // Entry
    switch(history.entry_line.type) {
    case SuicaKeikyu:
        canvas_draw_disc(canvas, 24, 38, 24);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, 24, 38, 21);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_icon(canvas, 16, 24, history.entry_line.logo_icon);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.entry_station.station_number);
        canvas_draw_str(canvas, 13, 54, furi_string_get_cstr(buffer));
        break;
    case SuicaTokyoMonorail:
        canvas_draw_rbox(canvas, 8, 22, 34, 34, 7);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 12, 26, 26, 26);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 25, 35, AlignCenter, AlignBottom, history.entry_line.short_name);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.entry_station.station_number);
        canvas_draw_str(canvas, 14, 51, furi_string_get_cstr(buffer));
        break;
    case SuicaJREast:
        if(!furi_string_equal_str(history.entry_station.jr_header, "0")) {
            canvas_draw_rbox(canvas, 4, 13, 42, 51, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas,
                25,
                24,
                AlignCenter,
                AlignBottom,
                furi_string_get_cstr(history.entry_station.jr_header));
            canvas_draw_rbox(canvas, 8, 26, 34, 34, 7);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, 12, 30, 26, 26);
            canvas_set_font(canvas, FontKeyboard);
            canvas_draw_str_aligned(
                canvas, 25, 39, AlignCenter, AlignBottom, history.entry_line.short_name);
            canvas_set_font(canvas, FontBigNumbers);
            furi_string_printf(buffer, "%02d", history.entry_station.station_number);
            canvas_draw_str(canvas, 14, 54, furi_string_get_cstr(buffer));
        } else {
            canvas_draw_rframe(canvas, 8, 22, 34, 34, 7);
            canvas_draw_frame(canvas, 12, 26, 26, 26);
            canvas_set_font(canvas, FontKeyboard);
            canvas_draw_str_aligned(
                canvas, 25, 35, AlignCenter, AlignBottom, history.entry_line.short_name);
            canvas_set_font(canvas, FontBigNumbers);
            furi_string_printf(buffer, "%02d", history.entry_station.station_number);
            canvas_draw_str(canvas, 14, 50, furi_string_get_cstr(buffer));
        }
        break;
    case SuicaJRCentral:
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 8, 23, 34, 33);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas,
            24 + history.entry_line.logo_offset[0],
            33 + history.entry_line.logo_offset[1],
            AlignCenter,
            AlignBottom,
            history.entry_line.short_name);
        canvas_draw_box(canvas, 11, 35, 28, 18);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.entry_station.station_number);
        canvas_draw_str(canvas, 14, 51, furi_string_get_cstr(buffer));
        break;
    case SuicaJRWest:
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 7, 18, 36, 36);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_custom_u8g2_font(canvas, u8g2_font_JRWest15);
        canvas_draw_str(
            canvas,
            18 + history.entry_line.logo_offset[0],
            36 + history.entry_line.logo_offset[1],
            history.entry_line.short_name);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.entry_station.station_number);
        canvas_draw_str(canvas, 14, 52, furi_string_get_cstr(buffer));
        canvas_set_color(canvas, ColorBlack);
        break;
    case SuicaTokyoMetro:
    case SuicaToei:
        canvas_draw_disc(canvas, 24, 38, 24);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, 24, 38, 19);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_icon(
            canvas,
            17 + history.entry_line.logo_offset[0],
            22 + history.entry_line.logo_offset[1],
            history.entry_line.logo_icon);
        furi_string_printf(buffer, "%02d", history.entry_station.station_number);
        canvas_draw_str(canvas, 13, 53, furi_string_get_cstr(buffer));
        break;
    case SuicaTWR:
        canvas_draw_circle(canvas, 24, 38, 24);
        canvas_draw_circle(canvas, 24, 38, 20);
        canvas_draw_disc(canvas, 24, 38, 18);

        // supplement the circle
        canvas_draw_line(canvas, 39, 49, 35, 53);
        canvas_draw_line(canvas, 13, 23, 9, 27);
        canvas_draw_line(canvas, 39, 27, 35, 23);
        canvas_draw_line(canvas, 13, 53, 9, 49);

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, 21, 23, history.entry_line.logo_icon);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.entry_station.station_number);
        canvas_draw_str(canvas, 13, 53, furi_string_get_cstr(buffer));
        canvas_set_color(canvas, ColorBlack);
        break;
    case SuicaYurikamome:
        canvas_draw_circle(canvas, 24, 38, 24);
        canvas_draw_disc(canvas, 24, 38, 21);

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, 20, 22, history.entry_line.logo_icon);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.entry_station.station_number);
        canvas_draw_str(canvas, 14, 53, furi_string_get_cstr(buffer));
        canvas_set_color(canvas, ColorBlack);
        break;
    case SuicaTokyu:
        canvas_draw_rbox(canvas, 8, 21, 34, 34, 7);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 24, 33, AlignCenter, AlignBottom, history.entry_line.short_name);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.entry_station.station_number);
        canvas_draw_str(canvas, 14, 51, furi_string_get_cstr(buffer));
        canvas_set_color(canvas, ColorBlack);
        break;
    case SuicaRailwayTypeMax:
        canvas_draw_circle(canvas, 24, 38, 24);
        canvas_draw_circle(canvas, 24, 38, 19);
        canvas_draw_icon(canvas, 14, 22, &I_Suica_QuestionMarkBig);
        break;
    default:
        break;
    }

    // Exit
    switch(history.exit_line.type) {
    case SuicaKeikyu:
        canvas_draw_disc(canvas, 103, 38, 24);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, 103, 38, 21);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_icon(canvas, 95, 24, history.exit_line.logo_icon);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.exit_station.station_number);
        canvas_draw_str(canvas, 92, 54, furi_string_get_cstr(buffer));
        break;
    case SuicaTokyoMonorail:
        canvas_draw_rbox(canvas, 85, 22, 34, 34, 7);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 89, 26, 26, 26);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 102, 35, AlignCenter, AlignBottom, history.exit_line.short_name);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.exit_station.station_number);
        canvas_draw_str(canvas, 91, 51, furi_string_get_cstr(buffer));
        break;
    case SuicaJREast:
        if(!furi_string_equal_str(history.exit_station.jr_header, "0")) {
            canvas_draw_rbox(canvas, 81, 13, 42, 51, 10);
            canvas_set_color(canvas, ColorWhite);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas,
                101,
                24,
                AlignCenter,
                AlignBottom,
                furi_string_get_cstr(history.exit_station.jr_header));
            canvas_draw_rbox(canvas, 85, 26, 34, 34, 7);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, 89, 30, 26, 26);
            canvas_set_font(canvas, FontKeyboard);
            canvas_draw_str_aligned(
                canvas, 102, 39, AlignCenter, AlignBottom, history.exit_line.short_name);
            canvas_set_font(canvas, FontBigNumbers);
            furi_string_printf(buffer, "%02d", history.exit_station.station_number);
            canvas_draw_str(canvas, 91, 54, furi_string_get_cstr(buffer));
        } else {
            canvas_draw_rframe(canvas, 85, 22, 34, 34, 7);
            canvas_draw_frame(canvas, 89, 26, 26, 26);
            canvas_set_font(canvas, FontKeyboard);
            canvas_draw_str_aligned(
                canvas, 102, 35, AlignCenter, AlignBottom, history.exit_line.short_name);
            canvas_set_font(canvas, FontBigNumbers);
            furi_string_printf(buffer, "%02d", history.exit_station.station_number);
            canvas_draw_str(canvas, 91, 50, furi_string_get_cstr(buffer));
        }
        break;
    case SuicaJRCentral:
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 86, 23, 34, 33);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas,
            102 + history.exit_line.logo_offset[0],
            33 + history.exit_line.logo_offset[1],
            AlignCenter,
            AlignBottom,
            history.exit_line.short_name);
        canvas_draw_box(canvas, 89, 35, 28, 18);
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.exit_station.station_number);
        canvas_draw_str(canvas, 92, 51, furi_string_get_cstr(buffer));
        break;
    case SuicaJRWest:
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 85, 18, 36, 36);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_custom_u8g2_font(canvas, u8g2_font_JRWest15);
        canvas_draw_str(
            canvas,
            96 + history.exit_line.logo_offset[0],
            36 + history.exit_line.logo_offset[1],
            history.exit_line.short_name);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.exit_station.station_number);
        canvas_draw_str(canvas, 92, 52, furi_string_get_cstr(buffer));
        canvas_set_color(canvas, ColorBlack);
        break;
    case SuicaTokyoMetro:
    case SuicaToei:
        canvas_draw_disc(canvas, 103, 38, 24);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_disc(canvas, 103, 38, 19);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_icon(
            canvas,
            96 + history.exit_line.logo_offset[0],
            22 + history.exit_line.logo_offset[1],
            history.exit_line.logo_icon);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.exit_station.station_number);
        canvas_draw_str(canvas, 92, 53, furi_string_get_cstr(buffer));
        break;
    case SuicaTWR:
        canvas_draw_circle(canvas, 103, 38, 24);
        canvas_draw_circle(canvas, 103, 38, 20);
        canvas_draw_disc(canvas, 103, 38, 18);

        // supplement the circle
        canvas_draw_line(canvas, 118, 49, 114, 53);
        canvas_draw_line(canvas, 118, 27, 114, 23);
        canvas_draw_line(canvas, 92, 23, 88, 27);
        canvas_draw_line(canvas, 92, 53, 88, 49);

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, 100, 23, history.exit_line.logo_icon);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.exit_station.station_number);
        canvas_draw_str(canvas, 92, 53, furi_string_get_cstr(buffer));
        canvas_set_color(canvas, ColorBlack);
        break;
    case SuicaYurikamome:
        canvas_draw_circle(canvas, 103, 38, 24);
        canvas_draw_disc(canvas, 103, 38, 21);

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_icon(canvas, 99, 22, history.exit_line.logo_icon);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.exit_station.station_number);
        canvas_draw_str(canvas, 93, 53, furi_string_get_cstr(buffer));
        canvas_set_color(canvas, ColorBlack);
        break;
    case SuicaTokyu:
        canvas_draw_rbox(canvas, 86, 21, 34, 34, 7);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 103, 33, AlignCenter, AlignBottom, history.exit_line.short_name);
        canvas_set_font(canvas, FontBigNumbers);
        furi_string_printf(buffer, "%02d", history.exit_station.station_number);
        canvas_draw_str(canvas, 92, 51, furi_string_get_cstr(buffer));
        canvas_set_color(canvas, ColorBlack);
        break;
    case SuicaRailwayTypeMax:
        canvas_draw_circle(canvas, 103, 38, 24);
        canvas_draw_circle(canvas, 103, 38, 19);
        canvas_draw_icon(canvas, 93, 22, &I_Suica_QuestionMarkBig);
    default:
        break;
    }

    uint8_t arrow_bits[3] = {0b100, 0b010, 0b001};

    // Arrow
    if(model->animator_tick > 2) {
        // 4 steps of animation
        model->animator_tick = 0;
    }
    uint8_t current_arrow_bits = arrow_bits[model->animator_tick];
    canvas_draw_icon(
        canvas,
        51,
        32,
        (current_arrow_bits & 0b100) ? &I_Suica_FilledArrowRight : &I_Suica_EmptyArrowRight);
    canvas_draw_icon(
        canvas,
        59,
        32,
        (current_arrow_bits & 0b010) ? &I_Suica_FilledArrowRight : &I_Suica_EmptyArrowRight);
    canvas_draw_icon(
        canvas,
        67,
        32,
        (current_arrow_bits & 0b001) ? &I_Suica_FilledArrowRight : &I_Suica_EmptyArrowRight);

    furi_string_free(buffer);
}

static void
    suica_draw_birthday_page_2(Canvas* canvas, SuicaHistory history, SuicaHistoryViewModel* model) {
    UNUSED(history);
    canvas_draw_icon(canvas, 27, 14, &I_Suica_PenguinHappyBirthday);
    canvas_draw_icon(canvas, 14, 14, &I_Suica_PenguinTodaysVIP);
    canvas_draw_rframe(canvas, 12, 12, 13, 52, 2); // VIP frame
    uint8_t intertic_bits[4] = {0b11000000, 0b11110000, 0b11111111, 0b00000000};

    // Arrow
    if(model->animator_tick > 3) {
        // 4 steps of animation
        model->animator_tick = 0;
    }
    uint8_t current_intertic_bits = intertic_bits[model->animator_tick];
    canvas_draw_icon(
        canvas, 87, 30, (current_intertic_bits & 0b10000000) ? &I_Suica_BigStar : &I_Suica_Nothing);
    canvas_draw_icon(
        canvas, 90, 12, (current_intertic_bits & 0b01000000) ? &I_Suica_PlusStar : &I_Suica_Nothing);
    canvas_draw_icon(
        canvas, 99, 34, (current_intertic_bits & 0b00100000) ? &I_Suica_SmallStar : &I_Suica_Nothing);
    canvas_draw_icon(
        canvas, 103, 12, (current_intertic_bits & 0b00010000) ? &I_Suica_SmallStar : &I_Suica_Nothing);
    canvas_draw_icon(
        canvas, 106, 21, (current_intertic_bits & 0b00001000) ? &I_Suica_BigStar : &I_Suica_Nothing);
    canvas_draw_icon(
        canvas, 109, 43, (current_intertic_bits & 0b00000100) ? &I_Suica_PlusStar : &I_Suica_Nothing);
    canvas_draw_icon(
        canvas, 117, 28, (current_intertic_bits & 0b00000010) ? &I_Suica_BigStar : &I_Suica_Nothing);
    canvas_draw_icon(
        canvas, 115, 16, (current_intertic_bits & 0b00000100) ? &I_Suica_PlusStar : &I_Suica_Nothing);
}

static void suica_draw_vending_machine_page_1(
    Canvas* canvas,
    SuicaHistory history,
    SuicaHistoryViewModel* model) {
    FuriString* buffer = furi_string_alloc();
    // Animate Bubbles and LCD Refresh
    if(model->animator_tick > 13) {
        // 14 steps of animation
        model->animator_tick = 0;
    }
    switch(model->animator_tick) {
    case 0:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full1);
        break;
    case 1:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full2);
        break;
    case 2:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full3);
        break;
    case 3:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full4);
        break;
    case 4:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full5);
        break;
    case 5:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full6);
        break;
    case 6:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full7);
        break;
    case 7:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full8);
        break;
    case 8:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full9);
        break;
    case 9:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full10);
        break;
    case 10:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full11);
        break;
    case 11:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full12);
        break;
    case 12:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full13);
        break;
    case 13:
        canvas_draw_icon(canvas, 0, 10, &I_Suica_VendingPage2Full14);
        break;
    default:
        break;
    }

    furi_string_printf(buffer, "%d", history.balance_change);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 98, 39, AlignRight, AlignBottom, furi_string_get_cstr(buffer));
    furi_string_free(buffer);
}

static void suica_draw_vending_machine_page_2(
    Canvas* canvas,
    SuicaHistory history,
    SuicaHistoryViewModel* model) {
    FuriString* buffer = furi_string_alloc();

    if(model->animator_tick > 41) {
        // 6 steps of animation
        model->animator_tick = 0;
    }

    // Draw Thank You Banner
    canvas_draw_icon(
        canvas, 49 - model->animator_tick, -9 + model->animator_tick, &I_Suica_VendingThankYou);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 50, 0, 128, 64);
    canvas_draw_box(canvas, 0, 0, 42, 64);

    // Clock Component
    canvas_set_color(canvas, ColorWhite); // Erase part of old frame to allow for new frame
    canvas_draw_line(canvas, 93, 9, 96, 6);
    canvas_draw_line(canvas, 59, 9, 95, 9);
    canvas_set_color(canvas, ColorBlack);
    furi_string_printf(buffer, "%02d:%02d", history.hour, history.minute);
    canvas_draw_line(canvas, 65, 21, 62, 18);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 65, 19, furi_string_get_cstr(buffer));
    canvas_draw_line(canvas, 93, 21, 96, 18);
    canvas_draw_line(canvas, 66, 21, 93, 21);
    canvas_draw_line(canvas, 96, 6, 96, 17);
    canvas_draw_line(canvas, 62, 12, 62, 17);
    canvas_draw_line(canvas, 62, 12, 59, 9);

    // Vending Machine
    switch(model->animator_tick % 6) {
    case 0:
        canvas_draw_icon(canvas, 4, 12, &I_Suica_VendingMachine1);
        break;
    case 1:
        canvas_draw_icon(canvas, 4, 12, &I_Suica_VendingMachine2);
        break;
    case 2:
        canvas_draw_icon(canvas, 4, 12, &I_Suica_VendingMachine3);
        break;
    case 3:
        canvas_draw_icon(canvas, 4, 12, &I_Suica_VendingMachine4);
        break;
    case 4:
        canvas_draw_icon(canvas, 4, 12, &I_Suica_VendingMachine5);
        break;
    case 5:
        canvas_draw_icon(canvas, 4, 12, &I_Suica_VendingMachine6);
        break;
    default:
        canvas_draw_icon(canvas, 4, 12, &I_Suica_VendingMachine1);
        break;
    }

    // Machine Code
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 75, 35, "Machine");
    canvas_draw_icon(canvas, 119, 25, &I_Suica_ShopPin);
    furi_string_printf(
        buffer, "%01d:%03d:%03d", history.area_code, history.shop_code[0], history.shop_code[1]);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 75, 45, furi_string_get_cstr(buffer));

    // Animate Vending Machine Flap

    switch(model->animator_tick % 7) {
    case 0:
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlapHollow);
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlap1);
        break;
    case 1:
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlapHollow);
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlap2);
        break;
    case 2:
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlapHollow);
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlap3);
        break;
    case 3:
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlapHollow);
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlap3);
        canvas_draw_icon(canvas, 59, 45, &I_Suica_VendingCan1);
        break;
    case 4:
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlapHollow);
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlap2);
        canvas_draw_icon(canvas, 74, 48, &I_Suica_VendingCan2);
        break;
    case 5:
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlapHollow);
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlap1);
        canvas_draw_icon(canvas, 89, 51, &I_Suica_VendingCan3);
        break;
    case 6:
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlapHollow);
        canvas_draw_icon(canvas, 44, 40, &I_Suica_VendingFlap1);
        canvas_draw_icon(canvas, 110, 54, &I_Suica_VendingCan4);
        break;
    default:
        break;
    }
    furi_string_free(buffer);
}

static void
    suica_draw_store_page_1(Canvas* canvas, SuicaHistory history, SuicaHistoryViewModel* model) {
    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "%d", history.balance_change);
    canvas_draw_icon(canvas, 0, 15, &I_Suica_StoreP1Counter);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 98, 39, AlignRight, AlignBottom, furi_string_get_cstr(buffer));
    canvas_draw_icon(canvas, 59, 27, &I_Suica_StoreReceiptDashLine);
    // Animate Taxi and LCD Refresh
    if(model->animator_tick > 11) {
        // 14 steps of animation
        model->animator_tick = 0;
    }

    switch(model->animator_tick % 6) {
    case 0:
    case 1:
    case 2:
        canvas_draw_icon(canvas, 41, 18, &I_Suica_StoreReceiptFrame1);
        break;
    case 3:
    case 4:
    case 5:
        canvas_draw_icon(canvas, 41, 18, &I_Suica_StoreReceiptFrame2);
        break;
    default:
        break;
    }

    switch(model->animator_tick % 6) {
    case 0:
    case 1:
        canvas_draw_icon(canvas, 0, 24, &I_Suica_StoreLightningVertical);
        break;
    case 2:
    case 3:
        canvas_draw_icon(canvas, 3, 31, &I_Suica_StoreLightningHorizontal);
        break;
    case 4:
    case 5:
        canvas_draw_icon(canvas, 0, 24, &I_Suica_StoreLightningVertical);
        canvas_draw_icon(canvas, 3, 31, &I_Suica_StoreLightningHorizontal);
        break;
    default:
        break;
    }
    furi_string_free(buffer);
}

static void
    suica_draw_store_page_2(Canvas* canvas, SuicaHistory history, SuicaHistoryViewModel* model) {
    FuriString* buffer = furi_string_alloc();
    // Clock Component
    canvas_set_color(canvas, ColorWhite); // Erase part of old frame to allow for new frame
    canvas_draw_line(canvas, 93, 9, 96, 6);
    canvas_draw_line(canvas, 59, 9, 95, 9);
    canvas_set_color(canvas, ColorBlack);
    furi_string_printf(buffer, "%02d:%02d", history.hour, history.minute);
    canvas_draw_line(canvas, 65, 21, 62, 18);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 65, 19, furi_string_get_cstr(buffer));
    canvas_draw_line(canvas, 93, 21, 96, 18);
    canvas_draw_line(canvas, 66, 21, 93, 21);
    canvas_draw_line(canvas, 96, 6, 96, 17);
    canvas_draw_line(canvas, 62, 12, 62, 17);
    canvas_draw_line(canvas, 62, 12, 59, 9);

    // Machine Code
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 75, 35, "Store");
    canvas_draw_icon(canvas, 104, 25, &I_Suica_ShopPin);
    furi_string_printf(
        buffer, "%01d:%03d:%03d", history.area_code, history.shop_code[0], history.shop_code[1]);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 75, 45, furi_string_get_cstr(buffer));

    // Store Frame
    canvas_draw_icon(canvas, 0, 13, &I_Suica_StoreFrame);
    // Sliding Door
    uint8_t door_position[7] = {20, 18, 14, 6, 2, 0, 0};
    if(model->animator_tick > 20) {
        // 14 steps of animation
        model->animator_tick = 0;
    }

    if(model->animator_tick < 7) {
        canvas_draw_icon(
            canvas, -1 - door_position[6 - model->animator_tick], 28, &I_Suica_StoreSlidingDoor);
    } else if(model->animator_tick < 14) {
        canvas_draw_icon(
            canvas, -1 - door_position[model->animator_tick - 7], 28, &I_Suica_StoreSlidingDoor);
    } else {
        canvas_draw_icon(canvas, -1, 28, &I_Suica_StoreSlidingDoor);
    }

    // Animate Neon and Fan
    switch(model->animator_tick % 4) {
    case 0:
    case 1:
        canvas_draw_icon(canvas, 37, 18, &I_Suica_StoreFan1);
        break;
    case 2:
    case 3:
        canvas_draw_icon(canvas, 37, 18, &I_Suica_StoreFan2);
        break;
    default:
        break;
    }
    furi_string_free(buffer);
}

static void
    suica_draw_top_up_page_2(Canvas* canvas, SuicaHistory history, SuicaHistoryViewModel* model) {
    FuriString* buffer = furi_string_alloc();
    furi_string_printf(buffer, "%d", history.balance_change);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 98, 40, AlignRight, AlignBottom, furi_string_get_cstr(buffer));
    furi_string_free(buffer);

    if(model->animator_tick > 26) {
        // 14 steps of animation
        model->animator_tick = 0;
    }
    switch(model->animator_tick) {
    case 0:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0001);
        break;
    case 1:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0002);
        break;
    case 2:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0003);
        break;
    case 3:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0004);
        break;
    case 4:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0005);
        break;
    case 5:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0006);
        break;
    case 6:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0007);
        break;
    case 7:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0008);
        break;
    case 8:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0009);
        break;
    case 9:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0010);
        break;
    case 10:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0011);
        break;
    case 11:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0012);
        break;
    case 12:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0013);
        break;
    case 13:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0014);
        break;
    case 14:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0015);
        break;
    case 15:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0016);
        break;
    case 16:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0017);
        break;
    case 17:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0018);
        break;
    case 18:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0019);
        break;
    case 19:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0020);
        break;
    case 20:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0021);
        break;
    case 21:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0022);
        break;
    case 22:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0023);
        break;
    case 23:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0024);
        break;
    case 24:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0025);
        break;
    case 25:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0026);
        break;
    case 26:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0027);
        break;
    default:
        canvas_draw_icon(canvas, 0, 0, &I_Suica_TopUpPage2_0001);
        break;
    }
}

static void
    suica_draw_balance_page(Canvas* canvas, SuicaHistory history, SuicaHistoryViewModel* model) {
    FuriString* buffer = furi_string_alloc();

    // Balance
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_icon(canvas, 0, 48, &I_Suica_YenSign);
    canvas_draw_icon(canvas, 111, 48, &I_Suica_YenKanji);

    furi_string_printf(buffer, "%d", history.balance);
    canvas_draw_str_aligned(
        canvas, 109, 64, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    furi_string_printf(buffer, "%d", history.previous_balance);
    canvas_draw_str_aligned(
        canvas, 109, 26, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    furi_string_printf(buffer, "%d", history.balance_change);
    canvas_draw_str_aligned(
        canvas, 109, 43, AlignRight, AlignBottom, furi_string_get_cstr(buffer));

    // Separator
    canvas_draw_line(canvas, 26, 45, 128, 45);
    canvas_draw_line(canvas, 26, 46, 128, 46);

    if(history.balance_sign == SuicaBalanceAdd) {
        // Animate plus sign
        if(model->animator_tick > 3) {
            // 9 steps of animation
            model->animator_tick = 0;
        }
        switch(model->animator_tick) {
        case 0:
            canvas_draw_icon(canvas, 28, 28, &I_Suica_PlusSign1);
            break;
        case 1:
        case 3:
            canvas_draw_icon(canvas, 27, 27, &I_Suica_PlusSign2);
            break;
        case 2:
            canvas_draw_icon(canvas, 26, 26, &I_Suica_PlusSign3);
            break;
        default:
            break;
        }
    } else if(history.balance_sign == SuicaBalanceSub) {
        // Animate plus sign
        if(model->animator_tick > 12) {
            // 9 steps of animation
            model->animator_tick = 0;
        }
        switch(model->animator_tick) {
        case 0:
        case 1:
        case 2:
        case 3:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign0);
            break;
        case 4:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign1);
            break;
        case 5:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign2);
            break;
        case 6:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign3);
            break;
        case 7:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign4);
            break;
        case 8:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign5);
            break;
        case 9:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign6);
            break;
        case 10:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign7);
            break;
        case 11:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign8);
            break;
        case 12:
            canvas_draw_icon(canvas, 28, 32, &I_Suica_MinusSign9);
            break;
        default:
            break;
        }
    } else {
        canvas_draw_str(canvas, 30, 28, "=");
    }
    furi_string_free(buffer);
}

static void suica_history_draw_callback(Canvas* canvas, void* model) {
    canvas_set_bitmap_mode(canvas, true);
    SuicaHistoryViewModel* my_model = (SuicaHistoryViewModel*)model;
    FuriString* buffer = furi_string_alloc();
    // catch the case where the page and entry are not initialized

    if(my_model->entry > my_model->size || my_model->entry < 1) {
        my_model->entry = 1;
    }

    // Get previous balance if we are not at the earliest entry
    if(my_model->entry < my_model->size) {
        my_model->history.previous_balance = my_model->travel_history[(my_model->entry * 16) + 10];
        my_model->history.previous_balance |= my_model->travel_history[(my_model->entry * 16) + 11]
                                              << 8;
    } else {
        my_model->history.previous_balance = 0;
    }
    // Calculate balance change
    if(my_model->history.previous_balance < my_model->history.balance) {
        my_model->history.balance_change =
            my_model->history.balance - my_model->history.previous_balance;
        my_model->history.balance_sign = SuicaBalanceAdd;
    } else if(my_model->history.previous_balance > my_model->history.balance) {
        my_model->history.balance_change =
            my_model->history.previous_balance - my_model->history.balance;
        my_model->history.balance_sign = SuicaBalanceSub;
    } else {
        my_model->history.balance_change = 0;
        my_model->history.balance_sign = SuicaBalanceEqual;
    }

    switch((uint8_t)my_model->page) {
    case 0:
        switch(my_model->history.history_type) {
        case SuicaHistoryTrain:
            suica_draw_train_page_1(canvas, my_model->history, my_model, SuicaHistoryTrain);
            break;
        case SuicaHistoryHappyBirthday:
            suica_draw_train_page_1(
                canvas, my_model->history, my_model, SuicaHistoryHappyBirthday);
            break;
        case SuicaHistoryVendingMachine:
            suica_draw_vending_machine_page_1(canvas, my_model->history, my_model);
            break;
        case SuicaHistoryPosAndTaxi:
            suica_draw_store_page_1(canvas, my_model->history, my_model);
            break;
        case SuicaHistoryTopUp:
            suica_draw_train_page_1(canvas, my_model->history, my_model, SuicaHistoryTopUp);
            break;
        default:
            break;
        }
        break;
    case 1:
        switch(my_model->history.history_type) {
        case SuicaHistoryTrain:
            suica_draw_train_page_2(canvas, my_model->history, my_model);
            break;
        case SuicaHistoryHappyBirthday:
            suica_draw_birthday_page_2(canvas, my_model->history, my_model);
            break;
        case SuicaHistoryVendingMachine:

            suica_draw_vending_machine_page_2(canvas, my_model->history, my_model);
            break;
        case SuicaHistoryPosAndTaxi:
            suica_draw_store_page_2(canvas, my_model->history, my_model);
            break;
        case SuicaHistoryTopUp:
            suica_draw_top_up_page_2(canvas, my_model->history, my_model);
            break;
        default:
            break;
        }
        break;
    case 2:
        suica_draw_balance_page(canvas, my_model->history, my_model);
        break;
    default:
        break;
    }

    // The the banner last so it is on top

    // Draw Top Left Icons
    canvas_draw_icon(canvas, 0, 0, &I_Suica_CardIcon);
    switch(my_model->history.history_type) {
    case SuicaHistoryTrain:
        canvas_draw_icon(canvas, 20, 0, &I_Suica_TrainIcon);
        break;
    case SuicaHistoryHappyBirthday:
        canvas_draw_icon(canvas, 20, 0, &I_Suica_BdayCakeIcon);
        break;
    case SuicaHistoryVendingMachine:
        canvas_draw_icon(canvas, 21, 0, &I_Suica_VendingIcon);
        break;
    case SuicaHistoryPosAndTaxi:
        canvas_draw_icon(canvas, 19, 0, &I_Suica_ShopIcon);
        break;
    case SuicaHistoryTopUp:
        canvas_draw_icon(canvas, 20, 0, &I_Suica_TopUpIcon);
        break;
    default:
        canvas_draw_icon(canvas, 22, 0, &I_Suica_UnknownIcon);
        break;
    }

    // Date
    furi_string_printf(
        buffer,
        "20%02d-%02d-%02d",
        my_model->history.year,
        my_model->history.month,
        my_model->history.day);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 37, 8, furi_string_get_cstr(buffer));
    // Frame
    canvas_draw_line(canvas, 33, 0, 33, 6);
    canvas_draw_line(canvas, 36, 9, 33, 6);
    canvas_draw_line(canvas, 92, 9, 36, 9);
    canvas_draw_line(canvas, 93, 9, 96, 6);
    canvas_draw_line(canvas, 96, 0, 96, 6);

    // Entry Num
    canvas_draw_box(canvas, 106, 0, 13, 9);
    furi_string_printf(buffer, "%02d", my_model->entry);
    canvas_set_font(canvas, FontKeyboard);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, 107, 8, furi_string_get_cstr(buffer));
    canvas_set_color(canvas, ColorBlack);

    switch(my_model->page) {
    case 0:
        canvas_draw_icon(canvas, 100, 0, &I_Suica_EntrySlider1);
        break;
    case 1:
        canvas_draw_icon(canvas, 100, 0, &I_Suica_EntrySlider2);
        break;
    case 2:
        canvas_draw_icon(canvas, 100, 0, &I_Suica_EntrySlider3);
        break;
    default:
        break;
    }

    canvas_set_color(canvas, ColorWhite);
    if(my_model->entry == 1) {
        canvas_draw_box(canvas, 99, 0, 6, 9);
        canvas_set_color(canvas, ColorBlack);
        switch(my_model->page) {
        case 0:
            canvas_draw_icon(canvas, 101, 0, &I_Suica_EntryStopL1);
            break;
        case 1:
            canvas_draw_icon(canvas, 101, 0, &I_Suica_EntryStopL2);
            break;
        case 2:
            canvas_draw_icon(canvas, 101, 0, &I_Suica_EntryStopL3);
            break;
        default:
            break;
        }
    } else if(my_model->entry == my_model->size) {
        canvas_draw_box(canvas, 120, 0, 6, 9);
        canvas_set_color(canvas, ColorBlack);
        switch(my_model->page) {
        case 0:
            canvas_draw_icon(canvas, 120, 0, &I_Suica_EntryStopR1);
            break;
        case 1:
            canvas_draw_icon(canvas, 120, 0, &I_Suica_EntryStopR2);
            break;
        case 2:
            canvas_draw_icon(canvas, 120, 0, &I_Suica_EntryStopR3);
            break;
        default:
            break;
        }
    }

    canvas_set_color(canvas, ColorBlack);

    furi_string_free(buffer);
}

static void suica_view_history_timer_callback(void* context) {
    Metroflip* app = (Metroflip*)context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

static void suica_view_history_enter_callback(void* context) {
    uint32_t period = furi_ms_to_ticks(ARROW_ANIMATION_FRAME_MS);
    Metroflip* app = (Metroflip*)context;
    furi_assert(app->suica_context->timer == NULL);
    app->suica_context->timer =
        furi_timer_alloc(suica_view_history_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->suica_context->timer, period);
}

static void suica_view_history_exit_callback(void* context) {
    Metroflip* app = (Metroflip*)context;
    furi_timer_stop(app->suica_context->timer);
    furi_timer_free(app->suica_context->timer);
    app->suica_context->timer = NULL;
}

static bool suica_view_history_custom_event_callback(uint32_t event, void* context) {
    Metroflip* app = (Metroflip*)context;
    switch(event) {
    case 0:
        // Redraw screen by passing true to last parameter of with_view_model.
        {
            bool redraw = true;
            with_view_model(
                app->suica_context->view_history,
                SuicaHistoryViewModel * model,
                { model->animator_tick++; },
                redraw);
            return true;
        }
    default:
        return false;
    }
}

static uint32_t suica_navigation_raw_callback(void* _context) {
    UNUSED(_context);
    return MetroflipViewWidget;
}

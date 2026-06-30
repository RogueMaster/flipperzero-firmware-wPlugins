#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include "../../api/metroflip/metroflip_api.h"

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"


#include <datetime.h>
#include <furi/core/string.h>
#include <furi_hal_rtc.h>

#define TAG "Metroflip:Scene:Troika"


void render_section_header(
    FuriString* str,
    const char* name,
    uint8_t prefix_separator_cnt,
    uint8_t suffix_separator_cnt) {
    for(uint8_t i = 0; i < prefix_separator_cnt; i++) {
        furi_string_cat_printf(str, ":");
    }
    furi_string_cat_printf(str, "[ %s ]", name);
    for(uint8_t i = 0; i < suffix_separator_cnt; i++) {
        furi_string_cat_printf(str, ":");
    }
}

void from_days_to_datetime(uint32_t days, DateTime* datetime, uint16_t start_year) {
    uint32_t timestamp = days * 24 * 60 * 60;
    DateTime start_datetime = {0};
    start_datetime.year = start_year - 1;
    start_datetime.month = 12;
    start_datetime.day = 31;
    timestamp += datetime_datetime_to_timestamp(&start_datetime);
    datetime_timestamp_to_datetime(timestamp, datetime);
}

void from_minutes_to_datetime(uint32_t minutes, DateTime* datetime, uint16_t start_year) {
    uint32_t timestamp = minutes * 60;
    DateTime start_datetime = {0};
    start_datetime.year = start_year - 1;
    start_datetime.month = 12;
    start_datetime.day = 31;
    timestamp += datetime_datetime_to_timestamp(&start_datetime);
    datetime_timestamp_to_datetime(timestamp, datetime);
}

void from_seconds_to_datetime(uint32_t seconds, DateTime* datetime, uint16_t start_year) {
    uint32_t timestamp = seconds;
    DateTime start_datetime = {0};
    start_datetime.year = start_year - 1;
    start_datetime.month = 12;
    start_datetime.day = 31;
    timestamp += datetime_datetime_to_timestamp(&start_datetime);
    datetime_timestamp_to_datetime(timestamp, datetime);
}

typedef struct {
    uint16_t view; //101
    uint16_t type; //102
    uint8_t layout; //111
    uint8_t layout2; //112
    uint16_t blank_type; //121
    uint16_t type_of_extended; //122
    uint8_t extended; //123
    uint8_t benefit_code; //124
    uint32_t number; //201
    uint16_t use_before_date; //202
    uint16_t use_before_date2; //202.2
    uint16_t use_with_date; //205
    uint8_t requires_activation; //301
    uint16_t activate_during; //302
    uint16_t extension_counter; //304
    uint8_t blocked; //303
    uint32_t valid_from_date; //311
    uint16_t valid_to_date; //312
    uint8_t valid_for_days; //313
    uint32_t valid_for_minutes; //314
    uint16_t valid_for_time; //316
    uint16_t valid_for_time2; //316.2
    uint32_t valid_to_time; //317
    uint16_t remaining_trips; //321
    uint8_t remaining_trips1; //321.1
    uint32_t remaining_funds; //322
    uint16_t total_trips; //331
    uint16_t start_trip_date; //402
    uint16_t start_trip_time; //403
    uint32_t start_trip_neg_minutes; //404
    uint32_t start_trip_minutes; //405
    uint8_t start_trip_seconds; //406
    uint8_t minutes_pass; //412
    uint8_t passage_5_minutes; //413
    uint8_t metro_ride_with; //414
    uint8_t transport_type; //421
    uint8_t transport_type_flag; //421.0
    uint8_t transport_type1; //421.1
    uint8_t transport_type2; //421.2
    uint8_t transport_type3; //421.3
    uint8_t transport_type4; //421.4
    uint16_t validator; //422
    uint8_t validator1; //422.1
    uint16_t validator2; //422.2
    uint16_t route; //424
    uint8_t passage_in_metro; //431
    uint8_t transfer_in_metro; //432
    uint8_t passages_ground_transport; //433
    uint8_t fare_trip; //441
    uint16_t crc16; //501.1
    uint16_t crc16_2; //501.2
    uint32_t hash; //502
    uint16_t hash1; //502.1
    uint32_t hash2; //502.2
    uint8_t geozone_a; //GeoZoneA
    uint8_t geozone_b; //GeoZoneB
    uint8_t company; //Company
    uint8_t units; //Units
    uint64_t rfu1; //rfu1
    uint16_t rfu2; //rfu2
    uint32_t rfu3; //rfu3
    uint8_t rfu4; //rfu4
    uint8_t rfu5; //rfu5
    uint8_t write_enabled; //write_enabled
    uint32_t tech_code; //TechCode
    uint8_t interval; //Interval
    uint16_t app_code1; //AppCode1
    uint16_t app_code2; //AppCode2
    uint16_t app_code3; //AppCode3
    uint16_t app_code4; //AppCode4
    uint16_t type1; //Type1
    uint16_t type2; //Type2
    uint16_t type3; //Type3
    uint16_t type4; //Type4
    uint8_t zoo; //zoo
} BlockData;

void parse_layout_2(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x38, 16); //202
    data_block->benefit_code = bit_lib_get_bits(block->data, 0x48, 8); //124
    data_block->rfu1 = bit_lib_get_bits_32(block->data, 0x50, 32); //rfu1
    data_block->crc16 = bit_lib_get_bits_16(block->data, 0x70, 16); //501.1
    data_block->blocked = bit_lib_get_bits(block->data, 0x80, 1); //303
    data_block->start_trip_time = bit_lib_get_bits_16(block->data, 0x81, 12); //403
    data_block->start_trip_date = bit_lib_get_bits_16(block->data, 0x8D, 16); //402
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x9D, 16); //311
    data_block->valid_to_date = bit_lib_get_bits_16(block->data, 0xAD, 16); //312
    data_block->start_trip_seconds = bit_lib_get_bits(block->data, 0xDB, 6); //406
    data_block->transport_type1 = bit_lib_get_bits(block->data, 0xC3, 2); //421.1
    data_block->transport_type2 = bit_lib_get_bits(block->data, 0xC5, 2); //421.2
    data_block->transport_type3 = bit_lib_get_bits(block->data, 0xC7, 2); //421.3
    data_block->transport_type4 = bit_lib_get_bits(block->data, 0xC9, 2); //421.4
    data_block->use_with_date = bit_lib_get_bits_16(block->data, 0xBD, 16); //205
    data_block->route = bit_lib_get_bits(block->data, 0xCD, 1); //424
    data_block->validator1 = bit_lib_get_bits_16(block->data, 0xCE, 15); //422.1
    data_block->validator = bit_lib_get_bits_16(block->data, 0xCD, 16);
    data_block->total_trips = bit_lib_get_bits_16(block->data, 0xDD, 16); //331
    data_block->write_enabled = bit_lib_get_bits(block->data, 0xED, 1); //write_enabled
    data_block->rfu2 = bit_lib_get_bits(block->data, 0xEE, 2); //rfu2
    data_block->crc16_2 = bit_lib_get_bits_16(block->data, 0xF0, 16); //501.2
}

void parse_layout_6(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x38, 16); //202
    data_block->geozone_a = bit_lib_get_bits(block->data, 0x48, 4); //GeoZoneA
    data_block->geozone_b = bit_lib_get_bits(block->data, 0x4C, 4); //GeoZoneB
    data_block->blank_type = bit_lib_get_bits_16(block->data, 0x50, 10); //121
    data_block->type_of_extended = bit_lib_get_bits_16(block->data, 0x5A, 10); //122
    data_block->rfu1 = bit_lib_get_bits_16(block->data, 0x64, 12); //rfu1
    data_block->crc16 = bit_lib_get_bits_16(block->data, 0x70, 16); //501.1
    data_block->blocked = bit_lib_get_bits(block->data, 0x80, 1); //303
    data_block->start_trip_time = bit_lib_get_bits_16(block->data, 0x81, 12); //403
    data_block->start_trip_date = bit_lib_get_bits_16(block->data, 0x8D, 16); //402
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x9D, 16); //311
    data_block->valid_to_date = bit_lib_get_bits_16(block->data, 0xAD, 16); //312
    data_block->company = bit_lib_get_bits(block->data, 0xBD, 4); //Company
    data_block->validator1 = bit_lib_get_bits(block->data, 0xC1, 4); //422.1
    data_block->remaining_trips = bit_lib_get_bits_16(block->data, 0xC5, 10); //321
    data_block->units = bit_lib_get_bits(block->data, 0xCF, 6); //Units
    data_block->validator2 = bit_lib_get_bits_16(block->data, 0xD5, 10); //422.2
    data_block->total_trips = bit_lib_get_bits_16(block->data, 0xDF, 16); //331
    data_block->extended = bit_lib_get_bits(block->data, 0xEF, 1); //123
    data_block->crc16_2 = bit_lib_get_bits_16(block->data, 0xF0, 16); //501.2
}

void parse_layout_8(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x38, 16); //202
    data_block->rfu1 = bit_lib_get_bits_64(block->data, 0x48, 56); //rfu1
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x80, 16); //311
    data_block->valid_for_days = bit_lib_get_bits(block->data, 0x90, 8); //313
    data_block->requires_activation = bit_lib_get_bits(block->data, 0x98, 1); //301
    data_block->rfu2 = bit_lib_get_bits(block->data, 0x99, 7); //rfu2
    data_block->remaining_trips1 = bit_lib_get_bits(block->data, 0xA0, 8); //321.1
    data_block->remaining_trips = bit_lib_get_bits(block->data, 0xA8, 8); //321
    data_block->validator1 = bit_lib_get_bits(block->data, 0xB0, 2); //422.1
    data_block->validator = bit_lib_get_bits_16(block->data, 0xB1, 15); //422
    data_block->hash = bit_lib_get_bits_32(block->data, 0xC0, 32); //502
    data_block->rfu3 = bit_lib_get_bits_32(block->data, 0xE0, 32); //rfu3
}

void parse_layout_A(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x40, 12); //311
    data_block->valid_for_minutes = bit_lib_get_bits_32(block->data, 0x4C, 19); //314
    data_block->requires_activation = bit_lib_get_bits(block->data, 0x5F, 1); //301
    data_block->start_trip_minutes = bit_lib_get_bits_32(block->data, 0x60, 19); //405
    data_block->minutes_pass = bit_lib_get_bits(block->data, 0x77, 7); //412
    data_block->transport_type_flag = bit_lib_get_bits(block->data, 0x7E, 2); //421.0
    data_block->remaining_trips = bit_lib_get_bits(block->data, 0x80, 8); //321
    data_block->validator = bit_lib_get_bits_16(block->data, 0x88, 16); //422
    data_block->transport_type1 = bit_lib_get_bits(block->data, 0x98, 2); //421.1
    data_block->transport_type2 = bit_lib_get_bits(block->data, 0x9A, 2); //421.2
    data_block->transport_type3 = bit_lib_get_bits(block->data, 0x9C, 2); //421.3
    data_block->transport_type4 = bit_lib_get_bits(block->data, 0x9E, 2); //421.4
    data_block->hash = bit_lib_get_bits_32(block->data, 0xC0, 32); //502
}

void parse_layout_C(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x38, 16); //202
    data_block->rfu1 = bit_lib_get_bits_64(block->data, 0x48, 56); //rfu1
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x80, 16); //311
    data_block->valid_for_days = bit_lib_get_bits(block->data, 0x90, 8); //313
    data_block->requires_activation = bit_lib_get_bits(block->data, 0x98, 1); //301
    data_block->rfu2 = bit_lib_get_bits_16(block->data, 0x99, 13); //rfu2
    data_block->remaining_trips = bit_lib_get_bits_16(block->data, 0xA6, 10); //321
    data_block->validator = bit_lib_get_bits_16(block->data, 0xB0, 16); //422
    data_block->hash = bit_lib_get_bits_32(block->data, 0xC0, 32); //502
    data_block->start_trip_date = bit_lib_get_bits_16(block->data, 0xE0, 16); //402
    data_block->start_trip_time = bit_lib_get_bits_16(block->data, 0xF0, 11); //403
    data_block->transport_type = bit_lib_get_bits(block->data, 0xFB, 2); //421
    data_block->rfu3 = bit_lib_get_bits(block->data, 0xFD, 2); //rfu3
    data_block->transfer_in_metro = bit_lib_get_bits(block->data, 0xFF, 1); //432
}

void parse_layout_D(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->rfu1 = bit_lib_get_bits(block->data, 0x38, 8); //rfu1
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x40, 16); //202
    data_block->valid_for_time = bit_lib_get_bits_16(block->data, 0x50, 11); //316
    data_block->rfu2 = bit_lib_get_bits(block->data, 0x5B, 5); //rfu2
    data_block->use_before_date2 = bit_lib_get_bits_16(block->data, 0x60, 16); //202.2
    data_block->valid_for_time2 = bit_lib_get_bits_16(block->data, 0x70, 11); //316.2
    data_block->rfu3 = bit_lib_get_bits(block->data, 0x7B, 5); //rfu3
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x80, 16); //311
    data_block->valid_for_days = bit_lib_get_bits(block->data, 0x90, 8); //313
    data_block->requires_activation = bit_lib_get_bits(block->data, 0x98, 1); //301
    data_block->rfu4 = bit_lib_get_bits(block->data, 0x99, 2); //rfu4
    data_block->passage_5_minutes = bit_lib_get_bits(block->data, 0x9B, 5); //413
    data_block->transport_type1 = bit_lib_get_bits(block->data, 0xA0, 2); //421.1
    data_block->passage_in_metro = bit_lib_get_bits(block->data, 0xA2, 1); //431
    data_block->passages_ground_transport = bit_lib_get_bits(block->data, 0xA3, 3); //433
    data_block->remaining_trips = bit_lib_get_bits_16(block->data, 0xA6, 10); //321
    data_block->validator = bit_lib_get_bits_16(block->data, 0xB0, 16); //422
    data_block->hash = bit_lib_get_bits_32(block->data, 0xC0, 32); //502
    data_block->start_trip_date = bit_lib_get_bits_16(block->data, 0xE0, 16); //402
    data_block->start_trip_time = bit_lib_get_bits_16(block->data, 0xF0, 11); //403
    data_block->transport_type2 = bit_lib_get_bits(block->data, 0xFB, 2); //421.2
    data_block->rfu5 = bit_lib_get_bits(block->data, 0xFD, 2); //rfu5
    data_block->transfer_in_metro = bit_lib_get_bits(block->data, 0xFF, 1); //432
}

void parse_layout_E1(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->layout2 = bit_lib_get_bits(block->data, 0x38, 5); //112
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x3D, 16); //202
    data_block->blank_type = bit_lib_get_bits_16(block->data, 0x4D, 10); //121
    data_block->validator = bit_lib_get_bits_16(block->data, 0x80, 16); //422
    data_block->start_trip_date = bit_lib_get_bits_16(block->data, 0x90, 16); //402
    data_block->start_trip_time = bit_lib_get_bits_16(block->data, 0xA0, 11); //403
    data_block->transport_type1 = bit_lib_get_bits(block->data, 0xAB, 2); //421.1
    data_block->transport_type2 = bit_lib_get_bits(block->data, 0xAD, 2); //421.2
    data_block->transfer_in_metro = bit_lib_get_bits(block->data, 0xB1, 1); //432
    data_block->passage_in_metro = bit_lib_get_bits(block->data, 0xB2, 1); //431
    data_block->passages_ground_transport = bit_lib_get_bits(block->data, 0xB3, 3); //433
    data_block->minutes_pass = bit_lib_get_bits(block->data, 0xB9, 8); //412
    data_block->remaining_funds = bit_lib_get_bits_32(block->data, 0xC4, 19); //322
    data_block->fare_trip = bit_lib_get_bits(block->data, 0xD7, 2); //441
    data_block->blocked = bit_lib_get_bits(block->data, 0x9D, 1); //303
    data_block->zoo = bit_lib_get_bits(block->data, 0xDA, 1); //zoo
    data_block->hash = bit_lib_get_bits_32(block->data, 0xE0, 32); //502
}

void parse_layout_E2(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->layout2 = bit_lib_get_bits(block->data, 0x38, 5); //112
    data_block->type_of_extended = bit_lib_get_bits_16(block->data, 0x3D, 10); //122
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x47, 16); //202
    data_block->blank_type = bit_lib_get_bits_16(block->data, 0x57, 10); //121
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x61, 16); //311
    data_block->activate_during = bit_lib_get_bits_16(block->data, 0x71, 9); //302
    data_block->valid_for_minutes = bit_lib_get_bits_32(block->data, 0x83, 20); //314
    data_block->minutes_pass = bit_lib_get_bits(block->data, 0x9A, 8); //412
    data_block->transport_type = bit_lib_get_bits(block->data, 0xA3, 2); //421
    data_block->passage_in_metro = bit_lib_get_bits(block->data, 0xA5, 1); //431
    data_block->transfer_in_metro = bit_lib_get_bits(block->data, 0xA6, 1); //432
    data_block->remaining_trips = bit_lib_get_bits_16(block->data, 0xA7, 10); //321
    data_block->validator = bit_lib_get_bits_16(block->data, 0xB1, 16); //422
    data_block->start_trip_neg_minutes = bit_lib_get_bits_32(block->data, 0xC4, 20); //404
    data_block->requires_activation = bit_lib_get_bits(block->data, 0xD8, 1); //301
    data_block->blocked = bit_lib_get_bits(block->data, 0xD9, 1); //303
    data_block->extended = bit_lib_get_bits(block->data, 0xDA, 1); //123
    data_block->hash = bit_lib_get_bits_32(block->data, 0xE0, 32); //502
}

void parse_layout_E3(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->layout2 = bit_lib_get_bits(block->data, 0x38, 5); //112
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 61, 16); //202
    data_block->blank_type = bit_lib_get_bits_16(block->data, 0x4D, 10); //121
    data_block->remaining_funds = bit_lib_get_bits_32(block->data, 0xBC, 22); //322
    data_block->hash = bit_lib_get_bits_32(block->data, 224, 32); //502
    data_block->validator = bit_lib_get_bits_16(block->data, 0x80, 16); //422
    data_block->start_trip_minutes = bit_lib_get_bits_32(block->data, 0x90, 23); //405
    data_block->fare_trip = bit_lib_get_bits(block->data, 0xD2, 2); //441
    data_block->minutes_pass = bit_lib_get_bits(block->data, 0xAB, 7); //412
    data_block->transport_type_flag = bit_lib_get_bits(block->data, 0xB2, 2); //421.0
    data_block->transport_type1 = bit_lib_get_bits(block->data, 0xB4, 2); //421.1
    data_block->transport_type2 = bit_lib_get_bits(block->data, 0xB6, 2); //421.2
    data_block->transport_type3 = bit_lib_get_bits(block->data, 0xB8, 2); //421.3
    data_block->transport_type4 = bit_lib_get_bits(block->data, 0xBA, 2); //421.4
    data_block->blocked = bit_lib_get_bits(block->data, 0xD4, 1); //303
}

void parse_layout_E4(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->layout2 = bit_lib_get_bits(block->data, 0x38, 5); //112
    data_block->type_of_extended = bit_lib_get_bits_16(block->data, 0x3D, 10); //122
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x47, 13); //202
    data_block->blank_type = bit_lib_get_bits_16(block->data, 0x54, 10); //121
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x5E, 13); //311
    data_block->activate_during = bit_lib_get_bits_16(block->data, 0x6B, 9); //302
    data_block->extension_counter = bit_lib_get_bits_16(block->data, 0x74, 10); //304
    data_block->valid_for_minutes = bit_lib_get_bits_32(block->data, 0x80, 20); //314
    data_block->minutes_pass = bit_lib_get_bits(block->data, 0x98, 7); //412
    data_block->transport_type_flag = bit_lib_get_bits(block->data, 0x9F, 2); //421.0
    data_block->transport_type1 = bit_lib_get_bits(block->data, 0xA1, 2); //421.1
    data_block->transport_type2 = bit_lib_get_bits(block->data, 0xA3, 2); //421.2
    data_block->transport_type3 = bit_lib_get_bits(block->data, 0xA5, 2); //421.3
    data_block->transport_type4 = bit_lib_get_bits(block->data, 0xA7, 2); //421.4
    data_block->remaining_trips = bit_lib_get_bits_16(block->data, 0xA9, 10); //321
    data_block->validator = bit_lib_get_bits_16(block->data, 0xB3, 16); //422
    data_block->start_trip_neg_minutes = bit_lib_get_bits_32(block->data, 0xC3, 20); //404
    data_block->requires_activation = bit_lib_get_bits(block->data, 0xD7, 1); //301
    data_block->blocked = bit_lib_get_bits(block->data, 0xD8, 1); //303
    data_block->extended = bit_lib_get_bits(block->data, 0xD9, 1); //123
    data_block->hash = bit_lib_get_bits_32(block->data, 0xE0, 32); //502
}

void parse_layout_E5(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->layout2 = bit_lib_get_bits(block->data, 0x38, 5); //112
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x3D, 13); //202
    data_block->blank_type = bit_lib_get_bits_16(block->data, 0x4A, 10); //121
    data_block->valid_to_time = bit_lib_get_bits_32(block->data, 0x54, 23); //317
    data_block->extension_counter = bit_lib_get_bits_16(block->data, 0x6B, 10); //304
    data_block->start_trip_minutes = bit_lib_get_bits_32(block->data, 0x80, 23); //405
    data_block->metro_ride_with = bit_lib_get_bits(block->data, 0x97, 7); //414
    data_block->minutes_pass = bit_lib_get_bits(block->data, 0x9E, 7); //412
    data_block->remaining_funds = bit_lib_get_bits_32(block->data, 0xA7, 19); //322
    data_block->validator = bit_lib_get_bits_16(block->data, 0xBA, 16); //422
    data_block->blocked = bit_lib_get_bits(block->data, 0xCA, 1); //303
    data_block->route = bit_lib_get_bits_16(block->data, 0xCC, 12); //424
    data_block->passages_ground_transport = bit_lib_get_bits(block->data, 0xD8, 7); //433
    data_block->hash = bit_lib_get_bits_32(block->data, 0xE0, 32); //502
}

void parse_layout_E6(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->layout2 = bit_lib_get_bits(block->data, 0x38, 5); //112
    data_block->type_of_extended = bit_lib_get_bits_16(block->data, 0x3D, 10); //122
    data_block->use_before_date = bit_lib_get_bits_16(block->data, 0x47, 13); //202
    data_block->blank_type = bit_lib_get_bits_16(block->data, 0x54, 10); //121
    data_block->valid_from_date = bit_lib_get_bits_32(block->data, 0x5E, 23); //311
    data_block->extension_counter = bit_lib_get_bits_16(block->data, 0x75, 10); //304
    data_block->valid_for_minutes = bit_lib_get_bits_32(block->data, 0x80, 20); //314
    data_block->start_trip_neg_minutes = bit_lib_get_bits_32(block->data, 0x94, 20); //404
    data_block->metro_ride_with = bit_lib_get_bits(block->data, 0xA8, 7); //414
    data_block->minutes_pass = bit_lib_get_bits(block->data, 0xAF, 7); //412
    data_block->remaining_trips = bit_lib_get_bits(block->data, 0xB6, 7); //321
    data_block->validator = bit_lib_get_bits_16(block->data, 0xBD, 16); //422
    data_block->blocked = bit_lib_get_bits(block->data, 0xCD, 1); //303
    data_block->extended = bit_lib_get_bits(block->data, 0xCE, 1); //123
    data_block->route = bit_lib_get_bits_16(block->data, 0xD4, 12); //424
    data_block->hash = bit_lib_get_bits_32(block->data, 0xE0, 32); //502
}

void parse_layout_FCB(BlockData* data_block, const MfClassicBlock* block) {
    data_block->view = bit_lib_get_bits_16(block->data, 0x00, 10); //101
    data_block->type = bit_lib_get_bits_16(block->data, 0x0A, 10); //102
    data_block->number = bit_lib_get_bits_32(block->data, 0x14, 32); //201
    data_block->layout = bit_lib_get_bits(block->data, 0x34, 4); //111
    data_block->tech_code = bit_lib_get_bits_32(block->data, 0x38, 10); //tech_code
    data_block->valid_from_date = bit_lib_get_bits_16(block->data, 0x42, 16); //311
    data_block->valid_to_date = bit_lib_get_bits_16(block->data, 0x52, 16); //312
    data_block->interval = bit_lib_get_bits(block->data, 0x62, 4); //interval
    data_block->app_code1 = bit_lib_get_bits_16(block->data, 0x66, 10); //app_code1
    data_block->hash1 = bit_lib_get_bits_16(block->data, 0x70, 16); //502.1
    data_block->type1 = bit_lib_get_bits_16(block->data, 0x80, 10); //type1
    data_block->app_code2 = bit_lib_get_bits_16(block->data, 0x8A, 10); //app_code2
    data_block->type2 = bit_lib_get_bits_16(block->data, 0x94, 10); //type2
    data_block->app_code3 = bit_lib_get_bits_16(block->data, 0x9E, 10); //app_code3
    data_block->type3 = bit_lib_get_bits_16(block->data, 0xA8, 10); //type3
    data_block->app_code4 = bit_lib_get_bits_16(block->data, 0xB2, 10); //app_code4
    data_block->type4 = bit_lib_get_bits_16(block->data, 0xBC, 10); //type4
    data_block->hash2 = bit_lib_get_bits_32(block->data, 0xE0, 32); //502.2
}

void parse_transport_type(BlockData* data_block, FuriString* transport) {
    switch(data_block->transport_type_flag) {
    case 1:
        uint8_t transport_type =
            (data_block->transport_type1 || data_block->transport_type2 ||
             data_block->transport_type3 || data_block->transport_type4);
        switch(transport_type) {
        case 1:
            furi_string_cat(transport, "Metro");
            break;
        case 2:
            furi_string_cat(transport, "Monorail");
            break;
        case 3:
            furi_string_cat(transport, "MCC");
            break;
        default:
            furi_string_cat(transport, "Unknown");
            break;
        }
        break;
    case 2:
        furi_string_cat(transport, "Ground");
        break;
    default:
        furi_string_cat(transport, "");
        break;
    }
}

bool mosgortrans_parse_transport_block(const MfClassicBlock* block, FuriString* result) {
    BlockData data_block = {};
    const uint16_t valid_departments[] = {0x106, 0x108, 0x10A, 0x10E, 0x110, 0x117};
    uint16_t transport_department = bit_lib_get_bits_16(block->data, 0, 10);
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
        furi_string_cat_printf(result, "Transport department: %x\n", transport_department);
    }
    bool department_valid = false;
    for(uint8_t i = 0; i < 6; i++) {
        if(transport_department == valid_departments[i]) {
            department_valid = true;
            break;
        }
    }
    if(!department_valid) {
        return false;
    }
    FURI_LOG_D(TAG, "Transport department: %x", transport_department);
    uint16_t layout_type = bit_lib_get_bits_16(block->data, 52, 4);
    if(layout_type == 0xE) {
        layout_type = bit_lib_get_bits_16(block->data, 52, 9);
    } else if(layout_type == 0xF) {
        layout_type = bit_lib_get_bits_16(block->data, 52, 14);
    }
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagDebug)) {
        furi_string_cat_printf(result, "Layout: %x\n", layout_type);
    }
    FURI_LOG_D(TAG, "Layout type %x", layout_type);
    switch(layout_type) {
    case 0x02: {
        parse_layout_2(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);

        if(data_block.valid_from_date == 0 || data_block.valid_to_date == 0) {
            furi_string_cat(result, "\e#No ticket");
            return false;
        }
        //remaining_trips
        furi_string_cat_printf(result, "Trips: %d\n", data_block.total_trips);
        //valid_from_date
        DateTime card_valid_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_valid_from_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_valid_from_date_s.day,
            card_valid_from_date_s.month,
            card_valid_from_date_s.year);
        //valid_to_date
        DateTime card_valid_to_date_s = {0};
        from_days_to_datetime(data_block.valid_to_date, &card_valid_to_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d\n",
            card_valid_to_date_s.day,
            card_valid_to_date_s.month,
            card_valid_to_date_s.year);
        //trip_number
        furi_string_cat_printf(result, "Trips: %d\n", data_block.total_trips);
        //trip_from
        DateTime card_start_trip_minutes_s = {0};
        from_seconds_to_datetime(
            data_block.start_trip_date * 24 * 60 * 60 + data_block.start_trip_time * 60 +
                data_block.start_trip_seconds,
            &card_start_trip_minutes_s,
            1992);
        furi_string_cat_printf(
            result,
            "Trip from: %02d.%02d.%04d %02d:%02d",
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute);
        break;
    }
    case 0x06: {
        parse_layout_6(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        //remaining_trips
        furi_string_cat_printf(result, "Trips left: %d\n", data_block.remaining_trips);
        //valid_from_date
        DateTime card_valid_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_valid_from_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_valid_from_date_s.day,
            card_valid_from_date_s.month,
            card_valid_from_date_s.year);
        //valid_to_date
        DateTime card_valid_to_date_s = {0};
        from_days_to_datetime(data_block.valid_to_date, &card_valid_to_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d\n",
            card_valid_to_date_s.day,
            card_valid_to_date_s.month,
            card_valid_to_date_s.year);
        //trip_number
        furi_string_cat_printf(result, "Trips: %d\n", data_block.total_trips);
        //trip_from
        DateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            (data_block.start_trip_date) * 24 * 60 + data_block.start_trip_time,
            &card_start_trip_minutes_s,
            1992);
        furi_string_cat_printf(
            result,
            "Trip from: %02d.%02d.%04d %02d:%02d\n",
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute);
        //validator
        furi_string_cat_printf(
            result, "Validator: %05d", data_block.validator1 * 1024 + data_block.validator2);
        break;
    }
    case 0x08: {
        parse_layout_8(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 1992);
        //remaining_trips
        furi_string_cat_printf(result, "Trips left: %d\n", data_block.remaining_trips);
        //valid_from_date
        DateTime card_valid_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_valid_from_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_valid_from_date_s.day,
            card_valid_from_date_s.month,
            card_valid_from_date_s.year);
        //valid_to_date
        DateTime card_valid_to_date_s = {0};
        from_days_to_datetime(
            data_block.valid_from_date + data_block.valid_for_days, &card_valid_to_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d",
            card_valid_to_date_s.day,
            card_valid_to_date_s.month,
            card_valid_to_date_s.year);
        break;
    }
    case 0x0A: {
        parse_layout_A(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 2016);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        //remaining_trips
        furi_string_cat_printf(result, "Trips left: %d\n", data_block.remaining_trips);
        //valid_from_date
        DateTime card_valid_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_valid_from_date_s, 2016);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_valid_from_date_s.day,
            card_valid_from_date_s.month,
            card_valid_from_date_s.year);
        //valid_to_date
        DateTime card_valid_to_date_s = {0};
        from_minutes_to_datetime(
            data_block.valid_from_date * 24 * 60 + data_block.valid_for_minutes - 1,
            &card_valid_to_date_s,
            2016);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d",
            card_valid_to_date_s.day,
            card_valid_to_date_s.month,
            card_valid_to_date_s.year);
        //trip_from
        if(data_block.start_trip_minutes) {
            DateTime card_start_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.valid_from_date * 24 * 60 + data_block.start_trip_minutes,
                &card_start_trip_minutes_s,
                2016);
            furi_string_cat_printf(
                result,
                "\nTrip from: %02d.%02d.%04d %02d:%02d",
                card_start_trip_minutes_s.day,
                card_start_trip_minutes_s.month,
                card_start_trip_minutes_s.year,
                card_start_trip_minutes_s.hour,
                card_start_trip_minutes_s.minute);
        }
        //trip_switch
        if(data_block.minutes_pass) {
            DateTime card_start_switch_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.valid_from_date * 24 * 60 + data_block.start_trip_minutes +
                    data_block.minutes_pass,
                &card_start_switch_trip_minutes_s,
                2016);
            furi_string_cat_printf(
                result,
                "\nTrip switch: %02d.%02d.%04d %02d:%02d",
                card_start_switch_trip_minutes_s.day,
                card_start_switch_trip_minutes_s.month,
                card_start_switch_trip_minutes_s.year,
                card_start_switch_trip_minutes_s.hour,
                card_start_switch_trip_minutes_s.minute);
        }
        //transport
        FuriString* transport = furi_string_alloc();
        parse_transport_type(&data_block, transport);
        furi_string_cat_printf(result, "\nTransport: %s", furi_string_get_cstr(transport));
        //validator
        if(data_block.validator) {
            furi_string_cat_printf(result, "\nValidator: %05d", data_block.validator);
        }
        furi_string_free(transport);
        break;
    }
    case 0x0C: {
        parse_layout_C(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 1992);
        //remaining_trips
        furi_string_cat_printf(result, "Trips left: %d\n", data_block.remaining_trips);
        //valid_from_date
        DateTime card_valid_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_valid_from_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_valid_from_date_s.day,
            card_valid_from_date_s.month,
            card_valid_from_date_s.year);
        //valid_to_date
        DateTime card_valid_to_date_s = {0};
        from_days_to_datetime(
            data_block.valid_from_date + data_block.valid_for_days, &card_valid_to_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d\n",
            card_valid_to_date_s.day,
            card_valid_to_date_s.month,
            card_valid_to_date_s.year);
        //remaining_trips
        furi_string_cat_printf(result, "Trips left: %d", data_block.remaining_trips);
        //trip_from
        if(data_block.start_trip_date) { // TODO: (-nofl) unused
            DateTime card_start_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.start_trip_date * 24 * 60 + data_block.start_trip_time,
                &card_start_trip_minutes_s,
                1992);
        }
        //validator
        if(data_block.validator) {
            furi_string_cat_printf(result, "\nValidator: %05d", data_block.validator);
        }
        break;
    }
    case 0x0D: {
        parse_layout_D(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        //remaining_trips
        furi_string_cat_printf(result, "Trips left: %d\n", data_block.remaining_trips);
        //valid_from_date
        DateTime card_valid_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_valid_from_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_valid_from_date_s.day,
            card_valid_from_date_s.month,
            card_valid_from_date_s.year);
        //valid_to_date
        DateTime card_valid_to_date_s = {0};
        from_days_to_datetime(
            data_block.valid_from_date + data_block.valid_for_days, &card_valid_to_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d",
            card_valid_to_date_s.day,
            card_valid_to_date_s.month,
            card_valid_to_date_s.year);
        //trip_from
        if(data_block.start_trip_date) { // TODO: (-nofl) unused
            DateTime card_start_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.start_trip_date * 24 * 60 + data_block.start_trip_time,
                &card_start_trip_minutes_s,
                1992);
        }
        //trip_switch
        if(data_block.passage_5_minutes) { // TODO: (-nofl) unused
            DateTime card_start_switch_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.start_trip_date * 24 * 60 + data_block.start_trip_time +
                    data_block.passage_5_minutes,
                &card_start_switch_trip_minutes_s,
                1992);
        }
        //validator
        if(data_block.validator) {
            furi_string_cat_printf(result, "\nValidator: %05d", data_block.validator);
        }
        break;
    }
    case 0xE1:
    case 0x1C1: {
        parse_layout_E1(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        //remaining_funds
        furi_string_cat_printf(result, "Balance: %ld rub\n", data_block.remaining_funds / 100);
        //trip_from
        if(data_block.start_trip_date) {
            DateTime card_start_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.start_trip_date * 24 * 60 + data_block.start_trip_time,
                &card_start_trip_minutes_s,
                1992);
            furi_string_cat_printf(
                result,
                "Trip from: %02d.%02d.%04d %02d:%02d\n",
                card_start_trip_minutes_s.day,
                card_start_trip_minutes_s.month,
                card_start_trip_minutes_s.year,
                card_start_trip_minutes_s.hour,
                card_start_trip_minutes_s.minute);
        }
        //transport
        FuriString* transport = furi_string_alloc();
        switch(data_block.transport_type1) {
        case 1:
            switch(data_block.transport_type2) {
            case 1:
                furi_string_cat(transport, "Metro");
                break;
            case 2:
                furi_string_cat(transport, "Monorail");
                break;
            default:
                furi_string_cat(transport, "Unknown");
                break;
            }
            break;
        case 2:
            furi_string_cat(transport, "Ground");
            break;
        case 3:
            furi_string_cat(transport, "MCC");
            break;
        default:
            furi_string_cat(transport, "");
            break;
        }
        furi_string_cat_printf(result, "Transport: %s", furi_string_get_cstr(transport));
        //validator
        if(data_block.validator) {
            furi_string_cat_printf(result, "\nValidator: %05d", data_block.validator);
        }
        furi_string_free(transport);
        break;
    }
    case 0xE2:
    case 0x1C2: {
        parse_layout_E2(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        //remaining_trips
        furi_string_cat_printf(result, "Trips left: %d\n", data_block.remaining_trips);
        //valid_from_date
        DateTime card_valid_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_valid_from_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d",
            card_valid_from_date_s.day,
            card_valid_from_date_s.month,
            card_valid_from_date_s.year);
        //valid_to_date
        if(data_block.activate_during) {
            DateTime card_valid_to_date_s = {0};
            from_days_to_datetime(
                data_block.valid_from_date + data_block.activate_during,
                &card_valid_to_date_s,
                1992);
            furi_string_cat_printf(
                result,
                "\nValid to: %02d.%02d.%04d",
                card_valid_to_date_s.day,
                card_valid_to_date_s.month,
                card_valid_to_date_s.year);
        } else {
            DateTime card_valid_to_date_s = {0};
            from_minutes_to_datetime(
                data_block.valid_from_date * 24 * 60 + data_block.valid_for_minutes - 1,
                &card_valid_to_date_s,
                1992);
            furi_string_cat_printf(
                result,
                "\nValid to: %02d.%02d.%04d",
                card_valid_to_date_s.day,
                card_valid_to_date_s.month,
                card_valid_to_date_s.year);
        }
        //trip_from
        if(data_block.start_trip_neg_minutes) {
            DateTime card_start_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.valid_to_date * 24 * 60 + data_block.valid_for_minutes -
                    data_block.start_trip_neg_minutes,
                &card_start_trip_minutes_s,
                1992); //-time
            furi_string_cat_printf(
                result,
                "\nTrip from: %02d.%02d.%04d %02d:%02d",
                card_start_trip_minutes_s.day,
                card_start_trip_minutes_s.month,
                card_start_trip_minutes_s.year,
                card_start_trip_minutes_s.hour,
                card_start_trip_minutes_s.minute);
        }
        //trip_switch
        if(data_block.minutes_pass) {
            DateTime card_start_switch_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.valid_from_date * 24 * 60 + data_block.valid_for_minutes -
                    data_block.start_trip_neg_minutes + data_block.minutes_pass,
                &card_start_switch_trip_minutes_s,
                1992);
            furi_string_cat_printf(
                result,
                "\nTrip switch: %02d.%02d.%04d %02d:%02d",
                card_start_switch_trip_minutes_s.day,
                card_start_switch_trip_minutes_s.month,
                card_start_switch_trip_minutes_s.year,
                card_start_switch_trip_minutes_s.hour,
                card_start_switch_trip_minutes_s.minute);
        }
        //transport
        FuriString* transport = furi_string_alloc();
        switch(data_block.transport_type) { // TODO: (-nofl) unused
        case 1:
            furi_string_cat(transport, "Metro");
            break;
        case 2:
            furi_string_cat(transport, "Monorail");
            break;
        case 3:
            furi_string_cat(transport, "Ground");
            break;
        default:
            furi_string_cat(transport, "Unknown");
            break;
        }
        //validator
        if(data_block.validator) {
            furi_string_cat_printf(result, "\nValidator: %05d", data_block.validator);
        }
        furi_string_free(transport);
        break;
    }
    case 0xE3:
    case 0x1C3: {
        parse_layout_E3(&data_block, block);
        // number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        // use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        // remaining_funds
        furi_string_cat_printf(result, "Balance: %lu rub\n", data_block.remaining_funds);
        // start_trip_minutes
        DateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(data_block.start_trip_minutes, &card_start_trip_minutes_s, 2016);
        furi_string_cat_printf(
            result,
            "Trip from: %02d.%02d.%04d %02d:%02d\n",
            card_start_trip_minutes_s.day,
            card_start_trip_minutes_s.month,
            card_start_trip_minutes_s.year,
            card_start_trip_minutes_s.hour,
            card_start_trip_minutes_s.minute);
        // transport
        FuriString* transport = furi_string_alloc();
        parse_transport_type(&data_block, transport);
        furi_string_cat_printf(result, "Transport: %s\n", furi_string_get_cstr(transport));
        // validator
        furi_string_cat_printf(result, "Validator: %05d\n", data_block.validator);
        // fare
        FuriString* fare = furi_string_alloc();
        switch(data_block.fare_trip) {
        case 0:
            furi_string_cat(fare, "");
            break;
        case 1:
            furi_string_cat(fare, "Single");
            break;
        case 2:
            furi_string_cat(fare, "90 minutes");
            break;
        default:
            furi_string_cat(fare, "Unknown");
            break;
        }
        furi_string_cat_printf(result, "Fare: %s", furi_string_get_cstr(fare));
        furi_string_free(fare);
        furi_string_free(transport);
        break;
    }
    case 0xE4:
    case 0x1C4: {
        parse_layout_E4(&data_block, block);

        // number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        // use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 2016);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        // remaining_funds
        furi_string_cat_printf(result, "Balance: %lu rub\n", data_block.remaining_funds);
        // valid_from_date
        DateTime card_use_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_use_from_date_s, 2016);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_use_from_date_s.day,
            card_use_from_date_s.month,
            card_use_from_date_s.year);
        // valid_to_date
        DateTime card_use_to_date_s = {0};
        if(data_block.requires_activation) {
            from_days_to_datetime(
                data_block.valid_from_date + data_block.activate_during,
                &card_use_to_date_s,
                2016);
        } else {
            from_minutes_to_datetime(
                data_block.valid_from_date * 24 * 60 + data_block.valid_for_minutes - 1,
                &card_use_to_date_s,
                2016);
        }

        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d\n",
            card_use_to_date_s.day,
            card_use_to_date_s.month,
            card_use_to_date_s.year);
        // trip_number
        // furi_string_cat_printf(result, "Trips left: %d", data_block.remaining_trips);
        // trip_from
        DateTime card_start_trip_minutes_s = {0};
        from_minutes_to_datetime(
            data_block.valid_from_date * 24 * 60 + data_block.valid_for_minutes -
                data_block.start_trip_neg_minutes,
            &card_start_trip_minutes_s,
            2016); // TODO: (-nofl) unused
        //transport
        FuriString* transport = furi_string_alloc();
        parse_transport_type(&data_block, transport);
        furi_string_cat_printf(result, "Transport: %s", furi_string_get_cstr(transport));
        // validator
        if(data_block.validator) {
            furi_string_cat_printf(result, "\nValidator: %05d", data_block.validator);
        }
        furi_string_free(transport);
        break;
    }
    case 0xE5:
    case 0x1C5: {
        parse_layout_E5(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 2019);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        //remaining_funds
        furi_string_cat_printf(result, "Balance: %ld rub", data_block.remaining_funds / 100);
        //start_trip_minutes
        if(data_block.start_trip_minutes) {
            DateTime card_start_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.start_trip_minutes, &card_start_trip_minutes_s, 2019);
            furi_string_cat_printf(
                result,
                "\nTrip from: %02d.%02d.%04d %02d:%02d",
                card_start_trip_minutes_s.day,
                card_start_trip_minutes_s.month,
                card_start_trip_minutes_s.year,
                card_start_trip_minutes_s.hour,
                card_start_trip_minutes_s.minute);
        }
        //start_m_trip_minutes
        if(data_block.metro_ride_with) {
            DateTime card_start_m_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.start_trip_minutes + data_block.metro_ride_with,
                &card_start_m_trip_minutes_s,
                2019);
            furi_string_cat_printf(
                result,
                "\n(M) from: %02d.%02d.%04d %02d:%02d",
                card_start_m_trip_minutes_s.day,
                card_start_m_trip_minutes_s.month,
                card_start_m_trip_minutes_s.year,
                card_start_m_trip_minutes_s.hour,
                card_start_m_trip_minutes_s.minute);
        }
        if(data_block.minutes_pass) {
            DateTime card_start_change_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.start_trip_minutes + data_block.minutes_pass,
                &card_start_change_trip_minutes_s,
                2019);
            furi_string_cat_printf(
                result,
                "\nTrip edit: %02d.%02d.%04d %02d:%02d",
                card_start_change_trip_minutes_s.day,
                card_start_change_trip_minutes_s.month,
                card_start_change_trip_minutes_s.year,
                card_start_change_trip_minutes_s.hour,
                card_start_change_trip_minutes_s.minute);
        }
        //transport
        //validator
        if(data_block.validator) {
            furi_string_cat_printf(result, "\nValidator: %05d", data_block.validator);
        }
        break;
    }
    case 0xE6:
    case 0x1C6: {
        parse_layout_E6(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //use_before_date
        DateTime card_use_before_date_s = {0};
        from_days_to_datetime(data_block.use_before_date, &card_use_before_date_s, 2019);
        furi_string_cat_printf(
            result,
            "Use before: %02d.%02d.%04d\n",
            card_use_before_date_s.day,
            card_use_before_date_s.month,
            card_use_before_date_s.year);
        //remaining_trips
        furi_string_cat_printf(result, "Trips left: %d\n", data_block.remaining_trips);
        //valid_from_date
        DateTime card_use_from_date_s = {0};
        from_minutes_to_datetime(data_block.valid_from_date, &card_use_from_date_s, 2019);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_use_from_date_s.day,
            card_use_from_date_s.month,
            card_use_from_date_s.year);
        //valid_to_date
        DateTime card_use_to_date_s = {0};
        from_minutes_to_datetime(
            data_block.valid_from_date + data_block.valid_for_minutes - 1,
            &card_use_to_date_s,
            2019);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d",
            card_use_to_date_s.day,
            card_use_to_date_s.month,
            card_use_to_date_s.year);
        //start_trip_minutes
        if(data_block.start_trip_neg_minutes) {
            DateTime card_start_trip_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.valid_from_date + data_block.valid_for_minutes -
                    data_block.start_trip_neg_minutes,
                &card_start_trip_minutes_s,
                2019); //-time
            furi_string_cat_printf(
                result,
                "\nTrip from: %02d.%02d.%04d %02d:%02d",
                card_start_trip_minutes_s.day,
                card_start_trip_minutes_s.month,
                card_start_trip_minutes_s.year,
                card_start_trip_minutes_s.hour,
                card_start_trip_minutes_s.minute);
        }
        //start_trip_m_minutes
        if(data_block.metro_ride_with) {
            DateTime card_start_trip_m_minutes_s = {0};
            from_minutes_to_datetime(
                data_block.valid_from_date + data_block.valid_for_minutes -
                    data_block.start_trip_neg_minutes + data_block.metro_ride_with,
                &card_start_trip_m_minutes_s,
                2019);
            furi_string_cat_printf(
                result,
                "\n(M) from: %02d.%02d.%04d %02d:%02d",
                card_start_trip_m_minutes_s.day,
                card_start_trip_m_minutes_s.month,
                card_start_trip_m_minutes_s.year,
                card_start_trip_m_minutes_s.hour,
                card_start_trip_m_minutes_s.minute);
        }
        //transport
        //validator
        if(data_block.validator) {
            furi_string_cat_printf(result, "\nValidator: %05d", data_block.validator);
        }
        break;
    }
    case 0x3CCB: {
        parse_layout_FCB(&data_block, block);
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //valid_from_date
        DateTime card_use_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_use_from_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_use_from_date_s.day,
            card_use_from_date_s.month,
            card_use_from_date_s.year);
        //valid_to_date
        DateTime card_use_to_date_s = {0};
        from_days_to_datetime(data_block.valid_to_date, &card_use_to_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d",
            card_use_to_date_s.day,
            card_use_to_date_s.month,
            card_use_to_date_s.year);
        break;
    }
    case 0x3C0B: {
        //number
        furi_string_cat_printf(result, "Number: %010lu\n", data_block.number);
        //valid_from_date
        DateTime card_use_from_date_s = {0};
        from_days_to_datetime(data_block.valid_from_date, &card_use_from_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid from: %02d.%02d.%04d\n",
            card_use_from_date_s.day,
            card_use_from_date_s.month,
            card_use_from_date_s.year);
        //valid_to_date
        DateTime card_use_to_date_s = {0};
        from_days_to_datetime(data_block.valid_to_date, &card_use_to_date_s, 1992);
        furi_string_cat_printf(
            result,
            "Valid to: %02d.%02d.%04d",
            card_use_to_date_s.day,
            card_use_to_date_s.month,
            card_use_to_date_s.year);
        break;
    }
    default:
        result = NULL;
        return false;
    }

    return true;
}



const MfClassicKeyPair troika_1k_keys[16] = {
    {.a = 0xa0a1a2a3a4a5, .b = 0xfbf225dc5d58},
    {.a = 0xa82607b01c0d, .b = 0x2910989b6880},
    {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.a = 0x73068f118c13, .b = 0x2b7f3253fac5},
    {.a = 0xfbc2793d540b, .b = 0xd3a297dc2698},
    {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.a = 0xae3d65a3dad4, .b = 0x0f1c63013dba},
    {.a = 0xa73f5dc1d333, .b = 0xe35173494a81},
    {.a = 0x69a32f1c2f19, .b = 0x6b8bd9860763},
    {.a = 0x9becdf3d9273, .b = 0xf8493407799d},
    {.a = 0x08b386463229, .b = 0x5efbaecef46b},
    {.a = 0xcd4c61c26e3d, .b = 0x31c7610de3b0},
    {.a = 0xa82607b01c0d, .b = 0x2910989b6880},
    {.a = 0x0e8f64340ba4, .b = 0x4acec1205d75},
    {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
};

const MfClassicKeyPair troika_4k_keys[40] = {
    {.a = 0xEC29806D9738, .b = 0xFBF225DC5D58}, //1
    {.a = 0xA0A1A2A3A4A5, .b = 0x7DE02A7F6025}, //2
    {.a = 0x2AA05ED1856F, .b = 0xEAAC88E5DC99}, //3
    {.a = 0x2AA05ED1856F, .b = 0xEAAC88E5DC99}, //4
    {.a = 0x73068F118C13, .b = 0x2B7F3253FAC5}, //5
    {.a = 0xFBC2793D540B, .b = 0xD3A297DC2698}, //6
    {.a = 0x2AA05ED1856F, .b = 0xEAAC88E5DC99}, //7
    {.a = 0xAE3D65A3DAD4, .b = 0x0F1C63013DBA}, //8
    {.a = 0xA73F5DC1D333, .b = 0xE35173494A81}, //9
    {.a = 0x69A32F1C2F19, .b = 0x6B8BD9860763}, //10
    {.a = 0x9BECDF3D9273, .b = 0xF8493407799D}, //11
    {.a = 0x08B386463229, .b = 0x5EFBAECEF46B}, //12
    {.a = 0xCD4C61C26E3D, .b = 0x31C7610DE3B0}, //13
    {.a = 0xA82607B01C0D, .b = 0x2910989B6880}, //14
    {.a = 0x0E8F64340BA4, .b = 0x4ACEC1205D75}, //15
    {.a = 0x2AA05ED1856F, .b = 0xEAAC88E5DC99}, //16
    {.a = 0x6B02733BB6EC, .b = 0x7038CD25C408}, //17
    {.a = 0x403D706BA880, .b = 0xB39D19A280DF}, //18
    {.a = 0xC11F4597EFB5, .b = 0x70D901648CB9}, //19
    {.a = 0x0DB520C78C1C, .b = 0x73E5B9D9D3A4}, //20
    {.a = 0x3EBCE0925B2F, .b = 0x372CC880F216}, //21
    {.a = 0x16A27AF45407, .b = 0x9868925175BA}, //22
    {.a = 0xABA208516740, .b = 0xCE26ECB95252}, //23
    {.a = 0xCD64E567ABCD, .b = 0x8F79C4FD8A01}, //24
    {.a = 0x764CD061F1E6, .b = 0xA74332F74994}, //25
    {.a = 0x1CC219E9FEC1, .b = 0xB90DE525CEB6}, //26
    {.a = 0x2FE3CB83EA43, .b = 0xFBA88F109B32}, //27
    {.a = 0x07894FFEC1D6, .b = 0xEFCB0E689DB3}, //28
    {.a = 0x04C297B91308, .b = 0xC8454C154CB5}, //29
    {.a = 0x7A38E3511A38, .b = 0xAB16584C972A}, //30
    {.a = 0x7545DF809202, .b = 0xECF751084A80}, //31
    {.a = 0x5125974CD391, .b = 0xD3EAFB5DF46D}, //32
    {.a = 0x7A86AA203788, .b = 0xE41242278CA2}, //33
    {.a = 0xAFCEF64C9913, .b = 0x9DB96DCA4324}, //34
    {.a = 0x04EAA462F70B, .b = 0xAC17B93E2FAE}, //35
    {.a = 0xE734C210F27E, .b = 0x29BA8C3E9FDA}, //36
    {.a = 0xD5524F591EED, .b = 0x5DAF42861B4D}, //37
    {.a = 0xE4821A377B75, .b = 0xE8709E486465}, //38
    {.a = 0x518DC6EEA089, .b = 0x97C64AC98CA4}, //39
    {.a = 0xBB52F8CCE07F, .b = 0x6B6119752C70}, //40
};

static bool troika_get_card_config(TroikaCardConfig* config, MfClassicType type) {
    bool success = true;

    if(type == MfClassicType1k) {
        config->data_sector = 11;
        config->keys = troika_1k_keys;
    } else if(type == MfClassicType4k) {
        config->data_sector = 8; // Further testing needed
        config->keys = troika_4k_keys;
    } else {
        success = false;
    }

    return success;
}

static bool troika_parse(FuriString* parsed_data, const MfClassicData* data) {
    bool parsed = false;

    do {
        // Verify card type
        TroikaCardConfig cfg = {};
        if(!troika_get_card_config(&cfg, data->type)) break;

        // Verify key
        const MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, cfg.data_sector);

        const uint64_t key =
            bit_lib_bytes_to_num_be(sec_tr->key_a.data, COUNT_OF(sec_tr->key_a.data));
        if(key != cfg.keys[cfg.data_sector].a) break;

        FuriString* metro_result = furi_string_alloc();
        FuriString* ground_result = furi_string_alloc();
        FuriString* tat_result = furi_string_alloc();

        bool is_metro_data_present =
            mosgortrans_parse_transport_block(&data->block[32], metro_result);
        bool is_ground_data_present =
            mosgortrans_parse_transport_block(&data->block[28], ground_result);
        bool is_tat_data_present = mosgortrans_parse_transport_block(&data->block[16], tat_result);

        furi_string_cat_printf(parsed_data, "\e#Troyka card\n");
        if(is_metro_data_present && !furi_string_empty(metro_result)) {
            render_section_header(parsed_data, "Metro", 22, 21);
            furi_string_cat_printf(parsed_data, "%s\n", furi_string_get_cstr(metro_result));
        }

        if(is_ground_data_present && !furi_string_empty(ground_result)) {
            render_section_header(parsed_data, "Ediny", 22, 22);
            furi_string_cat_printf(parsed_data, "%s\n", furi_string_get_cstr(ground_result));
        }

        if(is_tat_data_present && !furi_string_empty(tat_result)) {
            render_section_header(parsed_data, "TAT", 24, 23);
            furi_string_cat_printf(parsed_data, "%s\n", furi_string_get_cstr(tat_result));
        }

        furi_string_free(tat_result);
        furi_string_free(ground_result);
        furi_string_free(metro_result);

        parsed = is_metro_data_present || is_ground_data_present || is_tat_data_present;
    } while(false);

    return parsed;
}

bool checked = false;

static NfcCommand troika_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcCommand command = NfcCommandContinue;
    const MfClassicPollerEvent* mfc_event = event.event_data;
    Metroflip* app = context;

    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardDetected);
        command = NfcCommandContinue;
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardLost);
        app->sec_num = 0;
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;

    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        MfClassicKey key = {0};
        MfClassicKeyType key_type = MfClassicKeyTypeA;
        bit_lib_num_to_bytes_be(troika_1k_keys[app->sec_num].a, COUNT_OF(key.data), key.data);
        if(!checked) {
            mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
            app->sec_num++;
            checked = true;
        }
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        if(mfc_data->type == MfClassicType1k) {
            bit_lib_num_to_bytes_be(troika_1k_keys[app->sec_num].a, COUNT_OF(key.data), key.data);

            mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
            if(app->sec_num == 16) {
                mfc_event->data->read_sector_request_data.key_provided = false;
                app->sec_num = 0;
            }
            app->sec_num++;
        } else if(mfc_data->type == MfClassicType4k) {
            bit_lib_num_to_bytes_be(troika_4k_keys[app->sec_num].a, COUNT_OF(key.data), key.data);

            mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
            if(app->sec_num == 40) {
                mfc_event->data->read_sector_request_data.key_provided = false;
                app->sec_num = 0;
            }
            app->sec_num++;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        FuriString* parsed_data = furi_string_alloc();
        Widget* widget = app->widget;
        if(!troika_parse(parsed_data, mfc_data)) {
            furi_string_reset(app->text_box_store);
            FURI_LOG_I(TAG, "Unknown card type");
            furi_string_printf(parsed_data, "\e#Unknown card\n");
        }
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Save", metroflip_save_widget_callback, app);

        furi_string_free(parsed_data);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        metroflip_app_blink_stop(app);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_I(TAG, "fail");
        command = NfcCommandStop;
    }

    return command;
}

static void troika_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    app->sec_num = 0;

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!troika_parse(parsed_data, mfc_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            mf_classic_free(mfc_data);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
    } else {
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        nfc_poller_start(app->poller, troika_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool troika_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void troika_on_exit(Metroflip* app) {
    widget_reset(app->widget);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin troika_plugin = {
    .card_name = "Troika",
    .plugin_on_enter = troika_on_enter,
    .plugin_on_event = troika_on_event,
    .plugin_on_exit = troika_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor troika_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &troika_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* troika_plugin_ep(void) {
    return &troika_plugin_descriptor;
}

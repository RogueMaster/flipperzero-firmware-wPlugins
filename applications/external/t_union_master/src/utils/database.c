#include "database.h"
#include <lib/flipper_format/flipper_format.h>

// 查询卡种类型名
void tum_db_get_card_type_name(TunionCardType card_type, FuriString* card_type_name) {
    switch(card_type) {
    case TunionCardTypeNormal:
        furi_string_set(card_type_name, "普通卡");
        break;
    case TunionCardTypeStudent:
        furi_string_set(card_type_name, "学生卡");
        break;
    case TunionCardTypeElder:
        furi_string_set(card_type_name, "老人卡");
        break;
    case TunionCardTypeTest:
        furi_string_set(card_type_name, "测试卡");
        break;
    case TunionCardTypeSoldier:
        furi_string_set(card_type_name, "军人卡");
        break;
    case TunionCardTypeNone:
    default:
        furi_string_set(card_type_name, "未知");
        break;
    }
}

// 查询线路类型名
void tum_db_get_line_type_name(LineType line_type, FuriString* line_type_name) {
    switch(line_type) {
    case LineTypeMetro:
        furi_string_set(line_type_name, "地铁");
        break;
    case LineTypeBRT:
        furi_string_set(line_type_name, "快速公交");
        break;
    case LineTypeTram:
        furi_string_set(line_type_name, "有轨电车");
        break;
    case LineTypeTrain:
        furi_string_set(line_type_name, "火车");
        break;
    case LineTypeBUS:
        furi_string_set(line_type_name, "公交");
        break;
    case LineTypeUnknown:
    default:
        furi_string_set(line_type_name, "未知");
        break;
    }
}

// 转换线路类型
LineType tum_db_parse_line_type(const char* line_type_str) {
    LineType line_type;
    if(!strncmp(line_type_str, "mtr", 3))
        line_type = LineTypeMetro;
    else if(!strncmp(line_type_str, "brt", 3))
        line_type = LineTypeBRT;
    else if(!strncmp(line_type_str, "trm", 3))
        line_type = LineTypeTram;
    else if(!strncmp(line_type_str, "trn", 3))
        line_type = LineTypeTrain;
    else if(!strncmp(line_type_str, "bus", 3))
        line_type = LineTypeBUS;
    else
        line_type = LineTypeUnknown;
    return line_type;
}

// 查询卡号对应卡名
bool tum_db_query_card_name(Storage* storage, const char* card_number, FuriString* card_name) {
    furi_assert(storage);
    bool parsed = false;
    static const uint8_t prefix_length_tab[] = {13, 12, 11, 10, 9, 8};

    FlipperFormat* file = flipper_format_buffered_file_alloc(storage);
    FuriString* key = furi_string_alloc();
    do {
        if(!flipper_format_buffered_file_open_existing(file, APP_ASSETS_PATH("card_name.txt")))
            break;

        for(uint8_t i = 0; i < COUNT_OF(prefix_length_tab); i++) {
            furi_string_reset(key);
            furi_string_set_strn(key, card_number, prefix_length_tab[i]);

            if(flipper_format_read_string(file, furi_string_get_cstr(key), card_name))
                parsed = true;

            if(parsed) break;
            flipper_format_rewind(file);
        }

    } while(false);

    furi_string_free(key);
    flipper_format_buffered_file_close(file);
    flipper_format_free(file);
    return parsed;
}

// 查询城市码对应城市
bool tum_db_query_city_name(Storage* storage, const char* city_id, FuriString* city_name) {
    furi_assert(storage);
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(storage);

    do {
        if(!flipper_format_file_open_existing(file, APP_ASSETS_PATH("city_code.txt"))) break;
        if(!flipper_format_read_string(file, city_id, city_name)) break;
        parsed = true;
    } while(false);

    flipper_format_file_close(file);
    flipper_format_free(file);
    return parsed;
}

// 查询线路标签
bool tum_db_query_line_label(
    Storage* storage,
    const char* city_id,
    const char* station_id,
    FuriString* line_label) {
    furi_assert(storage);
    bool parsed = false;

    FuriString* file_name = furi_string_alloc_printf(
        "%s/stations/%s/M-prefix-map.txt", STORAGE_APP_ASSETS_PATH_PREFIX, city_id);
    FuriString* prefix = furi_string_alloc();
    FlipperFormat* file = flipper_format_buffered_file_alloc(storage);
    uint32_t perfix_len_lst[4] = {0};
    uint32_t perfix_len_cnt = 0;

    do {
        if(!flipper_format_buffered_file_open_existing(file, furi_string_get_cstr(file_name)))
            break;

        // 读前缀长度
        if(!flipper_format_get_value_count(file, "prefix-len", &perfix_len_cnt)) break;
        if(perfix_len_cnt > 4) break;
        if(!flipper_format_read_uint32(file, "prefix-len", perfix_len_lst, perfix_len_cnt)) break;

        // 前缀反查线路标签
        for(uint8_t perfix_len_idx = 0; perfix_len_idx < perfix_len_cnt; perfix_len_idx++) {
            uint32_t perfix_len = perfix_len_lst[perfix_len_idx];
            furi_string_set_strn(prefix, station_id, perfix_len);
            if(flipper_format_read_string(file, furi_string_get_cstr(prefix), line_label)) {
                parsed = true;
                break;
            }
            furi_string_reset(prefix);
            flipper_format_rewind(file);
        }
    } while(false);
    flipper_format_buffered_file_close(file);
    furi_string_reset(prefix);
    furi_string_reset(file_name);

    flipper_format_free(file);
    furi_string_free(file_name);
    furi_string_free(prefix);
    return parsed;
}

// 查询线路名+类型
bool tum_db_query_line_name(
    Storage* storage,
    const char* city_id,
    const char* line_label,
    FuriString* line_name,
    LineType* line_type) {
    furi_assert(storage);
    bool parsed = false;

    FuriString* file_name = furi_string_alloc_printf(
        "%s/stations/%s/L-%s.txt", STORAGE_APP_ASSETS_PATH_PREFIX, city_id, line_label);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* line_type_str = furi_string_alloc();

    do {
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(file_name))) break;
        if(!flipper_format_read_string(file, "name", line_name)) break;
        if(!flipper_format_read_string(file, "type", line_type_str)) break;
        *line_type = tum_db_parse_line_type(furi_string_get_cstr(line_type_str));
        parsed = true;
    } while(false);

    furi_string_free(file_name);
    furi_string_free(line_type_str);
    flipper_format_file_close(file);
    flipper_format_free(file);
    return parsed;
}

// 查询站台名
bool tum_db_query_station_name(
    Storage* storage,
    const char* city_id,
    const char* line_label,
    const char* station_id,
    FuriString* station_name) {
    furi_assert(storage);
    bool parsed = false;

    FuriString* file_name = furi_string_alloc_printf(
        "%s/stations/%s/L-%s.txt", STORAGE_APP_ASSETS_PATH_PREFIX, city_id, line_label);
    FuriString* real_station_id = furi_string_alloc();
    FlipperFormat* file = flipper_format_file_alloc(storage);
    uint32_t station_id_len_lst[4] = {0};
    uint32_t station_id_len_cnt = 0;

    do {
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(file_name))) break;

        // 读站台id长度
        if(!flipper_format_get_value_count(file, "len", &station_id_len_cnt)) break;
        if(station_id_len_cnt > 4) break;
        if(!flipper_format_read_uint32(file, "len", station_id_len_lst, station_id_len_cnt)) break;

        // 站台id反查站台名
        for(uint8_t station_id_len_idx = 0; station_id_len_idx < station_id_len_cnt;
            station_id_len_idx++) {
            uint32_t station_id_len = station_id_len_lst[station_id_len_idx];
            furi_string_set_strn(real_station_id, station_id, station_id_len);
            if(flipper_format_read_string(
                   file, furi_string_get_cstr(real_station_id), station_name)) {
                parsed = true;
                break;
            }
            furi_string_reset(real_station_id);
            flipper_format_rewind(file);
        }

    } while(false);

    furi_string_free(file_name);
    furi_string_free(real_station_id);
    flipper_format_file_close(file);
    flipper_format_free(file);
    return parsed;
}

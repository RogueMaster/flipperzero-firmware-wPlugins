#include "dnie_country.h"

#include <string.h>

static const struct {
    const char* code;
    const char* name;
} COUNTRIES[] = {
    {"ESP", "Spain"},       {"USA", "USA"},          {"FRA", "France"},
    {"GBR", "UK"},          {"D", "Germany"},        {"DEU", "Germany"},
    {"ITA", "Italy"},       {"PRT", "Portugal"},     {"NLD", "Netherlands"},
    {"BEL", "Belgium"},     {"CHE", "Switzerland"},  {"AUT", "Austria"},
    {"IRL", "Ireland"},     {"POL", "Poland"},       {"SWE", "Sweden"},
    {"NOR", "Norway"},      {"DNK", "Denmark"},      {"FIN", "Finland"},
    {"GRC", "Greece"},      {"ROU", "Romania"},      {"CZE", "Czechia"},
    {"HUN", "Hungary"},     {"UKR", "Ukraine"},      {"RUS", "Russia"},
    {"TUR", "Turkey"},      {"CAN", "Canada"},       {"MEX", "Mexico"},
    {"BRA", "Brazil"},      {"ARG", "Argentina"},    {"COL", "Colombia"},
    {"CHL", "Chile"},       {"PER", "Peru"},         {"VEN", "Venezuela"},
    {"ECU", "Ecuador"},     {"URY", "Uruguay"},      {"CUB", "Cuba"},
    {"JPN", "Japan"},       {"CHN", "China"},        {"KOR", "South Korea"},
    {"IND", "India"},       {"IDN", "Indonesia"},    {"AUS", "Australia"},
    {"NZL", "New Zealand"}, {"SAU", "Saudi Arabia"}, {"ARE", "UAE"},
    {"ISR", "Israel"},      {"EGY", "Egypt"},        {"MAR", "Morocco"},
    {"DZA", "Algeria"},     {"ZAF", "South Africa"}, {"NGA", "Nigeria"},
    {"PHL", "Philippines"}, {"THA", "Thailand"},     {"VNM", "Vietnam"},
    {"SGP", "Singapore"},   {"MYS", "Malaysia"},     {"PAK", "Pakistan"},
    {"AND", "Andorra"},
};

const char* dnie_country_name(const char* code) {
    if(!code || !code[0]) return "";
    for(size_t i = 0; i < sizeof(COUNTRIES) / sizeof(COUNTRIES[0]); i++) {
        if(strcmp(code, COUNTRIES[i].code) == 0) return COUNTRIES[i].name;
    }
    return code;
}

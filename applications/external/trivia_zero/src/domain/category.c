#include "include/domain/category.h"

static const char *const k_es[8] = {
    "?",
    "Geografia",
    "Entretenimiento",
    "Historia",
    "Arte y Literatura",
    "Ciencia y Naturaleza",
    "Deportes y Ocio",
    "Cultura General",
};

static const char *const k_en[8] = {
    "?",
    "Geography",
    "Entertainment",
    "History",
    "Arts & Literature",
    "Science & Nature",
    "Sports & Leisure",
    "General Knowledge",
};

const char *category_name(uint8_t category_id, Lang lang) {
    if (category_id < 1u || category_id > 7u) {
        return "?";
    }
    return (lang == LangEs) ? k_es[category_id] : k_en[category_id];
}

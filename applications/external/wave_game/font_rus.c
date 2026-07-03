#include "font_rus.h"
#include <string.h>
#include <furi.h>
#include <furi_hal.h>

// Функция для отрисовки одного символа
static void draw_char(Canvas* canvas, uint8_t x, uint8_t y, const uint8_t* bitmap) {
    for(uint8_t py = 0; py < 8; py++) {
        uint8_t line = bitmap[py];
        for(uint8_t px = 0; px < 7; px++) {
            if(line & (1 << (6 - px))) {
                canvas_draw_dot(canvas, x + px, y + py);
            }
        }
    }
}

// Функция для получения битмапа английской буквы
static const uint8_t* get_english_char_bitmap(char c) {
    // Преобразуем строчные буквы в заглавные
    if(c >= 'a' && c <= 'z') {
        c = c - 'a' + 'A';
    }

    switch(c) {
    case 'A':
        return FONT_ENG_A;
    case 'B':
        return FONT_ENG_B;
    case 'C':
        return FONT_ENG_C;
    case 'D':
        return FONT_ENG_D;
    case 'E':
        return FONT_ENG_E;
    case 'F':
        return FONT_ENG_F;
    case 'G':
        return FONT_ENG_G;
    case 'H':
        return FONT_ENG_H;
    case 'I':
        return FONT_ENG_I;
    case 'J':
        return FONT_ENG_J;
    case 'K':
        return FONT_ENG_K;
    case 'L':
        return FONT_ENG_L;
    case 'M':
        return FONT_ENG_M;
    case 'N':
        return FONT_ENG_N;
    case 'O':
        return FONT_ENG_O;
    case 'P':
        return FONT_ENG_P;
    case 'Q':
        return FONT_ENG_Q;
    case 'R':
        return FONT_ENG_R;
    case 'S':
        return FONT_ENG_S;
    case 'T':
        return FONT_ENG_T;
    case 'U':
        return FONT_ENG_U;
    case 'V':
        return FONT_ENG_V;
    case 'W':
        return FONT_ENG_W;
    case 'X':
        return FONT_ENG_X;
    case 'Y':
        return FONT_ENG_Y;
    case 'Z':
        return FONT_ENG_Z;
    default:
        return NULL;
    }
}

// Функция для получения битмапа цифры
const uint8_t* get_number_bitmap(char c) {
    switch(c) {
    case '0':
        return FONT_NUM_0;
    case '1':
        return FONT_NUM_1;
    case '2':
        return FONT_NUM_2;
    case '3':
        return FONT_NUM_3;
    case '4':
        return FONT_NUM_4;
    case '5':
        return FONT_NUM_5;
    case '6':
        return FONT_NUM_6;
    case '7':
        return FONT_NUM_7;
    case '8':
        return FONT_NUM_8;
    case '9':
        return FONT_NUM_9;
    default:
        return NULL;
    }
}

// Функция для вычисления ширины текста
uint8_t measure_text_width(const char* str) {
    uint8_t width = 0;
    const unsigned char* p = (const unsigned char*)str;

    while(*p) {
        if(*p == 0xD0 || *p == 0xD1) {
            width += 6; // Ширина русской буквы
            p += 2;
        } else {
            width += 6; // Ширина английской буквы или цифры
            p++;
        }
    }

    return width;
}

// Отладочная функция для вывода кодов символов
static void debug_print_codes(const char* str) {
    const unsigned char* p = (const unsigned char*)str;
    FURI_LOG_I("Font", "=== Start of string ===");
    while(*p) {
        if(*p == 0xD0 || *p == 0xD1) {
            FURI_LOG_I("Font", "Russian char: %02X %02X", *p, *(p + 1));
            p += 2;
        } else {
            FURI_LOG_I("Font", "ASCII char: %02X ('%c')", *p, *p);
            p++;
        }
    }
    FURI_LOG_I("Font", "=== End of string ===");
}

// Функция для получения индекса русской буквы
static int get_russian_char_index(unsigned char c1, unsigned char c2) {
    FURI_LOG_I("Font", "Getting index for: %02X %02X", c1, c2);

    if(c1 == 0xD0) {
        if(c2 >= 0x90 && c2 <= 0xAF) {
            int index = c2 - 0x90;
            FURI_LOG_I("Font", "D0 char index: %d", index);
            return index;
        }
    } else if(c1 == 0xD1) {
        if(c2 >= 0x80 && c2 <= 0x8F) {
            int index = (c2 - 0x80) + (0xBF - 0x90 + 1);
            FURI_LOG_I("Font", "D1 char index: %d", index);
            return index;
        }
    }

    // Добавляем отладочный вывод для неизвестных комбинаций
    FURI_LOG_W("Font", "Unknown char combination: %02X %02X", c1, c2);
    return -1;
}

// Функция для получения битмапа маленькой английской буквы
const uint8_t* get_small_english_char_bitmap(char c) {
    switch(c) {
    case 'r':
        return FONT_ENG_SMALL_R;
    case 'u':
        return FONT_ENG_SMALL_U;
    case 'e':
        return FONT_ENG_SMALL_E;
    case 'n':
        return FONT_ENG_SMALL_N;
    default:
        return NULL;
    }
}

// Функция для отрисовки текста маленькими буквами
void draw_small_text(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* str) {
    uint8_t text_width = measure_text_width(str);
    uint8_t text_height = 8;

    uint8_t start_x = x;
    uint8_t start_y = y;

    if(horizontal == AlignCenter) {
        start_x = x - text_width / 2;
    } else if(horizontal == AlignRight) {
        start_x = x - text_width;
    }

    if(vertical == AlignCenter) {
        start_y = y - text_height / 2;
    } else if(vertical == AlignBottom) {
        start_y = y - text_height;
    }

    uint8_t current_x = start_x;
    const unsigned char* p = (const unsigned char*)str;

    while(*p) {
        const uint8_t* bitmap = get_small_english_char_bitmap(*p);
        if(bitmap) {
            draw_char(canvas, current_x, start_y, bitmap);
        }
        current_x += 6;
        p++;
    }
}

// Основная функция отрисовки текста (теперь без проверки маленьких букв)
void draw_russian_text(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* str) {
    debug_print_codes(str);

    uint8_t text_width = measure_text_width(str);
    uint8_t text_height = 8;

    uint8_t start_x = x;
    uint8_t start_y = y;

    if(horizontal == AlignCenter) {
        start_x = x - text_width / 2;
    } else if(horizontal == AlignRight) {
        start_x = x - text_width;
    }

    if(vertical == AlignCenter) {
        start_y = y - text_height / 2;
    } else if(vertical == AlignBottom) {
        start_y = y - text_height;
    }

    uint8_t current_x = start_x;
    const unsigned char* p = (const unsigned char*)str;

    while(*p) {
        if(*p == 0xD0 || *p == 0xD1) {
            // Русская буква
            unsigned char c2 = *(p + 1);
            int index = get_russian_char_index(*p, c2);

            if(index >= 0) {
                FURI_LOG_I("Font", "Drawing Russian char at index %d", index);
                if((size_t)index < sizeof(FONT_RUS) / sizeof(FONT_RUS[0])) {
                    draw_char(canvas, current_x, start_y, FONT_RUS[index]);
                } else {
                    FURI_LOG_E("Font", "Index out of bounds: %d", index);
                }
            } else {
                FURI_LOG_E("Font", "Invalid Russian char index");
            }
            current_x += 6;
            p += 2;
        } else if(*p >= '0' && *p <= '9') {
            // Используем кастомный шрифт для цифр
            const uint8_t* bitmap = get_number_bitmap(*p);
            if(bitmap) {
                FURI_LOG_I("Font", "Drawing number: %c", *p);
                draw_char(canvas, current_x, start_y, bitmap);
            } else {
                FURI_LOG_E("Font", "Failed to get bitmap for number: %c", *p);
            }
            current_x += 6;
            p++;
        } else if(*p == ' ') {
            // Пробел
            current_x += 6;
            p++;
        } else {
            // Английская буква
            const uint8_t* bitmap = get_english_char_bitmap(*p);
            if(bitmap) {
                draw_char(canvas, current_x, start_y, bitmap);
                current_x += 6;
            } else {
                FURI_LOG_W("Font", "Unknown char: %02X", *p);
                current_x += 6;
            }
            p++;
        }
    }
}

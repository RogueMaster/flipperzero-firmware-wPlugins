#include "morse.h"

#include <furi.h>

typedef struct {
    char character;
    const char* code;
} MorseEntry;

static const MorseEntry morse_table[] = {
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},   {'E', "."},     {'F', "..-."},
    {'G', "--."},   {'H', "...."},  {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
    {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},  {'Q', "--.-"},  {'R', ".-."},
    {'S', "..."},   {'T', "-"},     {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
    {'Y', "-.--"},  {'Z', "--.."},  {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
    {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
};

char flipper_blinker_morse_normalize_char(char c) {
    if((c >= 'a') && (c <= 'z')) {
        return c - ('a' - 'A');
    }
    return c;
}

bool flipper_blinker_morse_is_supported(char c) {
    c = flipper_blinker_morse_normalize_char(c);
    if(c == ' ') return true;
    return flipper_blinker_morse_get(c) != NULL;
}

const char* flipper_blinker_morse_get(char c) {
    c = flipper_blinker_morse_normalize_char(c);
    for(size_t i = 0; i < COUNT_OF(morse_table); i++) {
        if(morse_table[i].character == c) {
            return morse_table[i].code;
        }
    }
    return NULL;
}

size_t flipper_blinker_morse_count_supported(const char* text) {
    size_t count = 0;
    while(*text) {
        if(flipper_blinker_morse_is_supported(*text)) {
            count++;
        }
        text++;
    }
    return count;
}

uint32_t flipper_blinker_morse_unit_ms(uint8_t wpm) {
    if(wpm == 0) wpm = 1;
    return 1200U / wpm;
}

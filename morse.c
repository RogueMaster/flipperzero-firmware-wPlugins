#include "morse.h"

#include <stddef.h>

typedef struct {
    char c;
    const char* code;
} MorseEntry;

static const MorseEntry morse_table[] = {
    {'A', ".-"},      {'B', "-..."},    {'C', "-.-."},    {'D', "-.."},
    {'E', "."},       {'F', "..-."},    {'G', "--."},     {'H', "...."},
    {'I', ".."},      {'J', ".---"},    {'K', "-.-"},     {'L', ".-.."},
    {'M', "--"},      {'N', "-."},      {'O', "---"},     {'P', ".--."},
    {'Q', "--.-"},    {'R', ".-."},     {'S', "..."},     {'T', "-"},
    {'U', "..-"},     {'V', "...-"},    {'W', ".--"},     {'X', "-..-"},
    {'Y', "-.--"},    {'Z', "--.."},
    {'0', "-----"},   {'1', ".----"},   {'2', "..---"},   {'3', "...--"},
    {'4', "....-"},   {'5', "....."},   {'6', "-...."},   {'7', "--..."},
    {'8', "---.."},   {'9', "----."},
    {'.', ".-.-.-"},  {',', "--..--"},  {'?', "..--.."},  {'\'', ".----."},
    {'!', "-.-.--"},  {'/', "-..-."},   {'(', "-.--."},   {')', "-.--.-"},
    {'&', ".-..."},   {':', "---..."},  {';', "-.-.-."},  {'=', "-...-"},
    {'+', ".-.-."},   {'-', "-....-"},  {'_', "..--.-"},  {'"', ".-..-."},
    {'$', "...-..-"}, {'@', ".--.-."},
};

const char* morse_lookup(char c) {
    if(c >= 'a' && c <= 'z') c -= 32;
    for(size_t i = 0; i < sizeof(morse_table) / sizeof(morse_table[0]); i++) {
        if(morse_table[i].c == c) return morse_table[i].code;
    }
    return NULL;
}

// Arbol dicotomico estandar del Morse, por niveles:
//   nivel 1: E T
//   nivel 2: I A N M
//   nivel 3: S U R W D K G O
//   nivel 4: H V F (u) L (a) P J B X C Y Z Q (o) (ch)  -> los no-latinos quedan vacios
//   nivel 5: numeros 0-9 y signos & + = / (
const char morse_tree_letters[MORSE_TREE_NODES] = {
    0,
    'E', 'T',
    'I', 'A', 'N', 'M',
    'S', 'U', 'R', 'W', 'D', 'K', 'G', 'O',
    'H', 'V', 'F', 0,   'L', 0,   'P', 'J',
    'B', 'X', 'C', 'Y', 'Z', 'Q', 0,   0,
    '5', '4', 0,   '3', 0,   0,   0,   '2', // bajo H V F (u)
    '&', 0,   '+', 0,   0,   0,   0,   '1', // bajo L (a) P J
    '6', '=', '/', 0,   0,   0,   '(', 0, // bajo B X C Y
    '7', 0,   0,   0,   '8', 0,   '9', '0', // bajo Z Q (o) (ch)
};

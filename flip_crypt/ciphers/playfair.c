#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "playfair.h"

char* playfair_make_table(const char* keyword) {
    char* table_output = (char*)malloc(26);
    if (!table_output) return "memory allocation failed";

    int used[26] = {0}; // Track used letters (a-z), index 0 = 'a'
    size_t table_index = 0;

    // Process keyword: add unique letters to table, convert to lowercase, merge 'j' into 'i'
    for (size_t i = 0; keyword[i] != '\0'; i++) {
        char c = tolower(keyword[i]);
        if (c == 'j') c = 'i'; // Merge j to i

        if (c < 'a' || c > 'z') continue; // Skip non-alpha chars

        int idx = c - 'a';
        if (!used[idx]) {
            used[idx] = 1;
            table_output[table_index++] = c;
        }
    }

    // Fill the rest of the table with remaining letters (except 'j')
    for (char c = 'a'; c <= 'z'; c++) {
        if (c == 'j') continue; // skip 'j'
        int idx = c - 'a';
        if (!used[idx]) {
            used[idx] = 1;
            table_output[table_index++] = c;
        }
    }

    table_output[table_index] = '\0';

    return table_output;
}

void find_position(char letter, const char* table, int* row, int* col) {
    if (letter == 'j') letter = 'i'; // j maps to i
    for (int i = 0; i < 25; i++) {
        if (table[i] == letter) {
            *row = i / 5;
            *col = i % 5;
            return;
        }
    }
    // Not found (shouldn't happen), return -1
    *row = -1;
    *col = -1;
}

char* playfair_encrypt(const char* plaintext, const char* table) {
    size_t len = strlen(plaintext);
    // Max encrypted length can be up to twice plaintext length (worst case)
    char* prepared = (char*)malloc(len * 2 + 1);
    if (!prepared) return "memory allocation failed";

    // Step 1: Prepare plaintext as digraphs
    size_t prep_index = 0;
    for (size_t i = 0; i < len; i++) {
        char c = tolower(plaintext[i]);
        if (c < 'a' || c > 'z') continue;  // skip non-alpha chars
        if (c == 'j') c = 'i'; // j -> i

        // Add first letter of pair
        if (prep_index % 2 == 1 && prepared[prep_index - 1] == c) {
            // If duplicate letters in pair, insert 'x' before current letter
            prepared[prep_index++] = 'x';
        }

        prepared[prep_index++] = c;
    }
    // If odd length, add 'x' to last
    if (prep_index % 2 != 0) {
        prepared[prep_index++] = 'x';
    }
    prepared[prep_index] = '\0';

    // Step 2: Encrypt each digraph
    char* encrypted = (char*)malloc(prep_index + 1);
    if (!encrypted) {
        free(prepared);
        return "memory allocation failed";
    }

    for (size_t i = 0; i < prep_index; i += 2) {
        int r1, c1, r2, c2;
        find_position(prepared[i], table, &r1, &c1);
        find_position(prepared[i+1], table, &r2, &c2);

        if (r1 == r2) {
            // Same row: replace with letter to right (wrap around)
            encrypted[i]   = table[r1 * 5 + (c1 + 1) % 5];
            encrypted[i+1] = table[r2 * 5 + (c2 + 1) % 5];
        } else if (c1 == c2) {
            // Same column: replace with letter below (wrap around)
            encrypted[i]   = table[((r1 + 1) % 5) * 5 + c1];
            encrypted[i+1] = table[((r2 + 1) % 5) * 5 + c2];
        } else {
            // Rectangle swap: swap columns
            encrypted[i]   = table[r1 * 5 + c2];
            encrypted[i+1] = table[r2 * 5 + c1];
        }
    }
    encrypted[prep_index] = '\0';

    free(prepared);
    return encrypted;
}

char* playfair_decrypt(const char* ciphertext, const char* table) {
    size_t len = strlen(ciphertext);
    if (len % 2 != 0) {
        return "text to decrypt must be of an even length";
    }

    // Allocate and convert ciphertext to lowercase
    char* lower_text = (char*)malloc(len + 1);
    if (!lower_text) {
        return "memory allocation failure";
    }
    for (size_t i = 0; i < len; ++i) {
        lower_text[i] = tolower((unsigned char)ciphertext[i]);
    }
    lower_text[len] = '\0';

    // Allocate memory for the decrypted text
    char* decrypted = (char*)malloc(len + 1);
    if (!decrypted) {
        free(lower_text);
        return "memory allocation failure";
    }

    for (size_t i = 0; i < len; i += 2) {
        int r1, c1, r2, c2;
        find_position(lower_text[i], table, &r1, &c1);
        find_position(lower_text[i+1], table, &r2, &c2);

        if (r1 == r2) {
            // Same row: letter to the left (wrap around)
            decrypted[i]   = table[r1 * 5 + (c1 + 5 - 1) % 5];
            decrypted[i+1] = table[r2 * 5 + (c2 + 5 - 1) % 5];
        } else if (c1 == c2) {
            // Same column: letter above (wrap around)
            decrypted[i]   = table[((r1 + 5 - 1) % 5) * 5 + c1];
            decrypted[i+1] = table[((r2 + 5 - 1) % 5) * 5 + c2];
        } else {
            // Rectangle swap: swap columns
            decrypted[i]   = table[r1 * 5 + c2];
            decrypted[i+1] = table[r2 * 5 + c1];
        }
    }

    decrypted[len] = '\0';
    free(lower_text);
    return decrypted;
}


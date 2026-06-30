char* playfair_make_table(const char* keyword);
void find_position(char letter, const char* table, int* row, int* col);
char* playfair_encrypt(const char* plaintext, const char* table);
char* playfair_decrypt(const char* ciphertext, const char* table);
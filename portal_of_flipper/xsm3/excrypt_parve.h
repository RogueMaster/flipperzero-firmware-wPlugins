#ifndef EXCRYPT_PARVE_H_
#define EXCRYPT_PARVE_H_
// "Parve" functions, seem to be used during controller auth

void ExCryptParveEcb(const uint8_t* key, const uint8_t* sbox, const uint8_t* input, uint8_t* output);
void ExCryptParveCbcMac(const uint8_t* key, const uint8_t* sbox, const uint8_t* iv, const uint8_t* input, uint32_t input_size, uint8_t* output);
void ExCryptChainAndSumMac(const uint32_t* cd, const uint32_t* ab, const uint32_t* input, uint32_t input_dwords, uint32_t* output);

#endif // EXCRYPT_PARVE_H_
#include "excrypt.h"
#include "excrypt_des_data.h"
#include <string.h>

#define LODWORD(_qw)    ((uint32_t)(_qw))
#define HIDWORD(_qw)    ((uint32_t)(((_qw) >> 32) & 0xffffffff))

// DES code based on https://github.com/fffaraz/cppDES

void ExCryptDesParity(const uint8_t* input, uint32_t input_size, uint8_t* output)
{
  for (uint32_t i = 0; i < input_size; i++)
  {
    uint8_t parity = input[i];

    parity ^= parity >> 4;
    parity ^= parity >> 2;
    parity ^= parity >> 1;

    output[i] = (input[i] & 0xFE) | (~parity & 1);
  }
}

void ExCryptDesKey(EXCRYPT_DES_STATE* state, const uint8_t* key)
{
  uint64_t qkey = SWAP64(*(const uint64_t*)key);

  // initial key schedule calculation
  uint64_t permuted_choice_1 = 0; // 56 bits
  for (int i = 0; i < 56; i++)
  {
    permuted_choice_1 <<= 1;
    permuted_choice_1 |= (qkey >> (64 - PC1[i])) & LB64_MASK;
  }

  // 28 bits
  uint32_t C = (uint32_t)((permuted_choice_1 >> 28) & 0x000000000fffffff);
  uint32_t D = (uint32_t)(permuted_choice_1 & 0x000000000fffffff);

  // Calculation of the 16 keys
  for (int i = 0; i < 16; i++)
  {
    // key schedule, shifting Ci and Di
    for (int j = 0; j < ITERATION_SHIFT[i]; j++)
    {
      C = (0x0fffffff & (C << 1)) | (0x00000001 & (C >> 27));
      D = (0x0fffffff & (D << 1)) | (0x00000001 & (D >> 27));
    }

    uint64_t permuted_choice_2 = (((uint64_t)C) << 28) | (uint64_t)D;

    uint64_t sub_key = 0; // 48 bits (2*24)
    for (int j = 0; j < 48; j++)
    {
      sub_key <<= 1;
      sub_key |= (permuted_choice_2 >> (56 - PC2[j])) & LB64_MASK;
    }
    state->keytab[i] = sub_key;
  }
}

uint32_t f(uint32_t R, uint64_t k)
{
  // applying expansion permutation and returning 48-bit data
  uint64_t s_input = 0;
  for (int i = 0; i < 48; i++)
  {
    s_input <<= 1;
    s_input |= (uint64_t)((R >> (32 - EXPANSION[i])) & LB32_MASK);
  }

  // XORing expanded Ri with Ki, the round key
  s_input = s_input ^ k;

  // applying S-Boxes function and returning 32-bit data
  uint32_t s_output = 0;
  for (int i = 0; i < 8; i++)
  {
    // Outer bits
    char row = (char)((s_input & (0x0000840000000000 >> 6 * i)) >> (42 - 6 * i));
    row = (row >> 4) | (row & 0x01);

    // Middle 4 bits of input
    char column = (char)((s_input & (0x0000780000000000 >> 6 * i)) >> (43 - 6 * i));

    s_output <<= 4;
    s_output |= (uint32_t)(SBOX[i][16 * row + column] & 0x0f);
  }

  // applying the round permutation
  uint32_t f_result = 0;
  for (int i = 0; i < 32; i++)
  {
    f_result <<= 1;
    f_result |= (s_output >> (32 - PBOX[i])) & LB32_MASK;
  }

  return f_result;
}

void feistel(uint32_t* L, uint32_t* R, uint32_t F)
{
  uint32_t temp = *R;
  *R = *L ^ F;
  *L = temp;
}

void ExCryptDesEcb(const EXCRYPT_DES_STATE* state, const uint8_t* input, uint8_t* output, uint8_t encrypt)
{
  uint64_t block;
  memcpy(&block, input, sizeof(uint64_t));
  block = SWAP64(block);
  //uint64_t block = SWAP64(*(uint64_t*)input)


  // initial permutation
  uint64_t result = 0;
  for (int i = 0; i < 64; i++)
  {
    result <<= 1;
    result |= (block >> (64 - IP[i])) & LB64_MASK;
  }

  // dividing T' into two 32-bit parts
  uint32_t L = HIDWORD(result);
  uint32_t R = LODWORD(result);

  // 16 rounds
  for (int i = 0; i < 16; i++)
  {
    uint32_t F = !encrypt ? f(R, state->keytab[15 - i]) : f(R, state->keytab[i]);
    feistel(&L, &R, F);
  }

  // swapping the two parts
  block = (((uint64_t)R) << 32) | (uint64_t)L;

  // inverse initial permutation
  result = 0;
  for (int i = 0; i < 64; i++)
  {
    result <<= 1;
    result |= (block >> (64 - FP[i])) & LB64_MASK;
  }
  result = SWAP64(result);
  memcpy(output, &result, sizeof(result));
}

void ExCryptDes3Key(EXCRYPT_DES3_STATE* state, const uint64_t* keys)
{
  ExCryptDesKey(&state->des_state[0], (const uint8_t*)& keys[0]);
  ExCryptDesKey(&state->des_state[1], (const uint8_t*)& keys[1]);
  ExCryptDesKey(&state->des_state[2], (const uint8_t*)& keys[2]);
}

void ExCryptDes3Ecb(const EXCRYPT_DES3_STATE* state, const uint8_t* input, uint8_t* output, uint8_t encrypt)
{
  if (encrypt)
  {
    ExCryptDesEcb(&state->des_state[0], input, output, encrypt);
    ExCryptDesEcb(&state->des_state[1], output, output, !encrypt);
    ExCryptDesEcb(&state->des_state[2], output, output, encrypt);
  }
  else
  {
    ExCryptDesEcb(&state->des_state[2], input, output, encrypt);
    ExCryptDesEcb(&state->des_state[1], output, output, !encrypt);
    ExCryptDesEcb(&state->des_state[0], output, output, encrypt);
  }
}

void ExCryptDes3Cbc(const EXCRYPT_DES3_STATE* state, const uint8_t* input, uint32_t input_size, uint8_t* output, uint8_t* feed, uint8_t encrypt)
{
  uint64_t last_block = *(uint64_t*)feed;
  for (uint32_t i = 0; i < input_size / 8; i++)
  {
    if (encrypt) {
      uint64_t temp;
      memcpy(&temp, input, sizeof(temp));
      temp = temp ^ last_block;
      memcpy(output, &temp, sizeof(temp));
      ExCryptDes3Ecb(state, output, output, encrypt);
      memcpy(&last_block, output, sizeof(last_block));
    }
    else
    {
      ExCryptDes3Ecb(state, input, output, encrypt);
      uint64_t temp;
      memcpy(&temp, output, sizeof(temp));
      temp = temp ^ last_block;
      memcpy(output, &temp, sizeof(temp));
      memcpy(&last_block, input, sizeof(last_block));
    }
    input += 8;
    output += 8;
  }
  *(uint64_t*)feed = last_block;
}

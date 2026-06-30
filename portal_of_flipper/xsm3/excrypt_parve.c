#include <stdlib.h>
#include <string.h>

#include "excrypt.h"

void ExCryptParveEcb(const uint8_t* key, const uint8_t* sbox, const uint8_t* input, uint8_t* output)
{
  uint8_t block[9];

  memcpy(block, input, 8);
  block[8] = block[0];

  for (int i = 8; i > 0; i--)
  {
    for (int j = 0; j < 8; j++)
    {
      uint8_t x = key[j] + block[j] + i;
      uint8_t y = sbox[x] + block[j + 1];
      block[j + 1] = ROTL8(y, 1);
    }

    block[0] = block[8];
  }

  memcpy(output, block, 8);
}

void ExCryptParveCbcMac(const uint8_t* key, const uint8_t* sbox, const uint8_t* iv, const uint8_t* input, uint32_t input_size, uint8_t* output)
{
  uint64_t block;
  uint64_t temp;
  memcpy(&block, iv, 8);

  if (input_size >= 8)
  {
    for (uint32_t i = 0; i < input_size / 8; i++)
    {
      memcpy(&temp, input + (i * 8), sizeof(temp));
      block ^= temp;
      ExCryptParveEcb(key, sbox, (uint8_t*)&block, (uint8_t*)&block);
    }
  }

  memcpy(output, &block, 8);
}

void ExCryptChainAndSumMac(const uint32_t* cd, const uint32_t* ab, const uint32_t* input, uint32_t input_dwords, uint32_t* output)
{
  uint64_t out0 = 0;
  uint64_t out1 = 0;

  uint32_t ab0 = SWAP32(ab[0]) % 0x7FFFFFFF;
  uint32_t ab1 = SWAP32(ab[1]) % 0x7FFFFFFF;
  uint32_t cd0 = SWAP32(cd[0]) % 0x7FFFFFFF;
  uint32_t cd1 = SWAP32(cd[1]) % 0x7FFFFFFF;

  for (uint32_t i = 0; i < input_dwords / 2; i++)
  {
    out0 += (uint64_t)SWAP32(input[0]) * 0xE79A9C1;
    out0 = (out0 % 0x7FFFFFFF) * ab0;
    out0 += ab1;
    out0 = out0 % 0x7FFFFFFF;

    out1 += out0;

    out0 = (uint64_t)(SWAP32(input[1]) + out0) * cd0;
    out0 = (out0 % 0x7FFFFFFF) + cd1;
    out0 = out0 % 0x7FFFFFFF;

    out1 += out0;

    input += 2;
  }
  out0 = SWAP32((out0 + ab1) % 0x7FFFFFFF);
  out1 = SWAP32((out1 + cd1) % 0x7FFFFFFF);
  memcpy(output, &out0,  sizeof(uint32_t));
  memcpy(output+1, &out1,  sizeof(uint32_t));
  // output[0] = SWAP32((out0 + ab1) % 0x7FFFFFFF);
  // output[1] = SWAP32((out1 + cd1) % 0x7FFFFFFF);
}

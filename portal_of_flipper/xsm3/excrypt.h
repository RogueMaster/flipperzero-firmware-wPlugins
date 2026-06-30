#ifndef EXCRYPT_H_
#define EXCRYPT_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#if(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define SWAP16(i) i
#define SWAP32(i) i
#define SWAP64(i) i
#else
#define SWAP16(i) ((((i) & 0xFF) << 8 | ((i) >> 8) & 0xFF) & 0xFFFF)
#define SWAP32(i) ((((i) & 0xff) << 24) | (((i) & 0xff00) << 8) | (((i) & 0xff0000) >> 8) | (((i) >> 24) & 0xff))
#define SWAP64(i) ((SWAP32((i) & 0xFFFFFFFF) << 32) | (SWAP32(((i) >> 32) & 0xFFFFFFFF)))
#endif

#define U8V(data) ((uint8_t)(data) & 0xFF)
#define ROTL8(data, bits) (U8V((data) << (bits)) | ((data) >> (8 - (bits))))

#define U16V(data) ((uint16_t)(data) & 0xFFFF)
#define ROTL16(data, bits) (U16V((data) << (bits)) | ((data) >> (16 - (bits))))

#define U32V(data) ((uint32_t)(data) & 0xFFFFFFFF)
#define ROTL32(data, bits) (U32V((data) << (bits)) | ((data) >> (32 - (bits))))

#define ROTL64(data, bits) (((data) << (bits)) | ((data) >> (64 - (bits))))

typedef int BOOL;

#include "excrypt_des.h"
#include "excrypt_sha.h"
#include "excrypt_parve.h"
#ifdef __cplusplus
}
#endif

#endif // EXCRYPT_H_
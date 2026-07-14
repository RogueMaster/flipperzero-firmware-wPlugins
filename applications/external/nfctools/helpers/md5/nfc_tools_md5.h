#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Computes the MD5 digest of a memory buffer.
 *
 * @param[in]  data    Pointer to the source data.
 * @param[in]  len     Length of the data in bytes.
 * @param[out] digest  16-byte buffer that receives the result.
 */
void nfc_tools_md5(const uint8_t* data, size_t len, uint8_t digest[16]);

#ifdef __cplusplus
}
#endif

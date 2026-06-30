/*
 * (c) 2015-2017 Marcos Del Sol Vives
 * (c) 2016      javiMaD
 *
 * SPDX-License-Identifier: MIT
 */

#include "amiibo.h"
#include "util.h"
#include "mbedtls/md.h"
#include "mbedtls/aes.h"
#include <errno.h>
#include "portable_endian.h"

#define htobe16(x) __builtin_bswap16(x)
#define be16toh(x) __builtin_bswap16(x)

#define HMAC_POS_DATA 0x008
#define HMAC_POS_TAG 0x1B4

void nfc3d_amiibo_calc_seed(const uint8_t * dump, uint8_t * key) {
	memcpy(key + 0x00, dump + 0x029, 0x02);
	memset(key + 0x02, 0x00, 0x0E);
	memcpy(key + 0x10, dump + 0x1D4, 0x08);
	memcpy(key + 0x18, dump + 0x1D4, 0x08);
	memcpy(key + 0x20, dump + 0x1E8, 0x20);
}

void nfc3d_amiibo_keygen(const nfc3d_keygen_masterkeys * masterKeys, const uint8_t * dump, nfc3d_keygen_derivedkeys * derivedKeys) {
	uint8_t seed[NFC3D_KEYGEN_SEED_SIZE];

	nfc3d_amiibo_calc_seed(dump, seed);
	nfc3d_keygen(masterKeys, seed, derivedKeys);
}

void nfc3d_amiibo_cipher(const nfc3d_keygen_derivedkeys * keys, const uint8_t * in, uint8_t * out) {
	mbedtls_aes_context aes;
	size_t nc_off = 0;
	unsigned char nonce_counter[16];
	unsigned char stream_block[16];

	mbedtls_aes_setkey_enc( &aes, keys->aesKey, 128 );
	memset(nonce_counter, 0, sizeof(nonce_counter));
	memset(stream_block, 0, sizeof(stream_block));
	memcpy(nonce_counter, keys->aesIV, sizeof(nonce_counter));
	mbedtls_aes_crypt_ctr( &aes, 0x188, &nc_off, nonce_counter, stream_block, in + 0x02C, out + 0x02C );

	memcpy(out + 0x000, in + 0x000, 0x008);
	// Data signature NOT copied
	memcpy(out + 0x028, in + 0x028, 0x004);
	// Tag signature NOT copied
	memcpy(out + 0x1D4, in + 0x1D4, 0x034);
}

void nfc3d_amiibo_tag_to_internal(const uint8_t * tag, uint8_t * intl) {
	memcpy(intl + 0x000, tag + 0x008, 0x008);
	memcpy(intl + 0x008, tag + 0x080, 0x020);
	memcpy(intl + 0x028, tag + 0x010, 0x024);
	memcpy(intl + 0x04C, tag + 0x0A0, 0x168);
	memcpy(intl + 0x1B4, tag + 0x034, 0x020);
	memcpy(intl + 0x1D4, tag + 0x000, 0x008);
	memcpy(intl + 0x1DC, tag + 0x054, 0x02C);
}

void nfc3d_amiibo_internal_to_tag(const uint8_t * intl, uint8_t * tag) {
	memcpy(tag + 0x008, intl + 0x000, 0x008);
	memcpy(tag + 0x080, intl + 0x008, 0x020);
	memcpy(tag + 0x010, intl + 0x028, 0x024);
	memcpy(tag + 0x0A0, intl + 0x04C, 0x168);
	memcpy(tag + 0x034, intl + 0x1B4, 0x020);
	memcpy(tag + 0x000, intl + 0x1D4, 0x008);
	memcpy(tag + 0x054, intl + 0x1DC, 0x02C);
}

bool nfc3d_amiibo_unpack(const nfc3d_amiibo_keys * amiiboKeys, const uint8_t * tag, uint8_t * plain) {
	uint8_t internal[NFC3D_AMIIBO_SIZE];
	nfc3d_keygen_derivedkeys dataKeys;
	nfc3d_keygen_derivedkeys tagKeys;

	// Convert format
	nfc3d_amiibo_tag_to_internal(tag, internal);

	// Generate keys
	nfc3d_amiibo_keygen(&amiiboKeys->data, internal, &dataKeys);
	nfc3d_amiibo_keygen(&amiiboKeys->tag, internal, &tagKeys);

	// Decrypt
	nfc3d_amiibo_cipher(&dataKeys, internal, plain);

	// Regenerate tag HMAC. Note: order matters, data HMAC depends on tag HMAC!
	mbedtls_md_hmac( mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), tagKeys.hmacKey, sizeof(tagKeys.hmacKey),	
			 plain + 0x1D4, 0x34, plain + HMAC_POS_TAG );

	// Regenerate data HMAC
	mbedtls_md_hmac( mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), dataKeys.hmacKey, sizeof(dataKeys.hmacKey),	
			 plain + 0x029, 0x1DF, plain + HMAC_POS_DATA );

	return
			memcmp(plain + HMAC_POS_DATA, internal + HMAC_POS_DATA, 32) == 0 &&
			memcmp(plain + HMAC_POS_TAG, internal + HMAC_POS_TAG, 32) == 0;
}

void nfc3d_amiibo_pack(const nfc3d_amiibo_keys * amiiboKeys, const uint8_t * plain, uint8_t * tag) {
	uint8_t cipher[NFC3D_AMIIBO_SIZE];
	nfc3d_keygen_derivedkeys tagKeys;
	nfc3d_keygen_derivedkeys dataKeys;

	// Generate keys
	nfc3d_amiibo_keygen(&amiiboKeys->tag, plain, &tagKeys);
	nfc3d_amiibo_keygen(&amiiboKeys->data, plain, &dataKeys);

	// Generate tag HMAC
	mbedtls_md_hmac( mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), tagKeys.hmacKey, sizeof(tagKeys.hmacKey),	
			 plain + 0x1D4, 0x34, cipher + HMAC_POS_TAG );

	// Init mbedtls HMAC context
	mbedtls_md_context_t ctx;
	mbedtls_md_init( &ctx );
	mbedtls_md_setup( &ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1 );

	// Generate data HMAC
	mbedtls_md_hmac_starts( &ctx, dataKeys.hmacKey, sizeof(dataKeys.hmacKey) );
	mbedtls_md_hmac_update( &ctx, plain + 0x029, 0x18B ); // Data
	mbedtls_md_hmac_update( &ctx, cipher + HMAC_POS_TAG, 0x20 ); // Tag HMAC
	mbedtls_md_hmac_update( &ctx, plain + 0x1D4, 0x34 ); // Here be dragons

	mbedtls_md_hmac_finish( &ctx, cipher + HMAC_POS_DATA );

	// HMAC cleanup
	mbedtls_md_free( &ctx );

	// Encrypt
	nfc3d_amiibo_cipher(&dataKeys, plain, cipher);

	// Convert back to hardware
	nfc3d_amiibo_internal_to_tag(cipher, tag);
}

bool nfc3d_amiibo_load_keys(nfc3d_amiibo_keys * amiiboKeys, const char * path) {
	FILE * f = fopen(path, "rb");
	if (!f) {
		return false;
	}

	if (!fread(amiiboKeys, sizeof(*amiiboKeys), 1, f)) {
		fclose(f);
		return false;
	}
	fclose(f);

	if (
		(amiiboKeys->data.magicBytesSize > 16) ||
		(amiiboKeys->tag.magicBytesSize > 16)
	) {
		errno = EILSEQ;
		return false;
	}

	return true;
}


void nfc3d_amiibo_copy_app_data(const uint8_t * src, uint8_t * dst) {

	uint16_t *ami_nb_wr = (uint16_t*)(dst + 0x29);
	uint16_t *cfg_nb_wr = (uint16_t*)(dst + 0xB4);

	/* increment write counters */
	*ami_nb_wr = htobe16(be16toh(*ami_nb_wr) + 1);
	*cfg_nb_wr = htobe16(be16toh(*cfg_nb_wr) + 1);

	/* copy flags */
	dst[0x2C] = src[0x2C];
	/* copy programID */
	memcpy(dst + 0xAC, src + 0xAC, 8);
	/* copy AppID */
	memcpy(dst + 0xB6, src + 0xB6, 4);
	/* copy AppData */
	memcpy(dst + 0xDC, src + 0xDC, 216);
}


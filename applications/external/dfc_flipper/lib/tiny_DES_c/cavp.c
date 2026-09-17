/*
 * SPDX-License-Identifier: Unlicense
 *
 * Opt-in NIST CAVP response-file validation for TDES.
 *
 * Runs the complete TDES CAVP corpora checked in under test_vectors/cavp/:
 *   kat/  - Known Answer Tests (invperm, permop, subtab, varkey, vartext)
 *   mmt/  - Multi-block Message Tests (keying options 2 and 3)
 *   mct/  - Monte Carlo Tests per NIST SP 800-20 (keying options 2 and 3)
 *
 * This translation unit is part of the test executable only; it is never
 * linked into the library. Enable with DES_CAVP=1.
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>

#include "des.h"
#include "munit.h"

#ifndef CAVP_VECTOR_DIR
#define CAVP_VECTOR_DIR "test_vectors/cavp"
#endif

/* The corpus needs every mode and TDES; skip the whole TU otherwise */
#if defined(DES_CAVP) && (DES_CAVP == 1) && \
    (DES_ENABLE_ECB == 1) && (DES_ENABLE_CBC == 1) && (DES_ENABLE_CFB1 == 1) && (DES_ENABLE_CFB8 == 1) && \
    (DES_ENABLE_CFB64 == 1) && (DES_ENABLE_OFB == 1) && (DES_ENABLE_TDES == 1)

enum cavp_mode
{
  CAVP_TECB,
  CAVP_TCBC,
  CAVP_TCFB1,
  CAVP_TCFB8,
  CAVP_TCFB64,
  CAVP_TOFB
};

enum cavp_suite
{
  CAVP_KAT,
  CAVP_MMT,
  CAVP_MCT
};

/* MMT cases carry at most 10 blocks (80 bytes); KAT at most one block */
#define CAVP_MAX_DATA 96

struct cavp_record
{
  long count;
  uint8_t key[24];
  int have_key;
  uint8_t iv[8];
  int have_iv;
  uint8_t pt[CAVP_MAX_DATA];
  size_t pt_len; /* bytes, or bits for TCFB1 files */
  int have_pt;
  uint8_t ct[CAVP_MAX_DATA];
  size_t ct_len;
  int have_ct;
};

static void cavp_record_reset(struct cavp_record* r)
{
  r->have_key = r->have_iv = r->have_pt = r->have_ct = 0;
  r->pt_len = r->ct_len = 0;
}

static int cavp_hex_nibble(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

/* Parse a hex string into out; returns byte count or -1 */
static int cavp_parse_hex(const char* s, uint8_t* out, size_t max)
{
  size_t n = 0;
  while (s[0] != '\0' && s[0] != '\r' && s[0] != '\n')
  {
    int hi = cavp_hex_nibble(s[0]);
    int lo = cavp_hex_nibble(s[1]);
    if (hi < 0 || lo < 0 || n >= max)
      return -1;
    out[n++] = (uint8_t)((hi << 4) | lo);
    s += 2;
  }
  return (int)n;
}

/* Parse a binary digit string (CFB1 payloads) into MSB-first packed bits;
   returns bit count or -1 */
static int cavp_parse_bits(const char* s, uint8_t* out, size_t max_bytes)
{
  size_t n = 0;
  memset(out, 0, max_bytes);
  while (s[0] == '0' || s[0] == '1')
  {
    if (n / 8 >= max_bytes)
      return -1;
    if (s[0] == '1')
      out[n / 8] |= (uint8_t)(0x80U >> (n % 8));
    n++;
    s++;
  }
  if (s[0] != '\0' && s[0] != '\r' && s[0] != '\n')
    return -1;
  return (int)n;
}

/* If line begins with "name" followed by optional spaces and '=', return the
   value (trailing whitespace stripped in place is not needed; parsers stop at
   CR/LF). Otherwise NULL. */
static const char* cavp_value(char* line, const char* name)
{
  size_t n = strlen(name);
  char* p = line;
  if (strncmp(p, name, n) != 0)
    return NULL;
  p += n;
  if (*p != ' ' && *p != '\t' && *p != '=')
    return NULL;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p != '=')
    return NULL;
  p++;
  while (*p == ' ' || *p == '\t')
    p++;
  return p;
}

static void cavp_print_bytes(const char* label, const uint8_t* data, size_t len)
{
  size_t i;
  fprintf(stderr, "  %s = ", label);
  for (i = 0; i < len; i++)
    fprintf(stderr, "%02x", data[i]);
  fprintf(stderr, "\n");
}

/* ------------------------------------------------------------------------- */
/* KAT / MMT execution                                                       */
/* ------------------------------------------------------------------------- */

/* Apply the mode to buf in place. len is bytes (bits for CAVP_TCFB1). */
static void cavp_apply(int mode, int encrypt, const uint8_t key[24],
                       const uint8_t* iv, int have_iv, uint8_t* buf, size_t len)
{
  struct DES3_ctx ctx;
  size_t i;

  DES3_init_ctx(&ctx, key, 24);
  if (have_iv)
    DES3_ctx_set_iv(&ctx, iv);

  switch (mode)
  {
    case CAVP_TECB:
      for (i = 0; i < len; i += DES_BLOCKLEN)
      {
        if (encrypt)
          DES3_ECB_encrypt(&ctx, buf + i);
        else
          DES3_ECB_decrypt(&ctx, buf + i);
      }
      break;
    case CAVP_TCBC:
      if (encrypt)
        DES3_CBC_encrypt(&ctx, buf, len);
      else
        DES3_CBC_decrypt(&ctx, buf, len);
      break;
    case CAVP_TCFB1:
      if (encrypt)
        DES3_CFB1_encrypt(&ctx, buf, len);
      else
        DES3_CFB1_decrypt(&ctx, buf, len);
      break;
    case CAVP_TCFB8:
      if (encrypt)
        DES3_CFB8_encrypt(&ctx, buf, len);
      else
        DES3_CFB8_decrypt(&ctx, buf, len);
      break;
    case CAVP_TCFB64:
      if (encrypt)
        DES3_CFB64_encrypt(&ctx, buf, len);
      else
        DES3_CFB64_decrypt(&ctx, buf, len);
      break;
    default: /* CAVP_TOFB */
      DES3_OFB_crypt(&ctx, buf, len);
      break;
  }
}

static int cavp_standard_case(int mode, const char* file, int encrypt,
                              const struct cavp_record* r)
{
  uint8_t buf[CAVP_MAX_DATA];
  const uint8_t* input = encrypt ? r->pt : r->ct;
  const uint8_t* expected = encrypt ? r->ct : r->pt;
  size_t in_len = encrypt ? r->pt_len : r->ct_len;
  size_t out_len = encrypt ? r->ct_len : r->pt_len;
  size_t cmp_bytes = (mode == CAVP_TCFB1) ? (out_len + 7) / 8 : out_len;

  if (in_len != out_len || cmp_bytes > sizeof(buf))
  {
    fprintf(stderr, "CAVP malformed: %s Count=%ld\n", file, r->count);
    return 0;
  }
  memcpy(buf, input, (mode == CAVP_TCFB1) ? (in_len + 7) / 8 : in_len);
  cavp_apply(mode, encrypt, r->key, r->iv, r->have_iv, buf, in_len);
  if (memcmp(buf, expected, cmp_bytes) != 0)
  {
    fprintf(stderr, "CAVP failure: %s Count=%ld [%s]\n", file, r->count,
            encrypt ? "ENCRYPT" : "DECRYPT");
    cavp_print_bytes("expected", expected, cmp_bytes);
    cavp_print_bytes("actual  ", buf, cmp_bytes);
    return 0;
  }
  return 1;
}

/* ------------------------------------------------------------------------- */
/* Monte Carlo Test (NIST SP 800-20)                                         */
/* ------------------------------------------------------------------------- */

#define MCT_INNER 10000

/* Rolling history of the last 192 output bits (newest at hist[23] LSB end),
   used for the per-round key updates. */
static void mct_hist_push_block(uint8_t hist[24], const uint8_t block[8])
{
  memmove(hist, hist + 8, 16);
  memcpy(hist + 16, block, 8);
}

static void mct_hist_push_byte(uint8_t hist[24], uint8_t b)
{
  memmove(hist, hist + 1, 23);
  hist[23] = b;
}

static void mct_hist_push_bit(uint8_t hist[24], uint8_t bit)
{
  int i;
  for (i = 0; i < 23; i++)
    hist[i] = (uint8_t)((hist[i] << 1) | (hist[i + 1] >> 7));
  hist[23] = (uint8_t)((hist[23] << 1) | (bit & 1U));
}

static void mct_shift_iv_byte(uint8_t iv[8], uint8_t b)
{
  memmove(iv, iv + 1, 7);
  iv[7] = b;
}

static void mct_shift_iv_bit(uint8_t iv[8], uint8_t bit)
{
  int i;
  for (i = 0; i < 7; i++)
    iv[i] = (uint8_t)((iv[i] << 1) | (iv[i + 1] >> 7));
  iv[7] = (uint8_t)((iv[7] << 1) | (bit & 1U));
}

static void mct_set_odd_parity(uint8_t* key, size_t len)
{
  size_t i;
  for (i = 0; i < len; i++)
  {
    uint8_t b = key[i] >> 1;
    uint8_t parity = 0;
    while (b)
    {
      parity ^= b & 1U;
      b >>= 1;
    }
    key[i] = (uint8_t)((key[i] & 0xFEU) | (parity ^ 1U));
  }
}

/*
 * One MCT outer round: 10,000 inner iterations (SP 800-20 / CAVS rules,
 * verified against the NIST sample and intermediate-values files).
 *
 * Inner chaining per mode ("cur" is the running input, one block / byte / bit):
 *   TECB    enc: C=E(P);           P'=C            dec: mirrored
 *   TCBC    enc: C=E(P^CV);        P'=CV;   CV'=C  dec: P=D(C)^CV; CV'=C; C'=P
 *   TOFB    e/d: O=E(FB); R=in^O;  in'=FB;  FB'=O
 *   TCFB*   enc: O=E(CV); C=in^O;  in'=lead(CV); CV'=shift(CV,C)
 *           dec: O=E(CV); P=in^O;  CV'=shift(CV,in); in'=lead(O)
 * where lead() is the leading block/byte/bit of the register.
 *
 * After the loop, "cur" and the feedback register are already the next
 * round's text and IV -- except TOFB, whose next text is text0 ^ O[9998].
 *
 * Key update: K1 ^= last 64 output bits, K2 ^= the 64 before those,
 * K3 ^= the 64 before those (3-key) or the last 64 (2-key/1-key), then
 * restore odd parity.
 *
 * state->text/iv/key are updated in place; the round output (the value the
 * .rsp file records as CIPHERTEXT/PLAINTEXT) is written to result.
 */
struct mct_state
{
  uint8_t key[24];
  uint8_t iv[8]; /* unused for TECB */
  uint8_t text[8]; /* block; byte in text[0]; bit in text[0] bit 7 */
};

static void mct_round(int mode, int encrypt, struct mct_state* st,
                      uint8_t result[8])
{
  struct DES3_ctx ctx;
  uint8_t hist[24];
  uint8_t cur[8], cv[8], tmp[8], text0[8], last_ks[8];
  int j;

  DES3_init_ctx(&ctx, st->key, 24);
  memset(hist, 0, sizeof(hist));
  memcpy(cur, st->text, 8);
  memcpy(cv, st->iv, 8);
  memcpy(text0, st->text, 8);
  memset(last_ks, 0, sizeof(last_ks));

  for (j = 0; j < MCT_INNER; j++)
  {
    switch (mode)
    {
      case CAVP_TECB:
        if (encrypt)
          DES3_ECB_encrypt(&ctx, cur);
        else
          DES3_ECB_decrypt(&ctx, cur);
        mct_hist_push_block(hist, cur);
        break;

      case CAVP_TCBC:
        if (encrypt)
        {
          uint8_t k;
          for (k = 0; k < 8; k++)
            tmp[k] = (uint8_t)(cur[k] ^ cv[k]);
          DES3_ECB_encrypt(&ctx, tmp);
          mct_hist_push_block(hist, tmp);
          memcpy(cur, cv, 8); /* P' = CV */
          memcpy(cv, tmp, 8);
        }
        else
        {
          uint8_t k;
          memcpy(tmp, cur, 8);
          DES3_ECB_decrypt(&ctx, tmp);
          for (k = 0; k < 8; k++)
            tmp[k] ^= cv[k];
          mct_hist_push_block(hist, tmp);
          memcpy(cv, cur, 8); /* CV' = C */
          memcpy(cur, tmp, 8); /* C' = P */
        }
        break;

      case CAVP_TOFB:
      {
        uint8_t k;
        memcpy(tmp, cv, 8); /* cv doubles as the OFB feedback register */
        DES3_ECB_encrypt(&ctx, tmp); /* O = E(FB) */
        for (k = 0; k < 8; k++)
          last_ks[k] = (uint8_t)(cur[k] ^ tmp[k]); /* R = in ^ O (reuse) */
        mct_hist_push_block(hist, last_ks);
        memcpy(cur, cv, 8); /* in' = FB */
        memcpy(cv, tmp, 8); /* FB' = O */
        break;
      }

      case CAVP_TCFB64:
      {
        uint8_t k;
        memcpy(tmp, cv, 8);
        DES3_ECB_encrypt(&ctx, tmp); /* O = E(CV) */
        for (k = 0; k < 8; k++)
          tmp[k] ^= cur[k]; /* R = in ^ O; O = R ^ in later if needed */
        mct_hist_push_block(hist, tmp);
        if (encrypt)
        {
          memcpy(last_ks, cv, 8); /* remember CV for in' */
          memcpy(cv, tmp, 8); /* CV' = C */
          memcpy(cur, last_ks, 8); /* in' = CV */
        }
        else
        {
          uint8_t o[8];
          for (k = 0; k < 8; k++)
            o[k] = (uint8_t)(tmp[k] ^ cur[k]); /* recover keystream */
          memcpy(cv, cur, 8); /* CV' = C */
          memcpy(cur, o, 8); /* in' = O */
        }
        break;
      }

      case CAVP_TCFB8:
      {
        uint8_t o, res, in = cur[0];
        memcpy(tmp, cv, 8);
        DES3_ECB_encrypt(&ctx, tmp);
        o = tmp[0];
        res = (uint8_t)(in ^ o);
        mct_hist_push_byte(hist, res);
        if (encrypt)
        {
          cur[0] = cv[0]; /* in' = lead(CV) */
          mct_shift_iv_byte(cv, res);
        }
        else
        {
          mct_shift_iv_byte(cv, in);
          cur[0] = o; /* in' = keystream byte */
        }
        result[0] = res;
        break;
      }

      default: /* CAVP_TCFB1 */
      {
        uint8_t o, res, in = (uint8_t)(cur[0] >> 7);
        memcpy(tmp, cv, 8);
        DES3_ECB_encrypt(&ctx, tmp);
        o = (uint8_t)(tmp[0] >> 7);
        res = (uint8_t)(in ^ o);
        mct_hist_push_bit(hist, res);
        if (encrypt)
        {
          cur[0] = (uint8_t)((cv[0] >> 7) << 7); /* in' = lead(CV) */
          mct_shift_iv_bit(cv, res);
        }
        else
        {
          mct_shift_iv_bit(cv, in);
          cur[0] = (uint8_t)(o << 7); /* in' = keystream bit */
        }
        result[0] = (uint8_t)(res << 7);
        break;
      }
    }
  }

  /* Round output = the last 64 output bits */
  if (mode == CAVP_TCFB8)
    result[0] = hist[23];
  else if (mode != CAVP_TCFB1)
    memcpy(result, hist + 16, 8);
  else
    result[0] = (uint8_t)((hist[23] & 1U) << 7);

  /* Next-round text and IV */
  if (mode == CAVP_TOFB)
  {
    uint8_t k;
    for (k = 0; k < 8; k++)
      st->text[k] = (uint8_t)(text0[k] ^ cur[k]); /* cur = O[9998] here */
  }
  else
  {
    memcpy(st->text, cur, 8);
  }
  memcpy(st->iv, cv, 8);

  /* Key update with odd-parity restoration */
  {
    int two_key = (memcmp(st->key, st->key + 16, 8) == 0);
    int one_key = two_key && (memcmp(st->key, st->key + 8, 8) == 0);
    uint8_t k;
    for (k = 0; k < 8; k++)
    {
      st->key[k] ^= hist[16 + k];
      st->key[8 + k] ^= one_key ? hist[16 + k] : hist[8 + k];
      st->key[16 + k] ^= (two_key || one_key) ? hist[16 + k] : hist[k];
    }
    mct_set_odd_parity(st->key, 24);
  }
}

/* ------------------------------------------------------------------------- */
/* File driver                                                               */
/* ------------------------------------------------------------------------- */

struct mct_chain
{
  struct mct_state st;
  int active;
};

static int cavp_mct_case(int mode, const char* file, int encrypt,
                         const struct cavp_record* r, struct mct_chain* chain)
{
  const uint8_t* input = encrypt ? r->pt : r->ct;
  const uint8_t* expected = encrypt ? r->ct : r->pt;
  size_t cmp = (mode == CAVP_TCFB1 || mode == CAVP_TCFB8) ? 1 : 8;
  uint8_t result[8];

  if (!chain->active)
  {
    memcpy(chain->st.key, r->key, 24);
    memcpy(chain->st.iv, r->have_iv ? r->iv : (const uint8_t*)"\0\0\0\0\0\0\0\0", 8);
    memset(chain->st.text, 0, 8);
    memcpy(chain->st.text, input, cmp);
    chain->active = 1;
  }
  else
  {
    /* Our computed chain state must reproduce the recorded case inputs */
    if (memcmp(chain->st.key, r->key, 24) != 0 ||
        (r->have_iv && memcmp(chain->st.iv, r->iv, 8) != 0) ||
        memcmp(chain->st.text, input, cmp) != 0)
    {
      fprintf(stderr, "CAVP MCT chain mismatch: %s Count=%ld [%s]\n", file,
              r->count, encrypt ? "ENCRYPT" : "DECRYPT");
      cavp_print_bytes("state key ", chain->st.key, 24);
      cavp_print_bytes("record key", r->key, 24);
      if (r->have_iv)
      {
        cavp_print_bytes("state iv  ", chain->st.iv, 8);
        cavp_print_bytes("record iv ", r->iv, 8);
      }
      cavp_print_bytes("state txt ", chain->st.text, cmp);
      cavp_print_bytes("record txt", input, cmp);
      chain->active = 0;
      return 0;
    }
  }

  mct_round(mode, encrypt, &chain->st, result);
  if (memcmp(result, expected, cmp) != 0)
  {
    fprintf(stderr, "CAVP MCT failure: %s Count=%ld [%s]\n", file, r->count,
            encrypt ? "ENCRYPT" : "DECRYPT");
    cavp_print_bytes("expected", expected, cmp);
    cavp_print_bytes("actual  ", result, cmp);
    chain->active = 0;
    return 0;
  }
  return 1;
}

/* Determine mode from a CAVP file name like "TCFB64Monte3.rsp".
   Returns -1 on unknown names. */
static int cavp_mode_from_name(const char* name)
{
  if (name[0] != 'T')
    return -1;
  name++;
  if (strncmp(name, "ECB", 3) == 0)
    return CAVP_TECB;
  if (strncmp(name, "CBC", 3) == 0)
    return CAVP_TCBC;
  if (strncmp(name, "CFB64", 5) == 0)
    return CAVP_TCFB64;
  if (strncmp(name, "CFB8", 4) == 0)
    return CAVP_TCFB8;
  if (strncmp(name, "CFB1", 4) == 0)
    return CAVP_TCFB1;
  if (strncmp(name, "OFB", 3) == 0)
    return CAVP_TOFB;
  return -1;
}

static int cavp_run_file(const char* subdir, const char* filename)
{
  char path[512];
  char line[512];
  FILE* f;
  struct cavp_record rec;
  struct mct_chain chain;
  int mode = cavp_mode_from_name(filename);
  int suite = (strstr(filename, "Monte") != NULL) ? CAVP_MCT :
              (strstr(filename, "MMT") != NULL) ? CAVP_MMT : CAVP_KAT;
  int bitmode = (mode == CAVP_TCFB1);
  int encrypt = 1;
  int ok = 1;
  long cases = 0;

  if (mode < 0)
  {
    fprintf(stderr, "CAVP unknown file name: %s\n", filename);
    return 0;
  }

  snprintf(path, sizeof(path), "%s/%s/%s", CAVP_VECTOR_DIR, subdir, filename);
  f = fopen(path, "r");
  if (f == NULL)
  {
    fprintf(stderr, "CAVP file not found: %s\n", path);
    return 0;
  }

  cavp_record_reset(&rec);
  rec.count = -1;
  chain.active = 0;

  while (fgets(line, sizeof(line), f) != NULL)
  {
    const char* v;
    int n;

    if (line[0] == '#' || line[0] == '\r' || line[0] == '\n')
      continue;
    if (line[0] == '[')
    {
      if (strncmp(line, "[ENCRYPT]", 9) == 0)
      {
        encrypt = 1;
        chain.active = 0;
      }
      else if (strncmp(line, "[DECRYPT]", 9) == 0)
      {
        encrypt = 0;
        chain.active = 0;
      }
      continue;
    }

    if ((v = cavp_value(line, "COUNT")) != NULL)
    {
      rec.count = 0;
      while (*v >= '0' && *v <= '9')
        rec.count = rec.count * 10 + (*v++ - '0');
      continue;
    }
    if ((v = cavp_value(line, "KEYs")) != NULL)
    {
      if (cavp_parse_hex(v, rec.key, 8) != 8)
      {
        ok = 0;
        break;
      }
      memcpy(rec.key + 8, rec.key, 8);
      memcpy(rec.key + 16, rec.key, 8);
      rec.have_key = 1;
      continue;
    }
    if ((v = cavp_value(line, "KEY1")) != NULL)
    {
      ok &= cavp_parse_hex(v, rec.key, 8) == 8;
      rec.have_key = 1;
      continue;
    }
    if ((v = cavp_value(line, "KEY2")) != NULL)
    {
      ok &= cavp_parse_hex(v, rec.key + 8, 8) == 8;
      continue;
    }
    if ((v = cavp_value(line, "KEY3")) != NULL)
    {
      ok &= cavp_parse_hex(v, rec.key + 16, 8) == 8;
      continue;
    }
    if ((v = cavp_value(line, "IV")) != NULL)
    {
      ok &= cavp_parse_hex(v, rec.iv, 8) == 8;
      rec.have_iv = 1;
      continue;
    }
    if ((v = cavp_value(line, "PLAINTEXT")) != NULL)
    {
      n = bitmode ? cavp_parse_bits(v, rec.pt, sizeof(rec.pt))
                  : cavp_parse_hex(v, rec.pt, sizeof(rec.pt));
      if (n < 0)
      {
        ok = 0;
        break;
      }
      rec.pt_len = (size_t)n;
      rec.have_pt = 1;
    }
    else if ((v = cavp_value(line, "CIPHERTEXT")) != NULL)
    {
      n = bitmode ? cavp_parse_bits(v, rec.ct, sizeof(rec.ct))
                  : cavp_parse_hex(v, rec.ct, sizeof(rec.ct));
      if (n < 0)
      {
        ok = 0;
        break;
      }
      rec.ct_len = (size_t)n;
      rec.have_ct = 1;
    }
    else
    {
      continue;
    }

    /* A case is complete once both payload fields have been read */
    if (rec.have_pt && rec.have_ct && rec.have_key)
    {
      if (suite == CAVP_MCT)
        ok &= cavp_mct_case(mode, filename, encrypt, &rec, &chain);
      else
        ok &= cavp_standard_case(mode, filename, encrypt, &rec);
      cases++;
      rec.have_pt = rec.have_ct = 0;
      rec.pt_len = rec.ct_len = 0;
    }
  }

  fclose(f);
  if (cases == 0)
  {
    fprintf(stderr, "CAVP no cases parsed: %s\n", path);
    return 0;
  }
  return ok;
}

/* ------------------------------------------------------------------------- */
/* Corpus tables                                                             */
/* ------------------------------------------------------------------------- */

static const char* const cavp_kat_files[] = {
  "TECBinvperm.rsp",   "TECBpermop.rsp",   "TECBsubtab.rsp",
  "TECBvarkey.rsp",    "TECBvartext.rsp",
  "TCBCinvperm.rsp",   "TCBCpermop.rsp",   "TCBCsubtab.rsp",
  "TCBCvarkey.rsp",    "TCBCvartext.rsp",
  "TCFB1invperm.rsp",  "TCFB1permop.rsp",  "TCFB1subtab.rsp",
  "TCFB1varkey.rsp",   "TCFB1vartext.rsp",
  "TCFB8invperm.rsp",  "TCFB8permop.rsp",  "TCFB8subtab.rsp",
  "TCFB8varkey.rsp",   "TCFB8vartext.rsp",
  "TCFB64invperm.rsp", "TCFB64permop.rsp", "TCFB64subtab.rsp",
  "TCFB64varkey.rsp",  "TCFB64vartext.rsp",
  "TOFBinvperm.rsp",   "TOFBpermop.rsp",   "TOFBsubtab.rsp",
  "TOFBvarkey.rsp",    "TOFBvartext.rsp"
};

static const char* const cavp_mmt_files[] = {
  "TECBMMT2.rsp",   "TECBMMT3.rsp",   "TCBCMMT2.rsp",   "TCBCMMT3.rsp",
  "TCFB1MMT2.rsp",  "TCFB1MMT3.rsp",  "TCFB8MMT2.rsp",  "TCFB8MMT3.rsp",
  "TCFB64MMT2.rsp", "TCFB64MMT3.rsp", "TOFBMMT2.rsp",   "TOFBMMT3.rsp"
};

static const char* const cavp_mct_files[] = {
  "TECBMonte2.rsp",   "TECBMonte3.rsp",   "TCBCMonte2.rsp",   "TCBCMonte3.rsp",
  "TCFB1Monte2.rsp",  "TCFB1Monte3.rsp",  "TCFB8Monte2.rsp",  "TCFB8Monte3.rsp",
  "TCFB64Monte2.rsp", "TCFB64Monte3.rsp", "TOFBMonte2.rsp",   "TOFBMonte3.rsp"
};

MunitResult test_cavp(const MunitParameter params[], void* data)
{
  size_t i;
  int failures = 0;

  (void) params;
  (void) data;

  for (i = 0; i < sizeof(cavp_kat_files) / sizeof(cavp_kat_files[0]); i++)
    failures += !cavp_run_file("kat", cavp_kat_files[i]);
  for (i = 0; i < sizeof(cavp_mmt_files) / sizeof(cavp_mmt_files[0]); i++)
    failures += !cavp_run_file("mmt", cavp_mmt_files[i]);
  for (i = 0; i < sizeof(cavp_mct_files) / sizeof(cavp_mct_files[0]); i++)
    failures += !cavp_run_file("mct", cavp_mct_files[i]);

  munit_assert_int(failures, ==, 0);
  return MUNIT_OK;
}

#endif /* DES_CAVP */

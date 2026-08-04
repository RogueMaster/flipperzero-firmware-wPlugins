#include "fmtx_wav.h"

#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>

struct Wav
{
    Storage *sto;
    File *f;
    uint32_t left;
    uint16_t pos;
    uint16_t n;
    uint8_t buf[256];
};

static uint16_t le16(const uint8_t *x)
{
    return x[0] | ((uint16_t)x[1] << 8);
}

static uint32_t le32(const uint8_t *x)
{
    return x[0] | ((uint32_t)x[1] << 8) | ((uint32_t)x[2] << 16) | ((uint32_t)x[3] << 24);
}

static bool readn(File *f, void *data, size_t size)
{
    return storage_file_read(f, data, size) == size;
}

static void wavclose(Wav *wav)
{
    if(wav->f)
    {
        storage_file_close(wav->f);
        storage_file_free(wav->f);
        wav->f = NULL;
    }
    if(wav->sto)
    {
        furi_record_close(RECORD_STORAGE);
        wav->sto = NULL;
    }
}

Wav *wavnew(void)
{
    return calloc(1, sizeof(Wav));
}

void wavfree(Wav *wav)
{
    if(!wav) return;
    wavclose(wav);
    free(wav);
}

bool wavopen(Wav *wav, const char *path)
{
    uint8_t head[16];
    uint64_t file_size;
    uint64_t riff_end;
    uint32_t data_offset = 0;
    uint32_t data_size = 0;
    bool fmt_seen = false;

    if(!wav || !path) return false;
    wavclose(wav);
    memset(wav, 0, sizeof(*wav));
    wav->sto = furi_record_open(RECORD_STORAGE);
    wav->f = storage_file_alloc(wav->sto);
    if(!wav->f || !storage_file_open(wav->f, path, FSAM_READ, FSOM_OPEN_EXISTING)) goto bad;
    file_size = storage_file_size(wav->f);
    if(file_size > UINT32_MAX || file_size < 12 || !readn(wav->f, head, 12)) goto bad;
    if(memcmp(head, "RIFF", 4) || memcmp(head + 8, "WAVE", 4)) goto bad;
    riff_end = 8ULL + le32(head + 4);
    if(riff_end < 12 || riff_end > file_size) goto bad;

    while(storage_file_tell(wav->f) < riff_end)
    {
        uint64_t pos = storage_file_tell(wav->f);
        uint32_t size;
        uint64_t chunk_end;
        uint64_t padded_end;
        if(pos + 8 > riff_end || !readn(wav->f, head, 8)) goto bad;
        size = le32(head + 4);
        pos += 8;
        chunk_end = pos + size;
        padded_end = chunk_end + (size & 1U);
        if(chunk_end < pos || padded_end < chunk_end || padded_end > riff_end || padded_end > file_size) goto bad;

        if(!memcmp(head, "fmt ", 4))
        {
            if(size < 16 || !readn(wav->f, head, 16)) goto bad;
            if(le16(head) != 1 || le16(head + 2) != 1 || le32(head + 4) != 8000 || le32(head + 8) != 8000 || le16(head + 12) != 1 || le16(head + 14) != 8) goto bad;
            fmt_seen = true;
        }
        else if(!memcmp(head, "data", 4) && data_offset == 0)
        {
            data_offset = (uint32_t)pos;
            data_size = size;
        }
        if(!storage_file_seek(wav->f, (uint32_t)padded_end, true)) goto bad;
    }

    if(!fmt_seen || data_offset == 0 || data_size == 0 || !storage_file_seek(wav->f, data_offset, true)) goto bad;
    wav->left = data_size;
    return true;

bad:
    wavclose(wav);
    return false;
}

bool wavnext(Wav *wav, uint8_t *s)
{
    if(!wav || !wav->f || !s) return false;
    if(wav->pos == wav->n)
    {
        size_t want;
        size_t got;
        if(wav->left == 0) return false;
        want = wav->left < sizeof(wav->buf) ? wav->left : sizeof(wav->buf);
        got = storage_file_read(wav->f, wav->buf, want);
        if(got == 0 || got > wav->left) return false;
        wav->left -= got;
        wav->pos = 0;
        wav->n = got;
    }
    *s = wav->buf[wav->pos++];
    return true;
}

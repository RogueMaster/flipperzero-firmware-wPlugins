#include "flipperham_i.h"
#include "aprs.h"

#include <storage/storage.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t hline(FlipperHamApp *app, char *a, uint8_t j, bool *ok)
{
    char *p;
    char *q;
    char *e;
    char b[CALL_LEN];
    uint8_t s;
    bool d;
    long n;

    p = a;
    while (*p == ' ' || *p == '\t')
        p++;
    if (!*p)
        return j;

    q = p;
    while (*q && *q != ',')
        q++;
    if (*q != ',')
        return j;
    *q++ = 0;

    e = p + strlen(p);
    while (e > p)
        if (e[-1] == ' ' || e[-1] == '\t')
            *--e = 0;
        else
            break;
    while (*q == ' ' || *q == '\t')
        q++;
    e = q + strlen(q);
    while (e > q)
        if (e[-1] == ' ' || e[-1] == '\t')
            *--e = 0;
        else
            break;

    if (!*p || !*q)
        return j;
    if (!call_split(p, b, &s, &d))
        return j;

    n = strtol(q, &e, 10);
    if (e == q)
        return j;
    if (*e)
        return j;
    if (n < 0 || n > 65535)
        return j;
    if (!call_crc(b, (uint16_t)n))
        return j;

    *ok = true;
    if (j >= HAM_N)
        return j;

    snprintf(app->ham_calls[j], sizeof(app->ham_calls[j]), "%s", b);
    app->ham_ssid[j] = d ? s : 0;
    app->ham_has_ssid[j] = d;
    app->ham_pass[j] = n;
    return j + 1;
}

void ham_load_txt(FlipperHamApp *app)
{
    Storage *storage;
    File *file;
    char a[64];
    char c;
    uint8_t i;
    uint8_t j;
    bool ok;

    memset(app->ham_calls, 0, sizeof(app->ham_calls));
    memset(app->ham_ssid, 0, sizeof(app->ham_ssid));
    memset(app->ham_has_ssid, 0, sizeof(app->ham_has_ssid));
    memset(app->ham_pass, 0, sizeof(app->ham_pass));
    app->ham_n = 0;
    app->ham_ok = false;

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);

    if (!storage_file_open(file, MY_CALLS_FILE, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    i = 0;
    j = 0;
    ok = false;

    while (storage_file_read(file, &c, 1) == 1)
    {
        if (c == '\r')
            continue;

        if (c == '\n')
        {
            a[i] = 0;
            j = hline(app, a, j, &ok);
            i = 0;
            continue;
        }

        if (i < sizeof(a) - 1)
            a[i++] = c;
    }

    if (i)
    {
        a[i] = 0;
        j = hline(app, a, j, &ok);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    app->ham_n = j;
    app->ham_ok = ok && j;
    if (app->ham_index >= app->ham_n)
        app->ham_index = 0;
    if (app->ham_tx_index >= app->ham_n)
        app->ham_tx_index = 0;
}

void ham_save_txt(FlipperHamApp *app)
{
    Storage *storage;
    File *file;
    uint8_t i;
    char a[48];
    int n;

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);

    if (!storage_file_open(file, MY_CALLS_FILE, FSAM_WRITE, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    storage_file_seek(file, 0, true);
    storage_file_truncate(file);

    for (i = 0; i < app->ham_n; i++)
    {
        if (!app->ham_calls[i][0])
            continue;

        if (app->ham_has_ssid[i])
            n = snprintf(a, sizeof(a), "%s-%u,%u\n", app->ham_calls[i], app->ham_ssid[i],
                         app->ham_pass[i]);
        else
            n = snprintf(a, sizeof(a), "%s,%u\n", app->ham_calls[i], app->ham_pass[i]);

        if (n > 0)
            storage_file_write(file, a, n);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

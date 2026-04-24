#include "flipperham_i.h"

#include <storage/storage.h>

#include <stdio.h>
#include <string.h>

void callbook_save_txt(FlipperHamApp *app)
{
    Storage *storage;
    File *file;
    uint8_t i;
    uint16_t n;

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);

    storage_common_mkdir(storage, CALLBOOK_DIR);

    if (storage_file_open(file, CALLBOOK_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        for (i = 0; i < CALL_N; i++)
        {
            if (!app->calls_used[i])
                continue;
            if (!app->calls[i][0])
                continue;

            n = strlen(app->calls[i]);
            if (n)
                storage_file_write(file, app->calls[i], n);
            storage_file_write(file, "\n", 1);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void callbook_load_txt(FlipperHamApp *app)
{
    Storage *storage;
    File *file;
    char a[CALL_LEN + 4];
    char c;
    uint8_t i;
    uint8_t j;

    memset(app->calls, 0, sizeof(app->calls));
    memset(app->calls_used, 0, sizeof(app->calls_used));
    app->calls_n = 0;

    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);

    storage_common_mkdir(storage, CALLBOOK_DIR);

    if (!storage_file_open(file, CALLBOOK_FILE, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        /* Fall back to a tiny default callbook if the file is missing. */
        snprintf(app->calls[0], sizeof(app->calls[0]), "FL1PER");
        snprintf(app->calls[1], sizeof(app->calls[1]), "YO3GND-12");
        app->calls_used[0] = 1;
        app->calls_used[1] = 1;
        app->calls_n = 2;
        callbook_save_txt(app);
        return;
    }

    i = 0;
    j = 0;

    while (storage_file_read(file, &c, 1) == 1)
    {
        if (c == '\r')
            continue;

        if (c == '\n')
        {
            a[i] = 0;

            if (a[0] && j < CALL_N && call_validate(a))
            {
                i = 0;
                while (a[i])
                {
                    app->calls[j][i] = a[i];
                    i++;
                }
                app->calls[j][i] = 0;
                app->calls_used[j] = 1;
                j++;
            }

            i = 0;
            continue;
        }

        if (i < sizeof(a) - 1)
            a[i++] = c;
    }

    if (i)
    {
        a[i] = 0;

        if (a[0] && j < CALL_N && call_validate(a))
        {
            i = 0;
            while (a[i])
            {
                app->calls[j][i] = a[i];
                i++;
            }
            app->calls[j][i] = 0;
            app->calls_used[j] = 1;
            j++;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    calls_fix(app);
}

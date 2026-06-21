/*
 * KeeLoq Keystore Decryptor — a KeeLoq keystore decryptor for Flipper Zero.
 * https://github.com/Lechnio/KeeLoq-Keystore-Decryptor
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/buffered_file_stream.h>
#include <flipper_format/flipper_format.h>

#define KEYSTORE_PATH "/ext/subghz/assets/keeloq_mfcodes"
#define OUTPUT_PATH "/ext/keystore_decrypted.txt"
#define KEY_ENCLAVE_ID 1

typedef struct
{
    Gui *gui;
    ViewPort *view_port;
    FuriMessageQueue *queue;
    Storage *storage;
    Stream *output_stream;
    uint32_t keys_decrypted;
    bool finished;
    char status[128];
} KeystoreDecryptApp;

static void keystore_input_callback(InputEvent *event, void *context)
{
    KeystoreDecryptApp *app = context;
    if (event->type == InputTypeShort && event->key == InputKeyBack)
    {
        furi_message_queue_put(app->queue, event, 0);
    }
}

static void keystore_draw_callback(Canvas *canvas, void *context)
{
    KeystoreDecryptApp *app = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "KeeLoq Keystore Decryptor");

    canvas_set_font(canvas, FontSecondary);

    if (!app->finished)
    {
        canvas_draw_str(canvas, 0, 30, "Decrypting...");
        canvas_draw_str(canvas, 0, 40, app->status);
    }
    else
    {
        canvas_draw_str(canvas, 0, 30, "Done!");
        char buf[64];
        snprintf(buf, sizeof(buf), "Keys: %lu", app->keys_decrypted);
        canvas_draw_str(canvas, 0, 40, buf);
        canvas_draw_str(canvas, 0, 50, "Saved to:");
        canvas_draw_str(canvas, 0, 60, OUTPUT_PATH);
    }
}

static KeystoreDecryptApp *app_alloc(void)
{
    KeystoreDecryptApp *app = malloc(sizeof(KeystoreDecryptApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    app->queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    view_port_draw_callback_set(app->view_port, keystore_draw_callback, app);
    view_port_input_callback_set(app->view_port, keystore_input_callback, app);

    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->storage = furi_record_open(RECORD_STORAGE);
    app->keys_decrypted = 0;
    app->finished = false;
    app->output_stream = NULL;
    strncpy(app->status, "Starting...", sizeof(app->status));

    return app;
}

static void app_free(KeystoreDecryptApp *app)
{
    if (app->output_stream)
    {
        buffered_file_stream_close(app->output_stream);
        stream_free(app->output_stream);
    }

    furi_record_close(RECORD_STORAGE);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->queue);
    free(app);
}

static uint8_t hex_char_to_val(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    return 0;
}

static bool decrypt_keystore(KeystoreDecryptApp *app)
{
    bool success = false;

    app->output_stream = buffered_file_stream_alloc(app->storage);
    if (!buffered_file_stream_open(
            app->output_stream, OUTPUT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        strncpy(app->status, "Output error", sizeof(app->status));
        return false;
    }

    const char *header = "# Decrypted Keystore\n# Format: KEY:TYPE:NAME\n\n";
    stream_write(app->output_stream, (uint8_t *)header, strlen(header));

    Stream *keystore_stream = buffered_file_stream_alloc(app->storage);
    if (!buffered_file_stream_open(keystore_stream, KEYSTORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        strncpy(app->status, "Keystore not found", sizeof(app->status));
        stream_free(keystore_stream);
        return false;
    }

    char line[256];
    size_t line_pos = 0;
    bool header_parsed = false;
    uint8_t iv[16] = {0};
    bool encrypted = false;

    while (!header_parsed)
    {
        uint8_t c;
        if (stream_read(keystore_stream, &c, 1) != 1)
            break;

        if (c == '\n')
        {
            line[line_pos] = '\0';

            if (line_pos == 0 ||
                (line[0] != 'F' && line[0] != 'V' && line[0] != 'E' && line[0] != 'I'))
            {
                header_parsed = true;
                if (line_pos > 0)
                {
                    stream_seek(keystore_stream, -((int)line_pos + 1), StreamOffsetFromCurrent);
                }

                break;
            }

            if (strncmp(line, "Filetype:", 9) == 0)
            {
                strncpy(app->status, line, sizeof(app->status));
            }
            else if (strncmp(line, "Encryption:", 11) == 0)
            {
                if (strstr(line, "1"))
                    encrypted = true;
            }
            else if (strncmp(line, "IV:", 3) == 0)
            {
                char *iv_str = line + 3;
                while (*iv_str == ' ')
                    iv_str++;

                for (int i = 0; i < 16 && iv_str[i * 3] && iv_str[i * 3 + 1]; i++)
                {
                    iv[i] = (hex_char_to_val(iv_str[i * 3]) << 4) |
                            hex_char_to_val(iv_str[i * 3 + 1]);
                }
            }

            line_pos = 0;
        }
        else if (c != '\r' && line_pos < sizeof(line) - 1)
        {
            line[line_pos++] = c;
        }
    }

    if (encrypted)
    {
        uint32_t *iv32 = (uint32_t *)iv;
        for (int i = 0; i < 4; i++)
        {
            uint32_t val = iv32[i];
            uint32_t rotated = (val << 8) | (val >> 24);

            uint32_t result = 0;
            for (int j = 0; j < 4; j++)
            {
                uint8_t a = (rotated >> (j * 8)) & 0xFF;
                uint8_t b = (val >> (j * 8)) & 0xFF;
                uint16_t sum = a + b;

                if (sum > 255)
                    sum = 255;

                result |= (sum << (j * 8));
            }

            iv32[i] = result;
        }

        if (!furi_hal_crypto_enclave_load_key(KEY_ENCLAVE_ID, iv))
        {
            strncpy(app->status, "Crypto init failed", sizeof(app->status));
            buffered_file_stream_close(keystore_stream);
            stream_free(keystore_stream);
            return false;
        }
    }

    char enc_line[1024];
    size_t enc_pos = 0;
    uint8_t buffer[64];

    while (true)
    {
        size_t ret = stream_read(keystore_stream, buffer, sizeof(buffer));
        if (ret == 0)
            break;

        for (size_t i = 0; i < ret; i++)
        {
            if (buffer[i] == '\n')
            {
                if (enc_pos > 0)
                {
                    enc_line[enc_pos] = '\0';

                    size_t bin_len = enc_pos / 2;
                    uint8_t *bin_data = malloc(bin_len + 16);
                    memset(bin_data, 0, bin_len + 16);

                    for (size_t j = 0; j < enc_pos && j / 2 < bin_len; j += 2)
                    {
                        bin_data[j / 2] = (hex_char_to_val(enc_line[j]) << 4) |
                                          hex_char_to_val(enc_line[j + 1]);
                    }

                    if (encrypted)
                    {
                        uint8_t decrypted[512] = {0};

                        size_t decrypt_len = ((bin_len + 15) / 16) * 16;

                        if (furi_hal_crypto_decrypt(bin_data, decrypted, decrypt_len))
                        {
                            char skey[17] = {0};
                            char name[65] = {0};
                            uint16_t type = 0;

                            char *dec_str = (char *)decrypted;
                            for (int j = decrypt_len - 1; j >= 0; j--)
                            {
                                if (decrypted[j] != 0)
                                {
                                    decrypted[j + 1] = 0;
                                    break;
                                }
                            }

                            if (sscanf(dec_str, "%16[^:]:%hu:%64s", skey, &type, name) >= 2)
                            {
                                FuriString *out_line =
                                    furi_string_alloc_printf("%s:%u:%s\n", skey, type, name);
                                stream_write(
                                    app->output_stream,
                                    (uint8_t *)furi_string_get_cstr(out_line),
                                    furi_string_size(out_line));
                                furi_string_free(out_line);
                                app->keys_decrypted++;

                                if (app->keys_decrypted % 10 == 0)
                                {
                                    snprintf(
                                        app->status,
                                        sizeof(app->status),
                                        "Decrypted: %lu",
                                        app->keys_decrypted);
                                    view_port_update(app->view_port);
                                }
                            }
                        }
                    }
                    else
                    {
                        stream_write(app->output_stream, (uint8_t *)enc_line, enc_pos);
                        stream_write(app->output_stream, (uint8_t *)"\n", 1);
                    }

                    free(bin_data);
                    enc_pos = 0;
                }
            }
            else if (buffer[i] != '\r' && enc_pos < sizeof(enc_line) - 1)
            {
                enc_line[enc_pos++] = buffer[i];
            }
        }
    }

    if (encrypted)
    {
        furi_hal_crypto_enclave_unload_key(1);
    }

    buffered_file_stream_close(keystore_stream);
    stream_free(keystore_stream);

    success = true;
    app->finished = true;
    snprintf(app->status, sizeof(app->status), "Done! Keys: %lu", app->keys_decrypted);

    return success;
}

int32_t keystore_decrypt_app(void *p)
{
    UNUSED(p);

    KeystoreDecryptApp *app = app_alloc();

    decrypt_keystore(app);

    InputEvent event;
    while (true)
    {
        view_port_update(app->view_port);

        if (furi_message_queue_get(app->queue, &event, 100) == FuriStatusOk)
        {
            if (event.type == InputTypeShort && event.key == InputKeyBack)
            {
                break;
            }
        }
    }

    app_free(app);
    return 0;
}

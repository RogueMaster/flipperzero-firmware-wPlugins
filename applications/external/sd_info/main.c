#include <furi.h>
#include <gui/gui.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <furi_hal.h>

#define MAX_LINES       8
#define LINE_HEIGHT     13
#define FIRST_LINE_Y    12
#define INFO_LINE_LEN   32
#define SIZE_STR_LEN    8
#define TEST_BLOCK_SIZE (32 * 1024)
#define TEST_BLOCKS     16
#define TEST_ITERATIONS 3
#define TEST_FILE_PATH  "/ext/sdtest.tmp"
#define TOTAL_PAGES     3

typedef struct {
    float read_speed;
    float write_speed;
    float access_time;
    uint32_t errors;
    bool has_bad_blocks;
} TestResults;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    Storage* storage;
    char info_lines[MAX_LINES][INFO_LINE_LEN];
    uint8_t current_page;
    bool has_card;
    bool is_testing;
    TestResults test_results;
    float test_progress;
} SDCardInfo;

static const char* get_manufacturer_name(uint8_t id) {
    switch(id) {
    case 0x01:
        return "Panasonic";
    case 0x02:
        return "Kingston";
    case 0x03:
        return "SanDisk";
    case 0x06:
        return "Ritek";
    case 0x09:
        return "ATP";
    case 0x13:
        return "KingMax";
    case 0x1A:
        return "PQI";
    case 0x1B:
        return "Samsung";
    case 0x1D:
        return "AData";
    case 0x27:
        return "Phison";
    case 0x28:
        return "Lexar";
    case 0x31:
        return "Silicon Power";
    case 0x41:
        return "Kingston";
    case 0x6F:
        return "STMicro";
    case 0x73:
        return "Toshiba";
    case 0x74:
        return "Transcend";
    case 0x76:
        return "Samsung";
    case 0x82:
        return "Sony";
    case 0x83:
        return "Motorola";
    default: {
        static char unknown[16];
        snprintf(unknown, sizeof(unknown), "ID: 0x%02X", id);
        return unknown;
    }
    }
}

static const char* get_month_name(uint8_t month) {
    switch(month) {
    case 1:
        return "Jan";
    case 2:
        return "Feb";
    case 3:
        return "Mar";
    case 4:
        return "Apr";
    case 5:
        return "May";
    case 6:
        return "Jun";
    case 7:
        return "Jul";
    case 8:
        return "Aug";
    case 9:
        return "Sep";
    case 10:
        return "Oct";
    case 11:
        return "Nov";
    case 12:
        return "Dec";
    default:
        return "???";
    }
}

static void format_size(char* buffer, size_t buffer_size, uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;

    while(size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }

    snprintf(buffer, buffer_size, "%.1f%s", size, units[unit]);
}

static void get_test_result_string(
    const TestResults* results,
    char* status_buf,
    size_t status_size,
    char* speed_buf,
    size_t speed_size) {
    if(results->read_speed == 0 || results->write_speed == 0) {
        snprintf(status_buf, status_size, "Not tested");
        speed_buf[0] = '\0';
        return;
    }

    const char* health;
    if(results->has_bad_blocks) {
        health = "Bad blocks!";
    } else if(results->errors > 0) {
        health = "Errors";
    } else if(results->access_time > 100.0f) {
        health = "Slow";
    } else if(results->read_speed < 2.0f || results->write_speed < 1.0f) {
        health = "Very slow";
    } else if(results->read_speed > 8.0f && results->write_speed > 5.0f) {
        health = "Good";
    } else {
        health = "Fair";
    }

    snprintf(status_buf, status_size, "%s", health);
    snprintf(
        speed_buf,
        speed_size,
        "%.1f/%.1f MB/s",
        (double)results->read_speed,
        (double)results->write_speed);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    SDCardInfo* app = ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(app->has_card) {
        // Заголовок с номером страницы
        char title[32];
        snprintf(
            title, sizeof(title), "SD Card Info: Page %d/%d", app->current_page + 1, TOTAL_PAGES);
        canvas_draw_str(canvas, 2, FIRST_LINE_Y, title);

        canvas_set_font(canvas, FontSecondary);

        if(app->is_testing) {
            // Заголовок теста
            canvas_draw_str(canvas, 2, FIRST_LINE_Y + LINE_HEIGHT, "Testing SD card...");

            // Пояснение что тестируется
            char test_info[64];
            int block_num =
                ((int)(app->test_progress * TEST_BLOCKS * TEST_ITERATIONS) % TEST_BLOCKS) + 1;
            int iter_num =
                ((int)(app->test_progress * TEST_BLOCKS * TEST_ITERATIONS) / TEST_BLOCKS) + 1;
            snprintf(
                test_info,
                sizeof(test_info),
                "Block: %d/%d  Pass: %d/%d",
                block_num,
                TEST_BLOCKS,
                iter_num,
                TEST_ITERATIONS);
            canvas_draw_str(canvas, 2, FIRST_LINE_Y + LINE_HEIGHT * 2, test_info);

            // Прогресс-бар
            int bar_y = FIRST_LINE_Y + LINE_HEIGHT * 3;
            int progress_width = (int)(120 * app->test_progress);

            // Рамка и заполнение прогресс-бара
            canvas_draw_frame(canvas, 2, bar_y, 124, 12);
            if(progress_width > 0) {
                canvas_draw_box(canvas, 4, bar_y + 2, progress_width, 8);
            }

            // Инвертированный текст поверх прогресс-бара
            const char* progress_text = "one moment";
            int text_width = canvas_string_width(canvas, progress_text);
            int text_x = 63 - (text_width / 2);
            canvas_invert_color(canvas);
            canvas_draw_str(canvas, text_x, bar_y + 9, progress_text);
            canvas_invert_color(canvas);

            return;
        }

        // Страница 1: Основная информация
        if(app->current_page == 0) {
            for(int i = 0; i < 4; i++) {
                if(app->info_lines[i][0] != '\0') {
                    canvas_draw_str(
                        canvas, 2, FIRST_LINE_Y + LINE_HEIGHT * (i + 1), app->info_lines[i]);
                }
            }
        }
        // Страница 2: Информация о памяти
        else if(app->current_page == 1) {
            for(int i = 4; i < 6; i++) {
                if(app->info_lines[i][0] != '\0') {
                    canvas_draw_str(
                        canvas, 2, FIRST_LINE_Y + LINE_HEIGHT * (i - 3), app->info_lines[i]);
                }
            }
        }
        // Страница 3: Результаты теста
        else if(app->current_page == 2) {
            char status[16];
            char speed[16];
            get_test_result_string(
                &app->test_results, status, sizeof(status), speed, sizeof(speed));

            canvas_draw_str(canvas, 2, FIRST_LINE_Y + LINE_HEIGHT, "Test Results:");

            // Статус теста
            snprintf(app->info_lines[6], INFO_LINE_LEN, "Status: %s", status);
            canvas_draw_str(canvas, 2, FIRST_LINE_Y + LINE_HEIGHT * 2, app->info_lines[6]);

            if(speed[0] != '\0') {
                // Скорость чтения/записи
                snprintf(app->info_lines[7], INFO_LINE_LEN, "R/W: %s", speed);
                canvas_draw_str(canvas, 2, FIRST_LINE_Y + LINE_HEIGHT * 3, app->info_lines[7]);

                if(app->test_results.errors > 0) {
                    char errors[32];
                    snprintf(
                        errors, sizeof(errors), "Errors found: %ld", app->test_results.errors);
                    canvas_draw_str(canvas, 2, FIRST_LINE_Y + LINE_HEIGHT * 4, errors);
                }
            } else {
                canvas_draw_str(
                    canvas, 2, FIRST_LINE_Y + LINE_HEIGHT * 3, "Test time: ~15 seconds");
            }
        }

        // Подсказка для кнопки OK (только на странице теста и когда тест не запущен)
        if(app->current_page == 2 && !app->is_testing) {
            canvas_draw_str(canvas, 2, 63, "Press OK to start test");
        }
    } else {
        canvas_draw_str(canvas, 2, 32, "No SD Card detected");
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    SDCardInfo* app = ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

static void perform_sd_test(SDCardInfo* app) {
    Storage* storage = app->storage;
    File* file = storage_file_alloc(storage);
    uint8_t* write_buffer = malloc(TEST_BLOCK_SIZE);
    uint8_t* read_buffer = malloc(TEST_BLOCK_SIZE);
    uint32_t start_time;
    float total_read_speed = 0;
    float total_write_speed = 0;
    float max_access_time = 0;
    uint32_t total_errors = 0;
    bool bad_blocks_found = false;

    app->is_testing = true;
    app->test_progress = 0;
    memset(&app->test_results, 0, sizeof(TestResults));

    for(size_t i = 0; i < TEST_BLOCK_SIZE; i++) {
        write_buffer[i] = (uint8_t)(i & 0xFF);
    }

    for(int iter = 0; iter < TEST_ITERATIONS; iter++) {
        for(int block = 0; block < TEST_BLOCKS; block++) {
            app->test_progress =
                (float)(iter * TEST_BLOCKS + block) / (float)(TEST_ITERATIONS * TEST_BLOCKS);
            view_port_update(app->view_port);

            char block_file[128];
            snprintf(block_file, sizeof(block_file), "%s_%d", TEST_FILE_PATH, block);

            if(storage_file_open(file, block_file, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                start_time = furi_get_tick();
                if(storage_file_write(file, write_buffer, TEST_BLOCK_SIZE) == TEST_BLOCK_SIZE) {
                    uint32_t write_time = furi_get_tick() - start_time;
                    float write_speed = (float)TEST_BLOCK_SIZE / (1024.0f * 1024.0f) /
                                        ((float)write_time / 1000.0f);
                    total_write_speed += write_speed;

                    if(write_time > max_access_time) {
                        max_access_time = write_time;
                    }
                } else {
                    total_errors++;
                }
                storage_file_close(file);
            } else {
                total_errors++;
            }

            if(storage_file_open(file, block_file, FSAM_READ, FSOM_OPEN_EXISTING)) {
                start_time = furi_get_tick();
                if(storage_file_read(file, read_buffer, TEST_BLOCK_SIZE) == TEST_BLOCK_SIZE) {
                    uint32_t read_time = furi_get_tick() - start_time;
                    float read_speed = (float)TEST_BLOCK_SIZE / (1024.0f * 1024.0f) /
                                       ((float)read_time / 1000.0f);
                    total_read_speed += read_speed;

                    if(read_time > max_access_time) {
                        max_access_time = read_time;
                    }

                    for(size_t i = 0; i < TEST_BLOCK_SIZE; i++) {
                        if(read_buffer[i] != (uint8_t)(i & 0xFF)) {
                            bad_blocks_found = true;
                            break;
                        }
                    }
                } else {
                    total_errors++;
                }
                storage_file_close(file);
            } else {
                total_errors++;
            }

            storage_simply_remove(storage, block_file);
        }
    }

    float total_operations = TEST_BLOCKS * TEST_ITERATIONS;
    app->test_results.read_speed = total_read_speed / total_operations;
    app->test_results.write_speed = total_write_speed / total_operations;
    app->test_results.access_time = max_access_time;
    app->test_results.errors = total_errors;
    app->test_results.has_bad_blocks = bad_blocks_found;

    free(write_buffer);
    free(read_buffer);
    storage_file_free(file);
    app->is_testing = false;
    app->test_progress = 1.0f;
}

static void get_sd_info(SDCardInfo* app) {
    if(storage_sd_status(app->storage) == FSE_OK) {
        app->has_card = true;
        SDInfo sd_info;
        uint64_t total_space = 0;
        uint64_t free_space = 0;

        if(storage_sd_info(app->storage, &sd_info) == FSE_OK) {
            snprintf(
                app->info_lines[0],
                INFO_LINE_LEN,
                "Maker: %s",
                get_manufacturer_name(sd_info.manufacturer_id));

            snprintf(app->info_lines[1], INFO_LINE_LEN, "Name: %s", sd_info.product_name);

            snprintf(app->info_lines[2], INFO_LINE_LEN, "OEM ID: %s", sd_info.oem_id);

            snprintf(
                app->info_lines[3],
                INFO_LINE_LEN,
                "Made: %s %d",
                get_month_name(sd_info.manufacturing_month),
                sd_info.manufacturing_year);

            snprintf(
                app->info_lines[4],
                INFO_LINE_LEN,
                "Rev: %d.%d",
                sd_info.product_revision_major,
                sd_info.product_revision_minor);

            char status[16];
            char speed[16];
            get_test_result_string(
                &app->test_results, status, sizeof(status), speed, sizeof(speed));
            snprintf(app->info_lines[6], INFO_LINE_LEN, "Test: %s", status);
            if(speed[0] != '\0') {
                snprintf(app->info_lines[7], INFO_LINE_LEN, "Speed: %s", speed);
            } else if(app->test_results.errors > 0) {
                snprintf(
                    app->info_lines[7], INFO_LINE_LEN, "Errors: %ld", app->test_results.errors);
            } else {
                snprintf(app->info_lines[7], INFO_LINE_LEN, "Press OK to test");
            }
        }

        if(storage_common_fs_info(app->storage, "/ext", &total_space, &free_space) == FSE_OK) {
            char total_str[SIZE_STR_LEN];
            char free_str[SIZE_STR_LEN];
            format_size(total_str, SIZE_STR_LEN, total_space);
            format_size(free_str, SIZE_STR_LEN, free_space);
            snprintf(app->info_lines[5], INFO_LINE_LEN, "Mem:%s/%s", free_str, total_str);
        }
    } else {
        app->has_card = false;
        for(int i = 0; i < MAX_LINES; i++) {
            app->info_lines[i][0] = '\0';
        }
    }
}

int32_t sd_card_info_app(void* p) {
    UNUSED(p);
    SDCardInfo* app = malloc(sizeof(SDCardInfo));

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->current_page = 0;
    app->is_testing = false;
    app->test_progress = 0;

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    get_sd_info(app);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort) {
                switch(event.key) {
                case InputKeyRight:
                    if(app->current_page < TOTAL_PAGES - 1) {
                        app->current_page++;
                    }
                    break;
                case InputKeyLeft:
                    if(app->current_page > 0) {
                        app->current_page--;
                    }
                    break;
                case InputKeyOk:
                    if(!app->is_testing && app->current_page == 2) {
                        perform_sd_test(app);
                    }
                    break;
                case InputKeyBack:
                    running = false;
                    break;
                default:
                    break;
                }
            }
        }
        get_sd_info(app);
        view_port_update(app->view_port);
    }

    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    free(app);

    return 0;
}

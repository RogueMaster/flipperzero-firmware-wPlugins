/**
 * @file vario_app.c
 * @brief Вариометр для Flipper Zero на базе датчика BME280.
 *
 * Layout экрана 128×64:
 *
 *   x=0..13   y=0..63  — VBAR: полоса вертикальной скорости
 *   x=14..127 y=0..15  — Высота (FontBigNumbers) + "m"
 *   x=14..127 y=18..49 — График мин/макс скорости за 2с (114px = ~228с истории)
 *   x=14..127 y=50..56 — Вертикальная скорость (FontSecondary)
 *   x=14..127 y=57..63 — Температура + давление (FontSecondary)
 *
 * Меню (3 пункта):
 *   [0] Sound      — вкл/выкл звук
 *   [1] Backlight  — принудительная подсветка
 *   [2] Correct Alt.— Up/Down ±10м, OK применить, < отмена, > обнуление
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <datetime/datetime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bme280.h"

#define TAG "Vario"

/* ── Фильтр Калмана ───────────────────────────────────────────────────── */
#define KF_VAR_MEASUREMENT 0.1f
#define KF_VAR_ACCEL       0.75f
#define SEA_LEVEL_PRESSURE 101325.0f

/* ── Звук ─────────────────────────────────────────────────────────────── */
#define VARIO_CLIMB_THRESHOLD  0.3f
#define VARIO_SINK_THRESHOLD  -0.9f

/* ── Полоса вертикальной скорости (VBAR) ─────────────────────────────── */
#define VBAR_X      4
#define VBAR_W      6
#define VBAR_CY    32    /* центр по Y */
#define VBAR_HALF  26    /* полувысота шкалы в пикселях */
#define VBAR_MAX_MS 5.0f /* скорость при полном отклонении */

/* ── График скоростей ─────────────────────────────────────────────────── */
/*
 * Область: x=14..127 (113px), y=18..49 (32px), центр y=33.
 * Каждый столбец = одна 2-секундная выборка.
 * Новые данные появляются СЛЕВА, старые уходят ВПРАВО.
 * Буфер 114 элементов — история ~228 секунд.
 *
 * Таблица |скорость| → смещение от центра в пикселях:
 *   <0.5 м/с   → 0 (середина)
 *   0.5..0.99  → 1
 *   1.0..1.99  → 2
 *   2.0..4.99  → 3
 *   5.0..10.0  → 4
 *   >10.0      → 5
 */
#define GRAPH_X    15    /* x начала графика (на 1px правее VBAR) */
#define GRAPH_W   113    /* ширина в пикселях = 128 - GRAPH_X */
#define GRAPH_CY   33    /* центральная линия графика по Y */
#define GRAPH_TOP  18    /* верхняя граница области графика */
#define GRAPH_BOT  49    /* нижняя граница области графика */
#define GRAPH_INTERVAL_MS 2000u /* интервал накопления одной точки */

/* ── Лог ──────────────────────────────────────────────────────────────── */
#define LOG_FILE        "/ext/VarioLog.csv"
#define LOG_INTERVAL_MS 30000u  /* 30 секунд */

/* ── Меню ─────────────────────────────────────────────────────────────── */
/*
 * 3 пункта (индексы 0..2).
 * Экран: заголовок до y=13, доступно y=14..63 = 50px.
 * FontSecondary: высота 8px, baseline = нижний край.
 * FIRST_Y=27, STEP=14 → пункт 2: baseline=55, frame_bot=56 ✓
 *
 * Пункт 2 (коррекция высоты) имеет особый режим редактирования:
 * при нажатии OK переходим в режим menu_editing=true,
 * Up/Down меняют alt_correction ±10м, OK применяет и выходит из режима.
 */
#define MENU_FONT_H    8
#define MENU_ROW_STEP 14
#define MENU_FIRST_Y  27
#define MENU_FRAME_H  (MENU_FONT_H + 2)
#define MENU_COUNT     3
#define MENU_ROW_Y(i)     (MENU_FIRST_Y + (i) * MENU_ROW_STEP)
#define MENU_FRAME_TOP(i) (MENU_ROW_Y(i) - MENU_FONT_H - 1)

/* ── Настройки ────────────────────────────────────────────────────────── */
typedef struct {
    bool  sound_enabled;
    bool  backlight_forced;
    float alt_correction; /* поправка высоты в метрах */
} VarioSettings;

/* ── Данные одной точки графика ───────────────────────────────────────── */
typedef struct {
    int8_t min_px; /* смещение min скорости от центра (отриц. = вниз) */
    int8_t max_px; /* смещение max скорости от центра (полож. = вверх) */
    bool   valid;  /* true если данные есть */
} GraphPoint;

/* ── Состояние приложения ─────────────────────────────────────────────── */
typedef struct {
    /* Фильтр Калмана */
    float kf_x_abs;
    float kf_x_vel;
    float kf_p_abs_abs;
    float kf_p_abs_vel;
    float kf_p_vel_vel;
    float kf_var_accel;

    /* Данные датчика */
    float pressure;
    float temperature;
    float altitude;     /* сырая высота из барометра */
    float vario;
    float varioS;

    /* Датчик */
    bool    sensor_ready;
    BME280* bme280;

    /* Настройки и UI */
    VarioSettings settings;
    bool          show_menu;
    uint8_t       menu_selection;  /* 0..2 */
    bool          menu_editing;    /* режим редактирования alt_correction */
    float         alt_edit_value;  /* временное значение при редактировании */

    /* Подсветка */
    uint32_t last_button_time;

    /* Лог */
    uint32_t last_log_ms;      /* тик последней записи */
    uint32_t app_start_ms;     /* тик запуска приложения */

    /* График */
    GraphPoint graph[GRAPH_W]; /* [0]=самый новый (левый), [W-1]=старый (правый) */
    uint32_t   graph_last_ms;  /* тик последнего сбора точки */
    float      graph_vmin;     /* минимум за текущий 2с интервал */
    float      graph_vmax;     /* максимум за текущий 2с интервал */
    bool       graph_has_data; /* есть ли данные в текущем интервале */

    /* Синхронизация */
    FuriMutex*    mutex;
    volatile bool running;
} VarioData;

static VarioData*       vario_data   = NULL;
static NotificationApp* notification = NULL;

/* ── Вспомогательное ──────────────────────────────────────────────────── */

static float pressure2altitude(float pressure) {
    return 44330.0f * (1.0f - powf(pressure / SEA_LEVEL_PRESSURE, 0.190295f));
}

static void kf_reset(float abs_val, float vel_val) {
    vario_data->kf_x_abs     = abs_val;
    vario_data->kf_x_vel     = vel_val;
    vario_data->kf_p_abs_abs = 1.0e9f;
    vario_data->kf_p_abs_vel = 0.0f;
    vario_data->kf_p_vel_vel = KF_VAR_ACCEL;
    vario_data->kf_var_accel = KF_VAR_ACCEL;
}

/**
 * Переводит абсолютное значение скорости (м/с) в смещение пикселей от
 * центральной линии графика согласно таблице из ТЗ.
 */
static int speed_to_px(float v) {
    float av = fabsf(v);
    if(av < 0.5f)  return 0;
    if(av < 1.0f)  return 1;
    if(av < 2.0f)  return 2;
    if(av < 5.0f)  return 3;
    if(av <= 10.0f) return 4;
    return 5;
}

/* ── Инициализация датчика ────────────────────────────────────────────── */

static bool bme280_init_sensor(void) {
    if(vario_data->bme280) {
        free(vario_data->bme280);
        vario_data->bme280 = NULL;
    }
    vario_data->bme280 = malloc(sizeof(BME280));
    if(!vario_data->bme280) return false;
    memset(vario_data->bme280, 0, sizeof(BME280));

    bool ok = bme280_init(vario_data->bme280, BME280_I2C_ADDRESS_1,
                          &furi_hal_i2c_handle_external);
    if(!ok) {
        FURI_LOG_I(TAG, "0x76 failed, trying 0x77...");
        ok = bme280_init(vario_data->bme280, BME280_I2C_ADDRESS_2,
                         &furi_hal_i2c_handle_external);
    }
    if(!ok) {
        free(vario_data->bme280);
        vario_data->bme280 = NULL;
        return false;
    }
    bme280_set_filter(vario_data->bme280, BME280_FILTER_16);
    bme280_set_standby(vario_data->bme280, BME280_STANDBY_125);
    bme280_set_temp_oversample(vario_data->bme280, BME280_OS_2X);
    bme280_set_press_oversample(vario_data->bme280, BME280_OS_4X);
    bme280_set_humid_oversample(vario_data->bme280, BME280_OS_1X);
    bme280_set_mode(vario_data->bme280, BME280_MODE_NORMAL);
    FURI_LOG_I(TAG, "BME280 OK");
    return true;
}

/* ── Лог ──────────────────────────────────────────────────────────────── */

/**
 * Инициализация лога при старте приложения:
 * очищает файл если существует, создаёт заново.
 */
static void log_init(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    /* Удаляем старый файл — storage_simply_remove не вернёт ошибку
     * если файла нет, это нормально. */
    storage_simply_remove(storage, LOG_FILE);
    furi_record_close(RECORD_STORAGE);
    vario_data->last_log_ms  = furi_get_tick();
    vario_data->app_start_ms = furi_get_tick();
}

/**
 * Записывает строку в лог каждые 30 секунд.
 * Формат: время работы (сек); высота (м)
 */
static void log_to_file(void) {
    uint32_t now = furi_get_tick();
    if(now - vario_data->last_log_ms < LOG_INTERVAL_MS) return;

    furi_mutex_acquire(vario_data->mutex, FuriWaitForever);
    float alt = vario_data->altitude + vario_data->settings.alt_correction;
    furi_mutex_release(vario_data->mutex);

    uint32_t elapsed_sec = (now - vario_data->app_start_ms) / 1000u;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    if(storage_file_open(file, LOG_FILE, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        char line[64];
        snprintf(line, sizeof(line), "%lu;%.1f\n",
                 (unsigned long)elapsed_sec, (double)alt);
        storage_file_write(file, line, strlen(line));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    vario_data->last_log_ms = now;
}

/* ── Фильтр Калмана ───────────────────────────────────────────────────── */

static float last_time_kf = 0.0f;

static void update_pressure(void) {
    if(!vario_data->sensor_ready) return;

    float temp, press, humid;
    if(!bme280_read_sensor(vario_data->bme280, &temp, &press, &humid)) return;

    float t_now = (float)furi_get_tick() / 1000.0f;
    float dt    = t_now - last_time_kf;
    last_time_kf = t_now;
    if(dt <= 0.0f || dt > 1.0f) dt = 0.2f;

    furi_mutex_acquire(vario_data->mutex, FuriWaitForever);

    vario_data->temperature = temp;
    vario_data->pressure    = press;

    /* Predict */
    vario_data->kf_x_abs += vario_data->kf_x_vel * dt;
    float dt2 = dt*dt, dt3 = dt2*dt, dt4 = dt3*dt;
    vario_data->kf_p_abs_abs += 2.0f*dt*vario_data->kf_p_abs_vel
                              + dt2*vario_data->kf_p_vel_vel
                              + vario_data->kf_var_accel*dt4/4.0f;
    vario_data->kf_p_abs_vel += dt*vario_data->kf_p_vel_vel
                              + vario_data->kf_var_accel*dt3/2.0f;
    vario_data->kf_p_vel_vel += vario_data->kf_var_accel*dt2;

    /* Update */
    float y     = press - vario_data->kf_x_abs;
    float s_inv = 1.0f / (vario_data->kf_p_abs_abs + KF_VAR_MEASUREMENT);
    float k_abs = vario_data->kf_p_abs_abs * s_inv;
    float k_vel = vario_data->kf_p_abs_vel * s_inv;
    vario_data->kf_x_abs += k_abs * y;
    vario_data->kf_x_vel += k_vel * y;
    vario_data->kf_p_vel_vel -= vario_data->kf_p_abs_vel * k_vel;
    vario_data->kf_p_abs_vel -= vario_data->kf_p_abs_vel * k_abs;
    vario_data->kf_p_abs_abs -= vario_data->kf_p_abs_abs * k_abs;

    float altitude = pressure2altitude(vario_data->kf_x_abs);
    vario_data->altitude = altitude;
    vario_data->vario  = (altitude -
        pressure2altitude(vario_data->kf_x_abs - vario_data->kf_x_vel)) * 100.0f;
    vario_data->varioS = 0.7f * vario_data->varioS + 0.3f * vario_data->vario;

    /* Обновляем мин/макс для текущего интервала графика */
    float vs = vario_data->varioS * 0.01f; /* см/с → м/с */
    if(!vario_data->graph_has_data) {
        vario_data->graph_vmin = vs;
        vario_data->graph_vmax = vs;
        vario_data->graph_has_data = true;
    } else {
        if(vs < vario_data->graph_vmin) vario_data->graph_vmin = vs;
        if(vs > vario_data->graph_vmax) vario_data->graph_vmax = vs;
    }

    furi_mutex_release(vario_data->mutex);
}

/**
 * Раз в GRAPH_INTERVAL_MS сохраняет точку в буфер графика.
 * Сдвигает буфер: [0] — всегда самый новый элемент.
 * При отрисовке элемент [0] рисуется на x=GRAPH_X (левый край).
 */
static void graph_update(void) {
    uint32_t now = furi_get_tick();
    if(now - vario_data->graph_last_ms < GRAPH_INTERVAL_MS) return;
    vario_data->graph_last_ms = now;

    furi_mutex_acquire(vario_data->mutex, FuriWaitForever);

    /* Сдвигаем буфер вправо (старые данные → конец) */
    memmove(&vario_data->graph[1], &vario_data->graph[0],
            sizeof(GraphPoint) * (GRAPH_W - 1));

    GraphPoint pt;
    if(vario_data->graph_has_data) {
        float vmin = vario_data->graph_vmin;
        float vmax = vario_data->graph_vmax;
        int px_min = speed_to_px(vmin);
        int px_max = speed_to_px(vmax);

        /* Знак определяет направление от центра:
         * положительная скорость → вверх (отрицательный Y на экране)
         * отрицательная скорость → вниз (положительный Y на экране) */
        pt.min_px = (int8_t)(vmin >= 0.0f ? px_min : -px_min);
        pt.max_px = (int8_t)(vmax >= 0.0f ? px_max : -px_max);
        pt.valid  = true;
    } else {
        pt.min_px = 0;
        pt.max_px = 0;
        pt.valid  = false;
    }
    vario_data->graph[0] = pt;

    /* Сбрасываем аккумулятор для следующего интервала */
    vario_data->graph_has_data = false;

    furi_mutex_release(vario_data->mutex);
}

/* ── Звук ─────────────────────────────────────────────────────────────── */

static bool     sound_speaker_owned = false;
static uint32_t sound_next_beep     = 0;
static uint32_t sound_beep_end      = 0;

static void sound_stop(void) {
    if(sound_speaker_owned) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        sound_speaker_owned = false;
    }
    sound_beep_end  = 0;
    sound_next_beep = 0;
}

static void sound_update(void) {
    if(!vario_data || !vario_data->sensor_ready || !vario_data->settings.sound_enabled) {
        sound_stop();
        return;
    }
    uint32_t now = furi_get_tick();

    if(sound_speaker_owned && sound_beep_end != 0 && now >= sound_beep_end) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        sound_speaker_owned = false;
        sound_beep_end      = 0;
    }
    if(now < sound_next_beep) return;

    furi_mutex_acquire(vario_data->mutex, FuriWaitForever);
    float vspeed = vario_data->varioS * 0.01f;
    furi_mutex_release(vario_data->mutex);

    if(vspeed > VARIO_CLIMB_THRESHOLD) {
        uint32_t beep_ms = (uint32_t)(350 - (int)(vspeed * 50.0f));
        if(beep_ms < 50u) beep_ms = 50u;
        int freq = 1300 + (int)(vspeed * 100.0f);
        if(freq > 2300) freq = 2300;
        sound_next_beep = now + beep_ms + beep_ms / 4u;
        sound_beep_end  = now + beep_ms;
        if(furi_hal_speaker_acquire(10)) {
            sound_speaker_owned = true;
            furi_hal_speaker_start((float)freq, 1.0f);
        }
    } else if(vspeed < VARIO_SINK_THRESHOLD) {
        int bp = 350 + (int)(vspeed * 25.0f);
        if(bp < 50) bp = 50;
        uint32_t beep_ms = (uint32_t)bp;
        int freq = 1000 + (int)(vspeed * 50.0f);
        if(freq < 300) freq = 300;
        sound_next_beep = now + beep_ms + beep_ms / 4u;
        sound_beep_end  = now + beep_ms;
        if(furi_hal_speaker_acquire(10)) {
            sound_speaker_owned = true;
            furi_hal_speaker_start((float)freq, 1.0f);
        }
    } else {
        sound_next_beep = now + 100u;
    }
}

/* ── Подсветка ────────────────────────────────────────────────────────── */

/*
 * ИСПРАВЛЕНО: принудительная подсветка теперь посылает нотификацию
 * при каждом цикле пока active=true, а не только при смене состояния.
 * Это нужно потому что Flipper гасит подсветку по своему таймеру,
 * и нам нужно постоянно её переустанавливать.
 * Интервал переустановки: 4000мс (чуть меньше чем системный таймаут ~5с).
 */
static void update_backlight(void) {
    static uint32_t last_backlight_set = 0;
    uint32_t now = furi_get_tick();

    bool active = vario_data->settings.backlight_forced ||
                  (now - vario_data->last_button_time < 5000u);

    if(active) {
        /* Переустанавливаем подсветку каждые 4 секунды пока active */
        if(now - last_backlight_set >= 4000u) {
            notification_message(notification, &sequence_display_backlight_on);
            last_backlight_set = now;
        }
    }
}

/* ── Отрисовка VBAR ───────────────────────────────────────────────────── */

static void draw_vbar(Canvas* canvas, float vspeed) {
    int cx = VBAR_X + VBAR_W / 2;
    canvas_draw_frame(canvas, VBAR_X, VBAR_CY - VBAR_HALF, VBAR_W, VBAR_HALF * 2);
    canvas_draw_line(canvas, VBAR_X, VBAR_CY, VBAR_X + VBAR_W - 1, VBAR_CY);

    float clamped = vspeed;
    if(clamped >  VBAR_MAX_MS) clamped =  VBAR_MAX_MS;
    if(clamped < -VBAR_MAX_MS) clamped = -VBAR_MAX_MS;
    int fill_px = (int)(clamped / VBAR_MAX_MS * (float)VBAR_HALF);

    if(fill_px > 1) {
        int top = VBAR_CY - fill_px;
        for(int y = top; y < VBAR_CY; y++)
            canvas_draw_line(canvas, VBAR_X+1, y, VBAR_X+VBAR_W-2, y);
        for(int i = 0; i <= VBAR_W/2; i++)
            canvas_draw_line(canvas, cx-i, top-(VBAR_W/2-i), cx+i, top-(VBAR_W/2-i));
    } else if(fill_px < -1) {
        int bot = VBAR_CY - fill_px;
        for(int y = VBAR_CY+1; y <= bot; y++)
            canvas_draw_line(canvas, VBAR_X+1, y, VBAR_X+VBAR_W-2, y);
        for(int i = 0; i <= VBAR_W/2; i++)
            canvas_draw_line(canvas, cx-i, bot+(VBAR_W/2-i), cx+i, bot+(VBAR_W/2-i));
    }
}

/* ── Отрисовка графика ────────────────────────────────────────────────── */

/*
 * Рисует график скоростей.
 * Буфер graph[0..GRAPH_W-1]: [0]=новый (левый), [W-1]=старый (правый).
 * Экранная координата: x = GRAPH_X + (GRAPH_W - 1 - i)
 *   т.е. graph[0] рисуется на x = GRAPH_X + GRAPH_W - 1 = 127 — крайний правый.
 *
 * Нет! Условие: "каждые 2с появляются пиксели СЛЕВА, сдвигается вправо".
 * Значит новые данные — слева. graph[0] → x=GRAPH_X (левый).
 *   x = GRAPH_X + i
 */
static void draw_graph(Canvas* canvas) {
    /* Центральная пунктирная линия */
    for(int x = GRAPH_X; x <= GRAPH_X + GRAPH_W - 1; x += 3)
        canvas_draw_dot(canvas, x, GRAPH_CY);

    for(int i = 0; i < GRAPH_W; i++) {
        GraphPoint* pt = &vario_data->graph[i];
        if(!pt->valid) continue;

        int x = GRAPH_X + i; /* [0]=левый, [W-1]=правый */

        if(pt->min_px == 0 && pt->max_px == 0) {
            /* Нейтраль — точка на центральной линии */
            canvas_draw_dot(canvas, x, GRAPH_CY);
        } else {
            /* Минимум: min_px > 0 → вверх (y уменьшается), < 0 → вниз */
            int y_min = GRAPH_CY - pt->min_px;
            int y_max = GRAPH_CY - pt->max_px;

            /* Ограничиваем в пределах области */
            if(y_min < GRAPH_TOP) y_min = GRAPH_TOP;
            if(y_min > GRAPH_BOT) y_min = GRAPH_BOT;
            if(y_max < GRAPH_TOP) y_max = GRAPH_TOP;
            if(y_max > GRAPH_BOT) y_max = GRAPH_BOT;

            canvas_draw_dot(canvas, x, y_min);
            if(y_max != y_min)
                canvas_draw_dot(canvas, x, y_max);
        }
    }
}

/* ── Главный draw callback ────────────────────────────────────────────── */

static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    if(!vario_data) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 30, "Initializing...");
        return;
    }

    furi_mutex_acquire(vario_data->mutex, FuriWaitForever);

    /* ── Меню ─────────────────────────────────────────────────────────── */
    if(vario_data->show_menu) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 11, "Settings");
        canvas_draw_line(canvas, 0, 13, 127, 13);

        canvas_set_font(canvas, FontSecondary);

        /* Пункт 0: Sound */
        if(vario_data->menu_selection == 0)
            canvas_draw_frame(canvas, 0, MENU_FRAME_TOP(0), 128, MENU_FRAME_H);
        canvas_draw_str(canvas, 4, MENU_ROW_Y(0), "Sound:");
        canvas_draw_str(canvas, 90, MENU_ROW_Y(0),
                        vario_data->settings.sound_enabled ? "ON" : "OFF");

        /* Пункт 1: Backlight */
        if(vario_data->menu_selection == 1)
            canvas_draw_frame(canvas, 0, MENU_FRAME_TOP(1), 128, MENU_FRAME_H);
        canvas_draw_str(canvas, 4, MENU_ROW_Y(1), "Backlight:");
        canvas_draw_str(canvas, 90, MENU_ROW_Y(1),
                        vario_data->settings.backlight_forced ? "ON" : "OFF");

        /* Пункт 2: Correct Alt. */
        if(vario_data->menu_selection == 2)
            canvas_draw_frame(canvas, 0, MENU_FRAME_TOP(2), 128, MENU_FRAME_H);
        canvas_draw_str(canvas, 4, MENU_ROW_Y(2), "Correct Alt.:");

        /* В режиме редактирования показываем временное значение */
        float disp_corr = vario_data->menu_editing
                          ? vario_data->alt_edit_value
                          : vario_data->settings.alt_correction;
        char cbuf[16];
        snprintf(cbuf, sizeof(cbuf), "%+.0fm", (double)disp_corr);
        canvas_draw_str(canvas, 95, MENU_ROW_Y(2), cbuf);

        /* Подсказка в режиме редактирования */
        if(vario_data->menu_editing) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 4, 63, "^v:+-10m OK <Del >-Alt");
        }

        furi_mutex_release(vario_data->mutex);
        return;
    }

    /* ── Датчик не найден ─────────────────────────────────────────────── */
    if(!vario_data->sensor_ready) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 5, 20, "BME280 not found!");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 5, 35, "Check I2C wiring:");
        canvas_draw_str(canvas, 5, 45, "SCL=C0  SDA=C1");
        canvas_draw_str(canvas, 5, 55, "VCC=3.3V  GND=GND");
        furi_mutex_release(vario_data->mutex);
        return;
    }

    /* ── Основной экран ───────────────────────────────────────────────── */
    float vspeed = vario_data->varioS * 0.01f;
    float alt_display = vario_data->altitude + vario_data->settings.alt_correction;

    /* VBAR */
    draw_vbar(canvas, vspeed);

    char buf[32];

    /* ── Высота: x=14, y=0..14 ────────────────────────────────────────── *
     * FontBigNumbers: высота ~15px, baseline = нижний край.
     * Рисуем число с baseline=14, "m" сразу после числа той же строкой.
     * "m" рисуем FontSecondary baseline=14.
     *
     * Для определения ширины числа используем canvas_string_width,
     * но у нас нет этой функции без canvas — поэтому рисуем число,
     * а "m" ставим через canvas_draw_str_aligned с AlignLeft после числа.
     * Проще: рисуем число AlignLeft от x=15, "m" AlignLeft сразу после.
     * Максимум "-10000" = 6 символов × ~11px = 66px + "m" 6px = 72px ≤ 113 OK.
     */
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%.0f", (double)alt_display);
    /* Выравниваем число по правому краю области высоты (x=113), baseline=14 */
    canvas_draw_str_aligned(canvas, 113, 0, AlignRight, AlignTop, buf);

    /* "m" — FontSecondary сразу после числа, на той же baseline */
    canvas_set_font(canvas, FontSecondary);
    /* Рисуем "m" после числа: позиция ~x=116 (после 113 + небольшой отступ) */
    canvas_draw_str(canvas, 115, 13, "m");



    /* ── График: y=16..47 ─────────────────────────────────────────────── */
    draw_graph(canvas);

    /* ── Скорость: y=43..54 ───────────────────────────────────────────── */
    canvas_set_font(canvas, FontSecondary);
    if(fabsf(vspeed) < 0.01f)
        snprintf(buf, sizeof(buf), "0.00 m/s");
    else
        snprintf(buf, sizeof(buf), "%+.2f m/s", (double)vspeed);
    canvas_draw_str_aligned(canvas, 70, 56, AlignCenter, AlignBottom, buf);

    /* ── Температура, MUTE и давление: y=57..63 ─────────────────────────
     * Строка: [15] "18.5C"  [64 центр] "m"(если mute)  [127] "1013hPa" */
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%.1fC", (double)vario_data->temperature);
    canvas_draw_str(canvas, 15, 63, buf);
    if(!vario_data->settings.sound_enabled)
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "m");
    snprintf(buf, sizeof(buf), "%.0fhPa", (double)(vario_data->pressure / 100.0f));
    canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, buf);

    furi_mutex_release(vario_data->mutex);
}

/* ── Кнопки ───────────────────────────────────────────────────────────── */

static void handle_input(InputEvent* event, void* context) {
    FuriMessageQueue* queue = (FuriMessageQueue*)context;
    furi_message_queue_put(queue, event, 0);
}

/* ── Рабочий поток ────────────────────────────────────────────────────── */

static int32_t vario_worker(void* context) {
    UNUSED(context);

    sound_speaker_owned = false;
    sound_next_beep     = 0;
    sound_beep_end      = 0;

    vario_data->settings.sound_enabled    = true;
    vario_data->settings.backlight_forced = false;
    vario_data->settings.alt_correction   = 0.0f;
    vario_data->last_button_time          = furi_get_tick();
    vario_data->menu_selection            = 0;
    vario_data->menu_editing              = false;
    vario_data->alt_edit_value            = 0.0f;
    vario_data->varioS                    = 0.0f;
    vario_data->graph_last_ms             = furi_get_tick();
    vario_data->graph_has_data            = false;
    memset(vario_data->graph, 0, sizeof(vario_data->graph));

    /* Очищаем лог и запоминаем время старта */
    log_init();

    FURI_LOG_I(TAG, "Worker started, init BME280...");

    if(bme280_init_sensor()) {
        vario_data->sensor_ready = true;
        last_time_kf = (float)furi_get_tick() / 1000.0f;
        kf_reset(SEA_LEVEL_PRESSURE, 0.0f);
        FURI_LOG_I(TAG, "BME280 ready");
    } else {
        vario_data->sensor_ready = false;
        FURI_LOG_E(TAG, "BME280 not found, will retry");
    }

    uint32_t last_retry = 0;

    while(vario_data->running) {
        if(vario_data->sensor_ready) {
            update_pressure();
            graph_update();
            sound_update();
            log_to_file();
        } else {
            uint32_t now = furi_get_tick();
            if(now - last_retry > 5000u) {
                last_retry = now;
                if(bme280_init_sensor()) {
                    vario_data->sensor_ready = true;
                    last_time_kf = (float)furi_get_tick() / 1000.0f;
                    kf_reset(SEA_LEVEL_PRESSURE, 0.0f);
                }
            }
        }
        furi_delay_ms(200);
    }

    if(sound_speaker_owned) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        sound_speaker_owned = false;
    } else if(furi_hal_speaker_acquire(100)) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    FURI_LOG_I(TAG, "Worker stopped");
    return 0;
}

/* ── Точка входа ──────────────────────────────────────────────────────── */

int32_t vario_app(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    vario_data = malloc(sizeof(VarioData));
    memset(vario_data, 0, sizeof(VarioData));
    vario_data->mutex   = furi_mutex_alloc(FuriMutexTypeNormal);
    vario_data->running = true;

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, NULL);
    view_port_input_callback_set(view_port, handle_input, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    notification = furi_record_open(RECORD_NOTIFICATION);

    FuriThread* worker = furi_thread_alloc();
    furi_thread_set_name(worker, "VarioWorker");
    furi_thread_set_stack_size(worker, 4096);
    furi_thread_set_context(worker, vario_data);
    furi_thread_set_callback(worker, vario_worker);
    furi_thread_start(worker);

    InputEvent event;
    while(vario_data->running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(vario_data->mutex, FuriWaitForever);
            vario_data->last_button_time = furi_get_tick();

            /* Back вне меню = выход */
            if(event.type == InputTypeShort && event.key == InputKeyBack
               && !vario_data->show_menu) {
                vario_data->running = false;
                furi_mutex_release(vario_data->mutex);
                break;
            }

            if(event.type == InputTypeShort) {
                if(vario_data->show_menu) {
                    if(vario_data->menu_editing) {
                        /* Режим редактирования alt_correction:
                         *   Up/Down  — ±10 м
                         *   Left (<) — отмена: вернуть предыдущее значение
                         *   Right(>) — обнулить высоту: correction = -altitude
                         *   OK       — применить и выйти
                         *   Back     — отмена: вернуть предыдущее значение */
                        switch(event.key) {
                        case InputKeyUp:
                            vario_data->alt_edit_value += 10.0f;
                            break;
                        case InputKeyDown:
                            vario_data->alt_edit_value -= 10.0f;
                            break;
                        case InputKeyLeft:
                            /* Отмена — возврат к сохранённому значению */
                            vario_data->alt_edit_value =
                                vario_data->settings.alt_correction;
                            vario_data->menu_editing = false;
                            break;
                        case InputKeyRight:
                            /* Обнулить высоту: correction = -current_altitude,
                             * чтобы altitude + correction = 0 */
                            vario_data->alt_edit_value = -vario_data->altitude;
                            vario_data->settings.alt_correction =
                                vario_data->alt_edit_value;
                            vario_data->menu_editing = false;
                            break;
                        case InputKeyOk:
                            /* Применяем поправку и выходим */
                            vario_data->settings.alt_correction =
                                vario_data->alt_edit_value;
                            vario_data->menu_editing = false;
                            break;
                        case InputKeyBack:
                            /* Отмена — возврат к сохранённому значению */
                            vario_data->alt_edit_value =
                                vario_data->settings.alt_correction;
                            vario_data->menu_editing = false;
                            break;
                        default:
                            break;
                        }
                    } else {
                        /* Обычная навигация по меню */
                        switch(event.key) {
                        case InputKeyUp:
                            vario_data->menu_selection =
                                (vario_data->menu_selection + (MENU_COUNT-1u)) % MENU_COUNT;
                            break;
                        case InputKeyDown:
                            vario_data->menu_selection =
                                (vario_data->menu_selection + 1u) % MENU_COUNT;
                            break;
                        case InputKeyOk:
                            switch(vario_data->menu_selection) {
                            case 0:
                                vario_data->settings.sound_enabled =
                                    !vario_data->settings.sound_enabled;
                                break;
                            case 1:
                                vario_data->settings.backlight_forced =
                                    !vario_data->settings.backlight_forced;
                                break;
                            case 2:
                                /* Входим в режим редактирования высоты */
                                vario_data->alt_edit_value =
                                    vario_data->settings.alt_correction;
                                vario_data->menu_editing = true;
                                break;
                            default:
                                break;
                            }
                            break;
                        case InputKeyBack:
                            vario_data->show_menu = false;
                            break;
                        default:
                            break;
                        }
                    }
                } else {
                    /* Основной экран */
                    switch(event.key) {
                    case InputKeyUp:
                        vario_data->settings.sound_enabled =
                            !vario_data->settings.sound_enabled;
                        break;
                    case InputKeyDown:
                        vario_data->settings.backlight_forced =
                            !vario_data->settings.backlight_forced;
                        break;
                    case InputKeyOk:
                        vario_data->show_menu      = true;
                        vario_data->menu_selection = 0;
                        vario_data->menu_editing   = false;
                        break;
                    default:
                        break;
                    }
                }
            }

            furi_mutex_release(vario_data->mutex);
            view_port_update(view_port);
        }
        update_backlight();
        view_port_update(view_port);
    }

    vario_data->running = false;
    furi_thread_join(worker);
    furi_thread_free(worker);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    furi_mutex_free(vario_data->mutex);
    if(vario_data->bme280) free(vario_data->bme280);
    free(vario_data);
    vario_data = NULL;

    furi_message_queue_free(event_queue);
    return 0;
}

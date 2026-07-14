#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <math.h>
#include "logo.xbm"
#include "ready_3.xbm"
#include "ready_2.xbm"
#include "ready_1.xbm"
#include "over.xbm"
#include "ready3ru.xbm"
#include "ready2ru.xbm"
#include "ready1ru.xbm"
#include "overru.xbm"
#include "font_rus.h"

// Определение текстов для уровней сложности
typedef struct {
    const char* eng;
    const char* rus;
} LocalizedText;

// UTF-8 коды для русских слов:
// ЛЕГКО:     0xD0 0x9B 0xD0 0x95 0xD0 0x93 0xD0 0x9A 0xD0 0x9E
// НОРМАЛЬНО: 0xD0 0x9D 0xD0 0x9E 0xD0 0xA0 0xD0 0x9C 0xD0 0x90 0xD0 0x9B 0xD0 0x9D 0xD0 0x9E
// СЛОЖНО:    0xD0 0xA1 0xD0 0x9B 0xD0 0x9E 0xD0 0x96 0xD0 0x9D 0xD0 0x9E

static const char RUS_EASY[] = {
    0xD0,
    0x9B, // Л
    0xD0,
    0x95, // Е
    0xD0,
    0x93, // Г
    0xD0,
    0x9A, // К
    0xD0,
    0x9E, // О
    0};

static const char RUS_MEDIUM[] = {
    0xD0,
    0x9D, // Н
    0xD0,
    0x9E, // О
    0xD0,
    0xA0, // Р
    0xD0,
    0x9C, // М
    0xD0,
    0x90, // А
    0xD0,
    0x9B, // Л
    0xD0,
    0xAC, // Ь
    0xD0,
    0x9D, // Н
    0xD0,
    0x9E, // О
    0};

static const char RUS_HARD[] = {
    0xD0,
    0xA1, // С
    0xD0,
    0x9B, // Л
    0xD0,
    0x9E, // О
    0xD0,
    0x96, // Ж
    0xD0,
    0x9D, // Н
    0xD0,
    0x9E, // О
    0};

static const LocalizedText DIFFICULTY_TEXTS[] = {
    {.eng = "Easy", .rus = RUS_EASY}, // Легко
    {.eng = "Medium", .rus = RUS_MEDIUM}, // Средний
    {.eng = "Hard", .rus = RUS_HARD} // Сложно
};

#define SCREEN_WIDTH          128
#define SCREEN_HEIGHT         64
#define PLAYER_WIDTH          9
#define PLAYER_HEIGHT         8
#define PLAYER_SPEED          1.0f
#define GAP_START             58.0f
#define GAP_MIN               40.0f
#define LEVEL_WIDTH           SCREEN_WIDTH
#define HIGHSCORE_PATH_EASY   "/ext/wave_game/highscore_easy.txt"
#define HIGHSCORE_PATH_MEDIUM "/ext/wave_game/highscore_medium.txt"
#define HIGHSCORE_PATH_HARD   "/ext/wave_game/highscore_hard.txt"
#define TRAIL_LENGTH          64
#define START_DELAY_MS        2000
#define LANGUAGE_PATH         "/ext/wave_game/language.txt"

// Определения для бонусов
#define BONUS_WIDTH                 12
#define BONUS_HEIGHT                12
#define ARROW_WIDTH                 10 // Increased from 8
#define ARROW_HEIGHT                10 // Increased from 8
#define BONUS_HITBOX_PADDING        2
#define SCORE_MULTIPLIER_DURATION   2000 // Длительность бонуса множителя очков (2 секунды)
#define TUNNEL_EXPANDER_DURATION    3000
#define TUNNEL_EXPAND_FACTOR        1.2f
#define BONUS_SPAWN_CHANCE          50
#define MAX_ACTIVE_BONUSES          10
#define BONUS_SPAWN_SCORE_THRESHOLD 300 // Бонусы начнут появляться после 300 очков
#define BONUS_BLINK_DISTANCE        20
#define BONUS_BLINK_SPEED           100
#define BONUS_REMOVAL_OFFSET        5

// Прототипы функций
void draw_pause_icon(Canvas* canvas, int x, int y);
void draw_big_arrow_right(Canvas* canvas, int x, int y);

typedef enum {
    BonusTypeNone = 0,
    BonusTypeScoreMultiplier,
    BonusTypeTunnelExpander
} BonusType;

typedef struct {
    float x;
    float y;
    BonusType type;
    bool active;
    bool should_blink;
    uint32_t last_blink;
    bool visible;
} Bonus;

typedef struct {
    uint32_t start_time;
    uint32_t duration;
    uint32_t pause_time; // Время, когда бонус был поставлен на паузу
    uint32_t total_pause_time; // Общее время на паузе
    bool active;
    bool visible;
    uint8_t power; // Степень двойки для множителя (1 = x2, 2 = x4, 3 = x8, etc.)
} BonusEffect;

typedef struct {
    BonusEffect score_multiplier;
    uint32_t multiplier_count;
    BonusEffect tunnel_expander;
    float original_gap;
    float target_gap;
} ActiveBonusEffects;

typedef enum {
    GameStateMainMenu,
    GameStatePlaying,
    GameStateGameOver
} GameScreenState;

typedef enum {
    DifficultyEasy = 0,
    DifficultyMedium,
    DifficultyHard,
    DifficultyCount
} GameDifficulty;

typedef enum {
    LanguageEng,
    LanguageRus
} GameLanguage;

typedef struct {
    float speed_multiplier;
} DifficultySettings;

static const DifficultySettings DIFFICULTY_CONFIGS[DifficultyCount] = {
    {.speed_multiplier = 0.7f},
    {.speed_multiplier = 1.0f},
    {.speed_multiplier = 1.3f}};

typedef struct {
    float x, y;
    float speed;
    bool button_pressed;
} Player;

typedef struct {
    float x, y;
    bool active;
} TrailSegment;

typedef struct {
    Player player;
    uint8_t level[LEVEL_WIDTH];
    TrailSegment trail[TRAIL_LENGTH];
    int write_index;
    int8_t dir;
    float gap;
    float target_gap;
    float score_acc;
    float score;
    int highscores[DifficultyCount];
    bool game_over;
    bool restart_requested;
    bool exit_requested;
    bool paused;
    bool start_delay_active;
    uint32_t start_time;
    uint32_t countdown_start;
    int current_countdown;
    float speed_multiplier;
    uint32_t warmup_start;
    bool warmup_active;
    GameScreenState screen_state;
    GameDifficulty selected_difficulty;
    int menu_selected_item;
    bool menu_transition_active;
    uint32_t arrow_blink_time;
    bool arrow_visible;
    uint32_t menu_text_switch_time;
    uint32_t last_menu_selection_time;
    bool show_scores;
    GameLanguage current_language;
    bool language_selection;
    FuriMutex* mutex;
    Bonus bonuses[MAX_ACTIVE_BONUSES];
    ActiveBonusEffects bonus_effects;
    uint32_t last_bonus_spawn;
    bool bonus_blink_state;
    uint32_t bonus_blink_time;
    // Добавляем переменные для анимации паузы
    bool pause_animation_active;
    uint32_t pause_animation_start;
    bool pause_blink_state;
    uint32_t pause_blink_time;
    int pause_countdown;
    // Добавляем новые переменные для отслеживания нажатий
    uint32_t last_click_time; // Время последнего нажатия
    uint32_t click_interval; // Интервал между нажатиями
    float click_multiplier; // Множитель очков от частоты нажатий
    uint32_t last_click_reset; // Время последнего сброса множителя
} GameState;

// Прототипы функций
void spawn_bonus(GameState* g, float x);
void update_bonuses(GameState* g);
void draw_bonuses(Canvas* canvas, GameState* g);

int interpolate(int x0, int y0, int x1, int y1, int y) {
    if(y1 == y0) return x0;
    return x0 + (x1 - x0) * (y - y0) / (y1 - y0);
}

const char* get_highscore_path(GameDifficulty difficulty) {
    switch(difficulty) {
    case DifficultyEasy:
        return HIGHSCORE_PATH_EASY;
    case DifficultyMedium:
        return HIGHSCORE_PATH_MEDIUM;
    case DifficultyHard:
        return HIGHSCORE_PATH_HARD;
    default:
        return HIGHSCORE_PATH_EASY;
    }
}

int load_highscore(GameDifficulty difficulty) {
    int hs = 0;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, get_highscore_path(difficulty), FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[16];
        int len = storage_file_read(file, buf, sizeof(buf) - 1);
        if(len > 0) {
            buf[len] = '\0';
            hs = atoi(buf);
        }
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return hs;
}

void ensure_directory_exists(Storage* storage) {
    if(!storage_dir_exists(storage, "/ext/wave_game")) {
        storage_common_mkdir(storage, "/ext/wave_game");
    }
}

void save_highscore(GameDifficulty difficulty, int hs) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_directory_exists(storage);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, get_highscore_path(difficulty), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[16];
        int len = snprintf(buf, sizeof(buf), "%d", hs);
        storage_file_write(file, buf, len);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void load_all_highscores(GameState* g) {
    for(int i = 0; i < DifficultyCount; i++) {
        g->highscores[i] = load_highscore(i);
    }
}

void game_reset(GameState* g) {
    g->player.x = 40;
    g->player.y = SCREEN_HEIGHT / 2;
    g->player.speed = PLAYER_SPEED;
    g->player.button_pressed = false;
    g->write_index = 0;
    g->dir = 1;
    g->gap = GAP_START;
    g->target_gap = GAP_START;
    uint8_t start = (SCREEN_HEIGHT + 1 - (int)g->gap) / 2;
    for(int i = 0; i < LEVEL_WIDTH; i++)
        g->level[i] = start;
    for(int i = 0; i < TRAIL_LENGTH; i++) {
        g->trail[i].y = -1;
        g->trail[i].x = -1;
        g->trail[i].active = false;
    }
    g->score_acc = 0.0f;
    g->score = 0.0f;
    g->game_over = false;
    g->restart_requested = false;
    g->exit_requested = false;
    g->paused = false;
    g->start_delay_active = true;
    g->start_time = furi_get_tick();
    g->countdown_start = g->start_time;
    g->current_countdown = 3;
    g->speed_multiplier = DIFFICULTY_CONFIGS[g->selected_difficulty].speed_multiplier * 0.3f;
    g->warmup_active = false;
    g->warmup_start = 0;

    // Инициализация бонусов
    for(int i = 0; i < MAX_ACTIVE_BONUSES; i++) {
        g->bonuses[i].active = false;
    }
    g->bonus_effects.score_multiplier.active = false;
    g->bonus_effects.multiplier_count = 0;
    g->bonus_effects.tunnel_expander.active = false;
    g->bonus_effects.tunnel_expander.duration = 0;
    g->bonus_effects.tunnel_expander.pause_time = 0;
    g->bonus_effects.tunnel_expander.total_pause_time = 0;
    g->bonus_effects.original_gap = g->gap;
    g->bonus_effects.target_gap = g->gap;
    g->last_bonus_spawn = 0;
    g->bonus_blink_state = true;
    g->bonus_blink_time = furi_get_tick();
    g->bonus_effects.tunnel_expander.start_time = 0;
    g->bonus_effects.score_multiplier.active = false;
    g->bonus_effects.score_multiplier.duration = 0;
    g->bonus_effects.score_multiplier.visible = true;
    g->bonus_effects.score_multiplier.power = 0;
    g->last_click_time = 0;
    g->click_interval = 1000;
    g->click_multiplier = 1.0f;
    g->last_click_reset = 0;
}

// Функция для загрузки сохраненного языка
GameLanguage load_language() {
    GameLanguage lang = LanguageRus; // По умолчанию русский
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, LANGUAGE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[8];
        uint16_t bytes_read = storage_file_read(file, buf, sizeof(buf) - 1);
        if(bytes_read > 0) {
            buf[bytes_read] = '\0';
            if(strcmp(buf, "en") == 0) {
                lang = LanguageEng;
            }
        }
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return lang;
}

// Функция для сохранения выбранного языка
void save_language(GameLanguage lang) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_directory_exists(storage);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, LANGUAGE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* lang_str = (lang == LanguageEng) ? "en" : "ru";
        storage_file_write(file, lang_str, strlen(lang_str));
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void game_init(GameState* g) {
    g->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    g->screen_state = GameStateMainMenu;
    g->selected_difficulty = DifficultyEasy;
    g->menu_selected_item = 0;
    g->menu_text_switch_time = furi_get_tick();
    g->show_scores = false;
    g->language_selection = false;
    g->pause_animation_active = false;
    g->pause_blink_state = true;
    g->pause_blink_time = 0;
    g->pause_countdown = 3;
    g->last_click_time = 0;
    g->click_interval = 1000;
    g->click_multiplier = 1.0f;
    g->last_click_reset = 0;

    // Загружаем рекорды и язык при старте
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_directory_exists(storage);
    furi_record_close(RECORD_STORAGE);
    load_all_highscores(g);
    g->current_language = load_language();

    game_reset(g);
}

void generate_next_column(GameState* g) {
    int prev = (g->write_index + LEVEL_WIDTH - 1) % LEVEL_WIDTH;
    uint8_t cur = g->level[prev];
    if(cur == 0)
        g->dir = 1;
    else if(cur + (int)g->gap >= SCREEN_HEIGHT)
        g->dir = -1;
    else if(rand() % 21 == 0)
        g->dir = -g->dir;
    cur += g->dir;
    g->level[g->write_index] = cur;

    // Генерация бонусов
    uint32_t now = furi_get_tick();
    if(now - g->last_bonus_spawn > 1000 && rand() % BONUS_SPAWN_CHANCE == 0) {
        spawn_bonus(g, SCREEN_WIDTH);
    }

    g->write_index = (g->write_index + 1) % LEVEL_WIDTH;
}

void draw_tunnel(Canvas* c, GameState* g) {
    for(int x = 0; x < LEVEL_WIDTH; x++) {
        int idx = (g->write_index + x) % LEVEL_WIDTH;
        uint8_t gs = g->level[idx];
        int ge = gs + (int)g->gap;
        canvas_draw_line(c, x, 0, x, gs);
        canvas_draw_line(c, x, ge, x, SCREEN_HEIGHT);
    }
}

void check_collision(GameState* g) {
    // Проверка столкновения с границами экрана
    if(g->player.y < 0 || g->player.y + PLAYER_HEIGHT > SCREEN_HEIGHT) {
        g->game_over = true;
        if((int)g->score > g->highscores[g->selected_difficulty]) {
            g->highscores[g->selected_difficulty] = (int)g->score;
            save_highscore(g->selected_difficulty, g->highscores[g->selected_difficulty]);
        }
        g->screen_state = GameStateGameOver;
        return;
    }

    int px = (int)g->player.x;
    if(px >= 0 && px < LEVEL_WIDTH) {
        int idx = (g->write_index + px) % LEVEL_WIDTH;
        uint8_t gs = g->level[idx];
        int ge = gs + (int)g->gap;

        // Проверка столкновения с учетом реального размера gap
        if(g->player.y < gs || g->player.y + PLAYER_HEIGHT > ge) {
            g->game_over = true;
            if((int)g->score > g->highscores[g->selected_difficulty]) {
                g->highscores[g->selected_difficulty] = (int)g->score;
                save_highscore(g->selected_difficulty, g->highscores[g->selected_difficulty]);
            }
            g->screen_state = GameStateGameOver;
        }
    }
}

void draw_player(Canvas* c, GameState* g) {
    (void)c;
    float angle = g->player.button_pressed ? -0.8f : 0.8f;
    float center_x = g->player.x;
    float center_y = g->player.y + PLAYER_HEIGHT / 2.0f;
    float half_w = PLAYER_WIDTH / 2.0f;
    float half_h = PLAYER_HEIGHT / 2.0f;
    float nose_x = center_x + half_w;
    float nose_y = center_y;
    float base_left_x = center_x - half_w;
    float base_left_y = center_y - half_h;
    float base_right_x = center_x - half_w;
    float base_right_y = center_y + half_h;
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    float rotate_x(float x, float y) {
        return cos_a * (x - center_x) - sin_a * (y - center_y) + center_x;
    }
    float rotate_y(float x, float y) {
        return sin_a * (x - center_x) + cos_a * (y - center_y) + center_y;
    }

    int x1 = (int)rotate_x(nose_x, nose_y);
    int y1 = (int)rotate_y(nose_x, nose_y);
    int x2 = (int)rotate_x(base_left_x, base_left_y);
    int y2 = (int)rotate_y(base_left_x, base_left_y);
    int x3 = (int)rotate_x(base_right_x, base_right_y);
    int y3 = (int)rotate_y(base_right_x, base_right_y);

    if(y2 > y3) {
        int tx = x2, ty = y2;
        x2 = x3;
        y2 = y3;
        x3 = tx;
        y3 = ty;
    }
    if(y1 > y2) {
        int tx = x1, ty = y1;
        x1 = x2;
        y1 = y2;
        x2 = tx;
        y2 = ty;
    }
    if(y2 > y3) {
        int tx = x2, ty = y2;
        x2 = x3;
        y2 = y3;
        x3 = tx;
        y3 = ty;
    }

    for(int y = y1; y <= y3; ++y) {
        int xa, xb;
        if(y < y2) {
            xa = interpolate(x1, y1, x2, y2, y);
            xb = interpolate(x1, y1, x3, y3, y);
        } else {
            xa = interpolate(x2, y2, x3, y3, y);
            xb = interpolate(x1, y1, x3, y3, y);
        }
        if(xa > xb) {
            int tmp = xa;
            xa = xb;
            xb = tmp;
        }
        canvas_draw_line(c, xa, y, xb, y);
    }
}

void draw_arrow(Canvas* canvas, int x, int y) {
    // Смещаем стрелку на 1 пиксель вверх для центрирования
    y -= 1;

    // Рисуем горизонтальную линию
    canvas_draw_line(canvas, x, y, x + 4, y);
    canvas_draw_line(canvas, x, y + 1, x + 4, y + 1);

    // Рисуем наконечник стрелки (развёрнутый вправо)
    canvas_draw_line(canvas, x + 4, y - 2, x + 8, y + 1);
    canvas_draw_line(canvas, x + 4, y + 3, x + 8, y);
    canvas_draw_line(canvas, x + 4, y - 1, x + 7, y + 1);
    canvas_draw_line(canvas, x + 4, y + 2, x + 7, y);
}

// Добавим функцию для отрисовки вертикальной стрелки
void draw_vertical_arrow(Canvas* canvas, int x, int y) {
    // Рисуем вертикальную линию
    canvas_draw_line(canvas, x, y, x, y + 4);
    canvas_draw_line(canvas, x + 1, y, x + 1, y + 4);

    // Рисуем наконечник стрелки (направлен вниз)
    canvas_draw_line(canvas, x - 2, y + 4, x + 1, y + 8);
    canvas_draw_line(canvas, x + 3, y + 4, x, y + 8);
    canvas_draw_line(canvas, x - 1, y + 4, x + 1, y + 7);
    canvas_draw_line(canvas, x + 2, y + 4, x, y + 7);
}

// Функция для отрисовки текста с учетом языка
void draw_localized_text(
    Canvas* canvas,
    int x,
    int y,
    Align horizontal,
    Align vertical,
    const LocalizedText* text,
    GameLanguage lang) {
    const char* display_text = (lang == LanguageEng) ? text->eng : text->rus;
    // Всегда используем кастомный шрифт для обоих языков
    draw_russian_text(canvas, x, y, horizontal, vertical, display_text);
}

// Функция для отрисовки иконки x2
void draw_multiplier_bonus(Canvas* canvas, int x, int y) {
    // Устанавливаем жирный шрифт
    canvas_set_font(canvas, FontPrimary);

    // Рисуем x2 в центре бонуса
    canvas_draw_str(
        canvas,
        x + BONUS_WIDTH / 2 - 4, // Центрируем по горизонтали с небольшой коррекцией
        y + BONUS_HEIGHT / 2 + 4, // Центрируем по вертикали с коррекцией для базовой линии
        "x2");

    // Возвращаем обычный шрифт
    canvas_set_font(canvas, FontSecondary);
}

// Специальная версия иконки для статуса расширения (без верхнего ряда)
void draw_expander_status_icon(Canvas* canvas, int x, int y, int size) {
    // Центрируем стрелку в заданной области
    int center_x = x + size / 2;
    int start_y = y + (size - ARROW_HEIGHT) / 2;

    // Рисуем стержень стрелки (4 пикселя, начиная на 1 пиксель ниже)
    canvas_draw_line(canvas, center_x - 1, start_y + 1, center_x - 1, start_y + 5);
    canvas_draw_line(canvas, center_x, start_y + 1, center_x, start_y + 5);
    canvas_draw_line(canvas, center_x + 1, start_y + 1, center_x + 1, start_y + 5);

    // Рисуем наконечник стрелки (7x5 пикселей)
    // Верхняя часть наконечника
    canvas_draw_line(canvas, center_x - 3, start_y + 5, center_x + 3, start_y + 5);
    // Средняя часть
    canvas_draw_line(canvas, center_x - 2, start_y + 6, center_x + 2, start_y + 6);
    // Нижняя часть
    canvas_draw_line(canvas, center_x - 1, start_y + 7, center_x + 1, start_y + 7);
    canvas_draw_dot(canvas, center_x, start_y + 8);
}

void draw_expander_status(Canvas* canvas, GameState* g) {
    if(!g->bonus_effects.tunnel_expander.active) return;

    const int base_x = 0;
    const int base_y = SCREEN_HEIGHT; // Максимально вниз
    const int icon_size = 10;
    const int padding = 2;

    // Вычисляем оставшееся время
    uint32_t now = furi_get_tick();
    uint32_t elapsed;

    if(g->paused) {
        elapsed = g->bonus_effects.tunnel_expander.pause_time -
                  g->bonus_effects.tunnel_expander.start_time -
                  g->bonus_effects.tunnel_expander.total_pause_time;
    } else {
        elapsed = now - g->bonus_effects.tunnel_expander.start_time -
                  g->bonus_effects.tunnel_expander.total_pause_time;
    }

    uint32_t remaining = (g->bonus_effects.tunnel_expander.duration - elapsed) / 1000;
    if(remaining > 99) remaining = 99;

    char time_text[8];
    snprintf(time_text, sizeof(time_text), "%d", (int)remaining + 1);
    int text_width = canvas_string_width(canvas, time_text);

    // Рисуем подложку
    int box_width = icon_size + text_width + padding * 2;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, base_x, base_y - icon_size, box_width, icon_size);

    // Рисуем белым цветом иконку и текст
    canvas_set_color(canvas, ColorWhite);
    draw_expander_status_icon(canvas, base_x, base_y - icon_size, icon_size);

    // Центрируем текст в оставшемся пространстве справа от иконки
    int text_x = base_x + icon_size + padding;

    canvas_draw_str(canvas, text_x, base_y - 2, time_text);

    canvas_set_color(canvas, ColorBlack);
}

void spawn_bonus(GameState* g, float x) {
    // Проверяем, достигнут ли порог очков для появления бонусов
    if(g->score < BONUS_SPAWN_SCORE_THRESHOLD) {
        return;
    }

    for(int i = 0; i < MAX_ACTIVE_BONUSES; i++) {
        if(!g->bonuses[i].active) {
            g->bonuses[i].active = true;
            g->bonuses[i].x = x;

            int idx = (g->write_index + (int)x) % LEVEL_WIDTH;
            uint8_t gs = g->level[idx];
            g->bonuses[i].y = gs + g->gap / 2 - BONUS_HEIGHT / 2;

            g->bonuses[i].type = (rand() % 2) ? BonusTypeScoreMultiplier : BonusTypeTunnelExpander;
            g->last_bonus_spawn = furi_get_tick();
            break;
        }
    }
}

void update_bonuses(GameState* g) {
    uint32_t now = furi_get_tick();

    // Обновляем состояние мигания
    if(now - g->bonus_blink_time >= 200) {
        g->bonus_blink_state = !g->bonus_blink_state;
        g->bonus_blink_time = now;
    }

    // Если игра на паузе, обновляем только время паузы
    if(g->paused) {
        if(g->bonus_effects.tunnel_expander.active &&
           g->bonus_effects.tunnel_expander.pause_time > 0) {
            g->bonus_effects.tunnel_expander.total_pause_time =
                now - g->bonus_effects.tunnel_expander.pause_time;
        }
        return;
    }

    // Обновляем эффект множителя очков
    if(g->bonus_effects.score_multiplier.active) {
        uint32_t elapsed = now - g->bonus_effects.score_multiplier.start_time;
        if(elapsed >= g->bonus_effects.score_multiplier.duration) {
            // Reset all multiplier states when expired
            g->bonus_effects.score_multiplier.active = false;
            g->bonus_effects.score_multiplier.power = 0;
            g->bonus_effects.score_multiplier.visible = true;
        } else if(elapsed >= g->bonus_effects.score_multiplier.duration - 500) {
            // Мигание в последние 500мс
            g->bonus_effects.score_multiplier.visible = g->bonus_blink_state;
        }
    }

    // Обновляем эффект расширения тоннеля
    if(g->bonus_effects.tunnel_expander.active) {
        uint32_t elapsed;

        if(g->paused) {
            elapsed = g->bonus_effects.tunnel_expander.pause_time -
                      g->bonus_effects.tunnel_expander.start_time -
                      g->bonus_effects.tunnel_expander.total_pause_time;
        } else {
            elapsed = now - g->bonus_effects.tunnel_expander.start_time -
                      g->bonus_effects.tunnel_expander.total_pause_time;
        }

        if(elapsed >= g->bonus_effects.tunnel_expander.duration) {
            // Время истекло, плавно возвращаем исходный размер
            g->bonus_effects.tunnel_expander.active = false;
            g->target_gap = g->bonus_effects.original_gap;
        } else if(elapsed >= g->bonus_effects.tunnel_expander.duration - 1000) {
            // Последняя секунда - плавное сужение
            float progress = (g->bonus_effects.tunnel_expander.duration - elapsed) / 1000.0f;
            g->target_gap =
                g->bonus_effects.original_gap +
                (g->bonus_effects.target_gap - g->bonus_effects.original_gap) * progress;
        } else {
            // Поддерживаем расширенный размер в течение действия бонуса
            g->target_gap = g->bonus_effects.target_gap;
        }

        // Плавно изменяем gap
        g->gap = g->gap * 0.95f + g->target_gap * 0.05f;

        // Проверяем и корректируем позицию игрока только если он близок к стенкам
        int px = (int)g->player.x;
        if(px >= 0 && px < LEVEL_WIDTH) {
            int idx = (g->write_index + px) % LEVEL_WIDTH;
            uint8_t gs = g->level[idx];
            float center = gs + g->gap / 2;
            float player_center = g->player.y + PLAYER_HEIGHT / 2;
            float dist_to_center = fabsf(player_center - center);

            if(dist_to_center > g->gap * 0.4f) {
                g->player.y = g->player.y * 0.9f + (center - PLAYER_HEIGHT / 2) * 0.1f;
            }
        }
    }

    // Перемещаем и проверяем столкновения с бонусами
    for(int i = 0; i < MAX_ACTIVE_BONUSES; i++) {
        if(g->bonuses[i].active) {
            g->bonuses[i].x -= g->speed_multiplier;

            float bonus_center_x = g->bonuses[i].x + BONUS_WIDTH / 2;
            float bonus_center_y = g->bonuses[i].y + BONUS_HEIGHT / 2;
            float player_center_x = g->player.x + PLAYER_WIDTH / 2;
            float player_center_y = g->player.y + PLAYER_HEIGHT / 2;

            if(bonus_center_x + BONUS_WIDTH / 2 < g->player.x) {
                g->bonuses[i].active = false;
                continue;
            }

            float dx = fabsf(bonus_center_x - player_center_x);
            float dy = fabsf(bonus_center_y - player_center_y);

            float collision_width = (BONUS_WIDTH + PLAYER_WIDTH) * 0.4f;
            float collision_height = (BONUS_HEIGHT + PLAYER_HEIGHT) * 0.4f;

            bool collision = false;
            if(dx <= collision_width && dy <= collision_height) {
                collision = true;
            } else if(dx <= collision_width * 1.2f && dy <= collision_height * 1.2f) {
                float normalized_dx = (dx - collision_width) / (collision_width * 0.2f);
                float normalized_dy = (dy - collision_height) / (collision_height * 0.2f);
                float corner_distance =
                    normalized_dx * normalized_dx + normalized_dy * normalized_dy;
                collision = (corner_distance <= 1.0f);
            }

            if(collision) {
                uint32_t current_time = furi_get_tick();

                if(g->bonuses[i].type == BonusTypeScoreMultiplier) {
                    // First activation or reactivation after expiration
                    if(!g->bonus_effects.score_multiplier.active) {
                        g->bonus_effects.score_multiplier.power = 1; // Start with x2
                    } else {
                        // Subsequent activations - increase multiplier
                        g->bonus_effects.score_multiplier.power++;
                    }

                    // Reset timer and set active state
                    g->bonus_effects.score_multiplier.active = true;
                    g->bonus_effects.score_multiplier.start_time = current_time;
                    g->bonus_effects.score_multiplier.duration = SCORE_MULTIPLIER_DURATION;
                    g->bonus_effects.score_multiplier.visible = true;
                    g->bonuses[i].active = false;
                } else if(g->bonuses[i].type == BonusTypeTunnelExpander) {
                    if(!g->bonus_effects.tunnel_expander.active) {
                        // Первая активация бонуса
                        int idx = (g->write_index + (int)g->player.x) % LEVEL_WIDTH;
                        uint8_t gs = g->level[idx];
                        float current_center = gs + g->gap / 2;
                        float relative_pos =
                            (g->player.y + PLAYER_HEIGHT / 2 - current_center) / (g->gap / 2);

                        g->bonus_effects.original_gap = g->gap;
                        g->bonus_effects.target_gap = g->gap * TUNNEL_EXPAND_FACTOR;
                        g->bonus_effects.tunnel_expander.duration = TUNNEL_EXPANDER_DURATION;
                        g->bonus_effects.tunnel_expander.start_time = current_time;
                        g->bonus_effects.tunnel_expander.total_pause_time = 0;
                        g->bonus_effects.tunnel_expander.pause_time = 0;
                        g->bonus_effects.tunnel_expander.active = true;

                        g->target_gap = g->bonus_effects.target_gap;

                        float new_center = gs + g->gap / 2;
                        g->player.y = new_center + relative_pos * (g->gap / 2) - PLAYER_HEIGHT / 2;
                    } else {
                        // Просто добавляем 3 секунды к текущей длительности
                        g->bonus_effects.tunnel_expander.duration += TUNNEL_EXPANDER_DURATION;
                    }
                    g->bonuses[i].active = false;
                }
            }
        }
    }
}

void draw_bonuses(Canvas* canvas, GameState* g) {
    // Отрисовка активных бонусов
    for(int i = 0; i < MAX_ACTIVE_BONUSES; i++) {
        if(g->bonuses[i].active && (!g->bonuses[i].should_blink || g->bonuses[i].visible)) {
            if(g->bonuses[i].type == BonusTypeScoreMultiplier) {
                draw_multiplier_bonus(canvas, (int)g->bonuses[i].x, (int)g->bonuses[i].y);
            } else if(g->bonuses[i].type == BonusTypeTunnelExpander) {
                draw_expander_status_icon(
                    canvas, (int)g->bonuses[i].x, (int)g->bonuses[i].y, BONUS_HEIGHT);
            }
        }
    }

    // Отрисовка множителя очков и счета в левом верхнем углу
    char score_text[16];
    snprintf(score_text, sizeof(score_text), "%d", (int)g->score);
    int score_width = measure_text_width(score_text);

    // Рисуем черную подложку для счета
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, score_width + 3, 9);

    // Рисуем белый текст счета
    canvas_set_color(canvas, ColorWhite);
    draw_russian_text(canvas, 0, 5, AlignLeft, AlignCenter, score_text);

    // Если есть активный множитель, рисуем его отдельным прямоугольником
    if(g->bonus_effects.score_multiplier.active && g->bonus_effects.score_multiplier.power > 0 &&
       g->bonus_effects.score_multiplier.visible) {
        char multiplier_text[8];
        int multiplier = 1 << g->bonus_effects.score_multiplier.power;
        snprintf(multiplier_text, sizeof(multiplier_text), "x%d", multiplier);
        int multiplier_width = measure_text_width(multiplier_text);

        // Рисуем подложку для множителя (на расстоянии 5 пикселей от счета)
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, score_width + 3, 0, multiplier_width + 2, 9);

        // Рисуем текст множителя
        canvas_set_color(canvas, ColorWhite);
        draw_russian_text(canvas, score_width + 3, 5, AlignLeft, AlignCenter, multiplier_text);
    }

    // Возвращаем черный цвет для последующей отрисовки
    canvas_set_color(canvas, ColorBlack);

    // Отрисовка статуса расширения тоннеля
    draw_expander_status(canvas, g);
}

void game_update(GameState* g) {
    if(g->screen_state != GameStatePlaying || g->game_over) {
        return;
    }

    uint32_t now = furi_get_tick();

    // Обработка анимации паузы
    if(g->pause_animation_active) {
        // Определяем время анимации
        uint32_t animation_elapsed = now - g->pause_animation_start;

        // Обновляем состояние мигания раз в секунду
        if(animation_elapsed % 1000 < 500) {
            g->pause_blink_state = true;
        } else {
            g->pause_blink_state = false;
        }

        // Завершение анимации после 3 секунд
        if(animation_elapsed >= 3000) {
            g->pause_animation_active = false;
            g->paused = false;
            g->pause_blink_state = true;

            // При снятии с паузы обновляем общее время паузы для бонусов
            if(g->bonus_effects.tunnel_expander.active &&
               g->bonus_effects.tunnel_expander.pause_time > 0) {
                g->bonus_effects.tunnel_expander.total_pause_time +=
                    now - g->bonus_effects.tunnel_expander.pause_time;
                g->bonus_effects.tunnel_expander.pause_time = 0;
            }
        }
        return;
    }

    if(g->paused) {
        return;
    }

    if(g->screen_state != GameStatePlaying || g->game_over || g->paused) {
        return;
    }

    if(g->start_delay_active) {
        if(now - g->start_time >= START_DELAY_MS) {
            g->start_delay_active = false;
            g->warmup_active = true;
            g->warmup_start = now;
        }
        return; // Выходим, пока идет отсчет
    }

    if(g->warmup_active) {
        uint32_t warmup_elapsed = now - g->warmup_start;
        if(warmup_elapsed < 3000) {
            float base_multiplier = DIFFICULTY_CONFIGS[g->selected_difficulty].speed_multiplier;
            g->speed_multiplier = base_multiplier * (0.3f + (float)warmup_elapsed * 0.00023333f);
            float progress = warmup_elapsed * 0.00033333f;
            g->target_gap = GAP_START - (GAP_START - GAP_MIN) * progress;
        } else {
            g->warmup_active = false;
            g->speed_multiplier = DIFFICULTY_CONFIGS[g->selected_difficulty].speed_multiplier;
            g->target_gap = GAP_MIN;
        }
    }

    if(g->gap > g->target_gap) {
        g->gap = g->gap * 0.95f + g->target_gap * 0.05f;
    }

    float current_speed = g->player.speed * g->speed_multiplier;
    if(g->player.button_pressed) {
        g->player.y -= current_speed;
    } else {
        g->player.y += current_speed;
    }

    for(int i = 0; i < TRAIL_LENGTH - 1; i++) {
        g->trail[i] = g->trail[i + 1];
    }
    g->trail[TRAIL_LENGTH - 1] =
        (TrailSegment){.x = g->player.x, .y = g->player.y + PLAYER_HEIGHT / 2 - 1, .active = true};

    for(int i = 0; i < TRAIL_LENGTH; i++) {
        if(g->trail[i].active) {
            g->trail[i].x -= g->speed_multiplier;
        }
    }

    static float generation_progress = 0.0f;
    generation_progress += g->speed_multiplier;

    while(generation_progress >= 1.0f) {
        generate_next_column(g);
        generation_progress -= 1.0f;
    }

    // Фиксированное базовое начисление очков
    float base_increment = 0.1f; // Фиксированное значение, не зависит от скорости

    // Проверяем, не истек ли множитель кликов
    uint32_t current_time = furi_get_tick();
    if(current_time - g->last_click_reset > 800) { // Уменьшаем время до сброса
        g->click_multiplier = 1.0f; // Мгновенный сброс до базового значения
    }

    // Применяем множитель от частоты кликов
    float score_increment = base_increment * g->click_multiplier;

    // Применяем множитель очков от бонусов, если активен
    if(g->bonus_effects.score_multiplier.active) {
        score_increment *= (1 << g->bonus_effects.score_multiplier.power);
    }

    g->score_acc += score_increment;
    g->score = g->score_acc;

    check_collision(g);
    update_bonuses(g);
}

void game_render(Canvas* c, void* ctx) {
    GameState* g = ctx;
    if(furi_mutex_acquire(g->mutex, FuriWaitForever) != FuriStatusOk) return;

    canvas_clear(c);
    uint32_t now = furi_get_tick();

    if(g->screen_state == GameStateMainMenu) {
        // Отрисовка фонового изображения
        canvas_draw_xbm(c, 0, 5, logo_width, logo_height, logo_bits);

        // Обновление состояния мигания стрелки
        uint32_t current_time = furi_get_tick();
        if(current_time - g->arrow_blink_time >= 300) {
            g->arrow_blink_time = current_time;
            g->arrow_visible = !g->arrow_visible;
        }

        // Обновление состояния показа счета только если не в режиме выбора языка
        if(!g->language_selection && current_time - g->menu_text_switch_time >= 2000) {
            g->menu_text_switch_time = current_time;
            g->show_scores = !g->show_scores;
        }

        // Отрисовка пунктов меню
        for(int i = 0; i < DifficultyCount; i++) {
            int menu_y = 35 + i * 11;

            if(i == g->menu_selected_item) {
                if(g->language_selection) {
                    // В режиме выбора языка показываем только текст
                    draw_localized_text(
                        c,
                        SCREEN_WIDTH / 2,
                        menu_y,
                        AlignCenter,
                        AlignCenter,
                        &DIFFICULTY_TEXTS[i],
                        g->current_language);
                } else {
                    // В обычном режиме показываем текст/счет и стрелку
                    if(g->show_scores) {
                        char score_text[32];
                        snprintf(score_text, sizeof(score_text), "%d", g->highscores[i]);
                        draw_russian_text(
                            c, SCREEN_WIDTH / 2, menu_y, AlignCenter, AlignCenter, score_text);

                        if(g->arrow_visible) {
                            int text_width = measure_text_width(score_text);
                            int arrow_x = SCREEN_WIDTH / 2 - text_width / 2 - 12;
                            draw_arrow(c, arrow_x, menu_y);
                        }
                    } else {
                        draw_localized_text(
                            c,
                            SCREEN_WIDTH / 2,
                            menu_y,
                            AlignCenter,
                            AlignCenter,
                            &DIFFICULTY_TEXTS[i],
                            g->current_language);

                        if(g->arrow_visible) {
                            const char* display_text = (g->current_language == LanguageRus) ?
                                                           DIFFICULTY_TEXTS[i].rus :
                                                           DIFFICULTY_TEXTS[i].eng;
                            int text_width = measure_text_width(display_text);
                            int arrow_x = SCREEN_WIDTH / 2 - text_width / 2 - 15;
                            draw_arrow(c, arrow_x, menu_y);
                        }
                    }
                }
            } else {
                // Для неактивных пунктов всегда показываем текст
                draw_localized_text(
                    c,
                    SCREEN_WIDTH / 2,
                    menu_y,
                    AlignCenter,
                    AlignCenter,
                    &DIFFICULTY_TEXTS[i],
                    g->current_language);
            }
        }

        // Отрисовка переключателя языков
        draw_small_text(c, SCREEN_WIDTH - 30, SCREEN_HEIGHT - 2, AlignLeft, AlignBottom, "ru");
        draw_small_text(c, SCREEN_WIDTH - 15, SCREEN_HEIGHT - 2, AlignLeft, AlignBottom, "en");

        // Отрисовка стрелки выбора языка
        if(g->language_selection && g->arrow_visible) {
            int arrow_x = g->current_language == LanguageRus ? SCREEN_WIDTH - 25 :
                                                               SCREEN_WIDTH - 10;
            draw_vertical_arrow(c, arrow_x, SCREEN_HEIGHT - 20);
        }
    } else if(g->screen_state == GameStatePlaying && !g->game_over) {
        if(g->start_delay_active) {
            uint32_t elapsed = now - g->countdown_start;

            // Отображаем отладочную информацию
            char debug_buf[128];
            snprintf(
                debug_buf,
                sizeof(debug_buf),
                "Time: %lu/%d\nPhase: %d",
                elapsed,
                START_DELAY_MS,
                (elapsed < 667)  ? 3 :
                (elapsed < 1334) ? 2 :
                                   1);
            canvas_set_font(c, FontSecondary);
            canvas_draw_str(c, 2, 8, debug_buf);

            // Обновляем и отображаем обратный отсчет
            if(elapsed >= START_DELAY_MS) {
                g->start_delay_active = false;
                g->warmup_active = true;
                g->warmup_start = now;
            } else {
                // Определяем и отображаем текущую картинку (примерно по 667мс на цифру)
                if(elapsed < 667) {
                    // Показываем 3
                    if(g->current_language == LanguageRus) {
                        canvas_draw_xbm(
                            c,
                            (SCREEN_WIDTH - ready3ru_width) / 2,
                            (SCREEN_HEIGHT - ready3ru_height) / 2,
                            ready3ru_width,
                            ready3ru_height,
                            ready3ru_bits);
                    } else {
                        canvas_draw_xbm(
                            c,
                            (SCREEN_WIDTH - ready_3_width) / 2,
                            (SCREEN_HEIGHT - ready_3_height) / 2,
                            ready_3_width,
                            ready_3_height,
                            ready_3_bits);
                    }
                } else if(elapsed < 1334) {
                    // Показываем 2
                    if(g->current_language == LanguageRus) {
                        canvas_draw_xbm(
                            c,
                            (SCREEN_WIDTH - ready2ru_width) / 2,
                            (SCREEN_HEIGHT - ready2ru_height) / 2,
                            ready2ru_width,
                            ready2ru_height,
                            ready2ru_bits);
                    } else {
                        canvas_draw_xbm(
                            c,
                            (SCREEN_WIDTH - ready_2_width) / 2,
                            (SCREEN_HEIGHT - ready_2_height) / 2,
                            ready_2_width,
                            ready_2_height,
                            ready_2_bits);
                    }
                } else {
                    // Показываем 1
                    if(g->current_language == LanguageRus) {
                        canvas_draw_xbm(
                            c,
                            (SCREEN_WIDTH - ready1ru_width) / 2,
                            (SCREEN_HEIGHT - ready1ru_height) / 2,
                            ready1ru_width,
                            ready1ru_height,
                            ready1ru_bits);
                    } else {
                        canvas_draw_xbm(
                            c,
                            (SCREEN_WIDTH - ready_1_width) / 2,
                            (SCREEN_HEIGHT - ready_1_height) / 2,
                            ready_1_width,
                            ready_1_height,
                            ready_1_bits);
                    }
                }
            }
        } else {
            draw_tunnel(c, g);

            // Отрисовываем след и игрока только если не идет анимация паузы или во время мигания
            if(!g->pause_animation_active || g->pause_blink_state) {
                for(int i = 0; i < TRAIL_LENGTH; i++) {
                    if(g->trail[i].active) {
                        canvas_draw_box(c, (int)g->trail[i].x, (int)g->trail[i].y, 2, 3);
                    }
                }
                draw_player(c, g);
            }

            // Если игра на паузе, рисуем значок паузы или стрелку
            if(g->paused) {
                if(g->pause_animation_active) {
                    uint32_t animation_elapsed = now - g->pause_animation_start;
                    if(animation_elapsed >= 2000) { // На последней секунде
                        if(g->pause_blink_state) {
                            draw_big_arrow_right(c, (SCREEN_WIDTH - 16) / 2, SCREEN_HEIGHT / 2);
                        }
                    } else {
                        if(g->pause_blink_state) {
                            draw_pause_icon(c, (SCREEN_WIDTH - 10) / 2, (SCREEN_HEIGHT - 12) / 2);
                        }
                    }
                } else {
                    draw_pause_icon(c, (SCREEN_WIDTH - 10) / 2, (SCREEN_HEIGHT - 12) / 2);
                }
            }

            draw_bonuses(c, g);
        }
    } else if(g->screen_state == GameStateGameOver) {
        // Отрисовка фонового изображения Game Over
        if(g->current_language == LanguageRus) {
            canvas_draw_xbm(c, 0, 5, overru_width, overru_height, overru_bits);
        } else {
            canvas_draw_xbm(c, 0, 5, over_width, over_height, over_bits);
        }

        // Отрисовка текущего результата после "Score:"
        char score_buf[32];
        snprintf(score_buf, sizeof(score_buf), "%d", (int)g->score);
        draw_russian_text(c, 68, 34, AlignLeft, AlignCenter, score_buf);

        // Отрисовка лучшего результата после "Best:"
        char hs_buf[32];
        snprintf(hs_buf, sizeof(hs_buf), "%d", g->highscores[g->selected_difficulty]);
        draw_russian_text(c, 55, 50, AlignLeft, AlignCenter, hs_buf);
    }

    furi_mutex_release(g->mutex);
}

void start_game(GameState* g) {
    if(!g->menu_transition_active) {
        g->menu_transition_active = true;
        g->selected_difficulty = (GameDifficulty)g->menu_selected_item;
        g->screen_state = GameStatePlaying;
        game_reset(g);
    }
}

void return_to_menu(GameState* g) {
    if(!g->menu_transition_active) {
        g->menu_transition_active = true;
        g->screen_state = GameStateMainMenu;
        g->menu_selected_item = g->selected_difficulty;
    }
}

void input_callback(InputEvent* ev, void* ctx) {
    GameState* g = ctx;

    if(furi_mutex_acquire(g->mutex, 0) != FuriStatusOk) {
        return;
    }

    switch(g->screen_state) {
    case GameStateMainMenu:
        if(ev->type == InputTypePress) {
            switch(ev->key) {
            case InputKeyUp:
                if(!g->language_selection) {
                    g->menu_selected_item =
                        (g->menu_selected_item - 1 + DifficultyCount) % DifficultyCount;
                    g->menu_text_switch_time = furi_get_tick();
                    g->show_scores = true;
                }
                break;
            case InputKeyDown:
                if(!g->language_selection) {
                    g->menu_selected_item = (g->menu_selected_item + 1) % DifficultyCount;
                    g->menu_text_switch_time = furi_get_tick();
                    g->show_scores = true;
                } else {
                    // Выход из режима выбора языка при нажатии Down
                    g->language_selection = false;
                }
                break;
            case InputKeyLeft:
                if(g->language_selection) {
                    g->current_language = LanguageRus;
                    save_language(g->current_language); // Сохраняем выбор
                }
                break;
            case InputKeyRight:
                if(g->language_selection) {
                    g->current_language = LanguageEng;
                    save_language(g->current_language); // Сохраняем выбор
                }
                break;
            case InputKeyOk:
                if(g->language_selection) {
                    // Выход из режима выбора языка при нажатии Ok
                    g->language_selection = false;
                } else {
                    g->selected_difficulty = (GameDifficulty)g->menu_selected_item;
                    g->screen_state = GameStatePlaying;
                    game_reset(g);
                }
                break;
            case InputKeyBack:
                if(g->language_selection) {
                    g->language_selection = false;
                } else {
                    g->exit_requested = true;
                }
                break;
            default:
                break;
            }
        } else if(ev->type == InputTypeLong) {
            // Добавляем активацию выбора языка по долгому нажатию Up
            if(ev->key == InputKeyUp) {
                g->language_selection = true;
            }
        }
        break;

    case GameStatePlaying:
        if(!g->game_over) {
            if(ev->key == InputKeyBack && ev->type == InputTypePress) {
                if(!g->start_delay_active) {
                    if(!g->paused) {
                        // Ставим на паузу
                        g->paused = true;
                        // Сохраняем время постановки на паузу для бонусов
                        if(g->bonus_effects.tunnel_expander.active) {
                            g->bonus_effects.tunnel_expander.pause_time = furi_get_tick();
                        }
                    } else if(!g->pause_animation_active) {
                        // Начинаем анимацию снятия с паузы
                        g->pause_animation_active = true;
                        g->pause_animation_start = furi_get_tick();
                        g->pause_blink_time = furi_get_tick();
                        g->pause_blink_state = true;
                        g->pause_countdown = 3;
                    }
                }
            } else if(ev->key == InputKeyOk || ev->key == InputKeyUp) {
                switch(ev->type) {
                case InputTypePress: {
                    g->player.button_pressed = true;
                    // Обновляем множитель очков при нажатии
                    uint32_t current_time = furi_get_tick();

                    // Рассчитываем интервал между нажатиями
                    if(g->last_click_time > 0) {
                        uint32_t interval = current_time - g->last_click_time;

                        // Обновляем множитель на основе интервала
                        if(interval < 300) { // Быстрее 3.33 раз в секунду
                            g->click_multiplier = 10.0f;
                        } else {
                            g->click_multiplier = 1.0f;
                        }
                    }

                    g->last_click_time = current_time;
                    g->last_click_reset = current_time;
                    break;
                }
                case InputTypeRelease:
                    g->player.button_pressed = false;
                    break;
                default:
                    break;
                }
            }
        } else {
            g->screen_state = GameStateGameOver;
        }
        break;

    case GameStateGameOver:
        if(ev->type == InputTypePress) {
            switch(ev->key) {
            case InputKeyOk:
                g->screen_state = GameStatePlaying;
                game_reset(g);
                break;
            case InputKeyBack:
                g->screen_state = GameStateMainMenu;
                g->menu_selected_item = g->selected_difficulty;
                break;
            default:
                break;
            }
        }
        break;
    }

    furi_mutex_release(g->mutex);
}

int32_t wave_app(void* p) {
    UNUSED(p);
    srand(furi_get_tick());

    static GameState g_state = {0};
    GameState* g = &g_state;

    g->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    g->screen_state = GameStateMainMenu;
    g->selected_difficulty = DifficultyEasy;
    g->menu_selected_item = 0;
    g->menu_text_switch_time = furi_get_tick();
    g->show_scores = false;
    g->language_selection = false;
    g->pause_animation_active = false;
    g->pause_blink_state = true;
    g->pause_blink_time = 0;
    g->pause_countdown = 3;

    // Загружаем рекорды и язык при старте
    Storage* storage = furi_record_open(RECORD_STORAGE);
    ensure_directory_exists(storage);
    furi_record_close(RECORD_STORAGE);
    load_all_highscores(g);
    g->current_language = load_language(); // Загружаем сохраненный язык

    game_reset(g);

    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, game_render, g);
    view_port_input_callback_set(vp, input_callback, g);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    uint32_t last_tick = furi_get_tick();
    const uint32_t frame_time = 16; // ~60 FPS

    while(!g->exit_requested) {
        uint32_t current_tick = furi_get_tick();
        if(current_tick - last_tick >= frame_time) {
            last_tick = current_tick;

            if(furi_mutex_acquire(g->mutex, 0) == FuriStatusOk) {
                game_update(g);
                furi_mutex_release(g->mutex);
                view_port_update(vp);
            }
        } else {
            furi_delay_tick(1);
        }
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_mutex_free(g->mutex);
    furi_record_close(RECORD_GUI);
    return 0;
}

// Добавляем функцию отрисовки значка паузы
void draw_pause_icon(Canvas* canvas, int x, int y) {
    // Рисуем две вертикальные полоски
    canvas_draw_box(canvas, x, y, 3, 12);
    canvas_draw_box(canvas, x + 7, y, 3, 12);
}

// Добавляем функцию отрисовки большой стрелки вправо
void draw_big_arrow_right(Canvas* canvas, int x, int y) {
    // Размеры стрелки
    const int body_width = 12; // Немного уменьшаем ширину тела
    const int body_height = 6; // Уменьшаем высоту тела
    const int head_height = 12; // Уменьшаем высоту наконечника
    const int head_width = 8; // Уменьшаем ширину наконечника

    // Центрируем стрелку по Y
    int center_y = y;

    // Рисуем тело стрелки (прямоугольник)
    canvas_draw_box(canvas, x, center_y - body_height / 2, body_width, body_height);

    // Рисуем наконечник стрелки (треугольник)
    // Начальные координаты наконечника
    int head_start_x = x + body_width - 1; // Небольшое перекрытие с телом
    int head_start_y = center_y - head_height / 2;

    // Заполняем наконечник построчно
    for(int i = 0; i < head_height; i++) {
        // Вычисляем длину линии для текущей строки
        float progress = (float)i / (head_height - 1);
        float line_length = head_width * (1.0f - fabsf(2.0f * progress - 1.0f));

        canvas_draw_line(
            canvas,
            head_start_x,
            head_start_y + i,
            head_start_x + (int)line_length,
            head_start_y + i);
    }
}

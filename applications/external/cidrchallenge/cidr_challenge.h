#pragma once

#include "progress.h"
#include "quiz.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#define APP_VERSION "1.0"

#define SPEED_DURATION_MS 60000
#define FEEDBACK_MS       1200
#define TICK_PERIOD_MS    100
#define MAX_ANSWER_TIME_MS 99999

typedef enum {
    SceneMenu,
    SceneDifficulty,
    SceneExamSize,
    SceneQuiz,
    SceneResult,
    SceneStats,
    SceneAbout,
} Scene;

typedef enum {
    ModeTraining,
    ModeSpeed,
    ModeSurvival,
    ModeExam,
} GameMode;

typedef enum {
    MenuTraining,
    MenuSpeed,
    MenuSurvival,
    MenuExam,
    MenuStats,
    MenuSound,
    MenuAbout,
    MenuCount,
} MenuItem;

typedef struct {
    GameMode mode;
    Question question;
    uint8_t selected;
    bool answered;
    bool last_correct;
    bool finished;
    uint32_t last_xp;
    uint32_t question_start;
    uint32_t answer_time;
    uint32_t deadline;
    uint32_t elapsed_ms;

    uint32_t asked;
    uint32_t correct;
    uint32_t streak;
    uint32_t best_streak;
    uint32_t xp;
    uint32_t fastest_ms;
    uint32_t total;
} Session;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    FuriTimer* timer;
    NotificationApp* notifications;

    Stats stats;
    Settings settings;
    Session session;

    Scene scene;
    GameMode pending_mode;
    uint8_t menu_index;
    uint8_t option_index;
    bool stats_cleared;
    bool running;
} CidrApp;

uint16_t exam_size(uint8_t index);
uint32_t speed_time_left_ms(const Session* session);

void cidr_draw_callback(Canvas* canvas, void* context);

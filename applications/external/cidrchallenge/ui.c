#include "cidr_challenge.h"

#include <gui/elements.h>
#include <stdio.h>

#define SCREEN_WIDTH  128
#define HEADER_HEIGHT 11

static const char* const menu_items[MenuCount] = {
    "Training",
    "Speed Challenge",
    "Survival",
    "Exam",
    "Statistics",
    "Sound", // the current state is appended at draw time
    "About",
};

static const char* const exam_titles[] = {
    "10 questions",
    "25 questions",
    "50 questions",
};

static void format_seconds(char* out, size_t out_size, uint32_t ms) {
    uint32_t total = ms / 1000;
    snprintf(out, out_size, "%lu:%02lu", (unsigned long)(total / 60), (unsigned long)(total % 60));
}

static void format_answer_time(char* out, size_t out_size, uint32_t ms) {
    if(ms == 0) {
        snprintf(out, out_size, "-");
    } else {
        snprintf(
            out, out_size, "%lu.%lus", (unsigned long)(ms / 1000), (unsigned long)(ms % 1000 / 100));
    }
}

static void format_xp(char* out, size_t out_size, uint32_t xp) {
    if(xp > 999999) {
        snprintf(out, out_size, "999999+ XP");
    } else {
        snprintf(out, out_size, "%lu XP", (unsigned long)xp);
    }
}

static void draw_header(Canvas* canvas, const char* left, const char* right) {
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, HEADER_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 9, left);
    if(right) canvas_draw_str_aligned(canvas, SCREEN_WIDTH - 3, 9, AlignRight, AlignBottom, right);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_option_list(
    Canvas* canvas,
    uint8_t selected,
    uint8_t count,
    const char* const* titles,
    const char* const* labels) {
    for(uint8_t i = 0; i < count; i++) {
        uint8_t y = 14 + i * 15;

        if(i == selected) {
            canvas_draw_box(canvas, 0, y, SCREEN_WIDTH, 15);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 6, y + 11, titles[i]);

        if(labels) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(
                canvas, SCREEN_WIDTH - 6, y + 11, AlignRight, AlignBottom, labels[i]);
        }
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_menu(Canvas* canvas, CidrApp* app) {
    char level[16];
    snprintf(level, sizeof(level), "Lv %lu", (unsigned long)stats_level(app->stats.xp));
    draw_header(canvas, "CIDR CHALLENGE", level);

    const uint8_t visible = 4;
    uint8_t top = 0;
    if(app->menu_index >= visible) top = app->menu_index - visible + 1;

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < visible; i++) {
        uint8_t index = top + i;
        if(index >= MenuCount) break;

        uint8_t y = HEADER_HEIGHT + 1 + i * 13;
        if(index == app->menu_index) {
            canvas_draw_box(canvas, 0, y, SCREEN_WIDTH - 5, 13);
            canvas_set_color(canvas, ColorWhite);
        }

        if(index == MenuSound) {
            char label[20];
            snprintf(
                label,
                sizeof(label),
                "%s: %s",
                menu_items[index],
                app->settings.sound ? "ON" : "OFF");
            canvas_draw_str(canvas, 5, y + 10, label);
        } else {
            canvas_draw_str(canvas, 5, y + 10, menu_items[index]);
        }
        canvas_set_color(canvas, ColorBlack);
    }

    elements_scrollbar_pos(canvas, SCREEN_WIDTH, HEADER_HEIGHT + 1, 52, app->menu_index, MenuCount);
}

static void draw_difficulty(Canvas* canvas, CidrApp* app) {
    const char* titles[DifficultyCount];
    const char* ranges[DifficultyCount];

    for(uint8_t i = 0; i < DifficultyCount; i++) {
        titles[i] = quiz_difficulty_name((Difficulty)i);
        ranges[i] = quiz_difficulty_range((Difficulty)i);
    }

    draw_header(canvas, "DIFFICULTY", NULL);
    draw_option_list(canvas, app->option_index, DifficultyCount, titles, ranges);
}

static void draw_exam_size(Canvas* canvas, CidrApp* app) {
    draw_header(canvas, "EXAM SETUP", "mixed masks");
    draw_option_list(canvas, app->option_index, 3, exam_titles, NULL);
}

static void draw_quiz_header(Canvas* canvas, CidrApp* app, const char* label) {
    Session* session = &app->session;
    char status[32];

    switch(session->mode) {
    case ModeSpeed: {
        char clock[12];
        format_seconds(clock, sizeof(clock), speed_time_left_ms(session));
        snprintf(status, sizeof(status), "%s  %lu", clock, (unsigned long)session->correct);
        draw_header(canvas, label, status);
        break;
    }
    case ModeSurvival: {
        snprintf(status, sizeof(status), "%lu", (unsigned long)session->streak);
        draw_header(canvas, label, status);

        // Streak pips fill whatever space is left between the two labels
        canvas_set_color(canvas, ColorWhite);
        int32_t x = 3 + canvas_string_width(canvas, label) + 5;
        int32_t limit = SCREEN_WIDTH - 6 - canvas_string_width(canvas, status);
        for(uint32_t i = 0; i < session->streak && x + 3 <= limit; i++, x += 5) {
            canvas_draw_box(canvas, x, 4, 3, 4);
        }
        canvas_set_color(canvas, ColorBlack);
        break;
    }
    case ModeExam:
        snprintf(
            status,
            sizeof(status),
            "%lu/%lu",
            (unsigned long)(session->asked + (session->answered ? 0 : 1)),
            (unsigned long)session->total);
        draw_header(canvas, label, status);
        break;

    default:
        snprintf(
            status,
            sizeof(status),
            "%lu/%lu",
            (unsigned long)session->correct,
            (unsigned long)session->asked);
        draw_header(canvas, label, status);
        break;
    }
}

static void draw_quiz(Canvas* canvas, CidrApp* app) {
    Session* session = &app->session;
    const Question* question = &session->question;

    char feedback[32];
    const char* label = question->prompt;

    if(session->answered) {
        if(session->last_correct) {
            snprintf(feedback, sizeof(feedback), "CORRECT +%lu XP", (unsigned long)session->last_xp);
        } else {
            snprintf(feedback, sizeof(feedback), "WRONG");
        }
        label = feedback;
    }

    draw_quiz_header(canvas, app, label);

    canvas_set_font(canvas, FontPrimary);
    if(canvas_string_width(canvas, question->subject) > SCREEN_WIDTH - 4) {
        canvas_set_font(canvas, FontSecondary);
    }
    canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignBottom, question->subject);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < QUIZ_ANSWER_COUNT; i++) {
        uint8_t y = 28 + i * 9;
        bool highlight = session->answered ? (i == question->correct) : (i == session->selected);

        if(highlight) {
            canvas_draw_box(canvas, 0, y, SCREEN_WIDTH, 9);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 6, y + 7, question->answers[i]);
        canvas_set_color(canvas, ColorBlack);

        if(session->answered && !session->last_correct && i == session->selected) {
            canvas_draw_frame(canvas, 0, y, SCREEN_WIDTH, 9);
        }
    }
}

static void draw_result(Canvas* canvas, CidrApp* app) {
    Session* session = &app->session;
    uint32_t accuracy = session->asked ? session->correct * 100 / session->asked : 0;

    char xp[16];
    snprintf(xp, sizeof(xp), "+%lu XP", (unsigned long)session->xp);
    draw_header(canvas, "RESULT", xp);

    char line[48];
    snprintf(
        line,
        sizeof(line),
        "%lu / %lu",
        (unsigned long)session->correct,
        (unsigned long)session->asked);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, line);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, accuracy_rank(accuracy));

    snprintf(line, sizeof(line), "Accuracy: %lu%%", (unsigned long)accuracy);
    canvas_draw_str(canvas, 5, 44, line);

    snprintf(line, sizeof(line), "Best streak: %lu", (unsigned long)session->best_streak);
    canvas_draw_str(canvas, 5, 52, line);

    char fastest[12];
    format_answer_time(fastest, sizeof(fastest), session->fastest_ms);
    snprintf(line, sizeof(line), "Fastest: %s", fastest);
    canvas_draw_str(canvas, 5, 60, line);
}

static void draw_stats(Canvas* canvas, CidrApp* app) {
    const Stats* stats = &app->stats;

    char xp[16];
    format_xp(xp, sizeof(xp), stats->xp);
    draw_header(canvas, "STATISTICS", xp);

    canvas_set_font(canvas, FontSecondary);

    char line[48];
    snprintf(line, sizeof(line), "Questions: %lu", (unsigned long)stats->questions);
    canvas_draw_str(canvas, 5, 20, line);

    snprintf(line, sizeof(line), "Correct: %lu", (unsigned long)stats->correct);
    canvas_draw_str(canvas, 5, 28, line);

    snprintf(line, sizeof(line), "Accuracy: %lu%%", (unsigned long)stats_accuracy(stats));
    canvas_draw_str(canvas, 5, 36, line);

    snprintf(line, sizeof(line), "Best streak: %lu", (unsigned long)stats->best_streak);
    canvas_draw_str(canvas, 5, 44, line);

    char fastest[12];
    format_answer_time(fastest, sizeof(fastest), stats->fastest_ms);
    snprintf(line, sizeof(line), "Fastest answer: %s", fastest);
    canvas_draw_str(canvas, 5, 52, line);

    if(app->stats_cleared) {
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Statistics cleared");
    } else {
        uint32_t level = stats_level(stats->xp);
        snprintf(
            line, sizeof(line), "Lv %lu - %s", (unsigned long)level, level_title(level));
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, line);
    }
}

static void draw_about(Canvas* canvas) {
    draw_header(canvas, "ABOUT", "v" APP_VERSION);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignBottom, "CIDR Challenge");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 33, AlignCenter, AlignBottom, "Created by Alastor");
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, "github.com/AlastorApps");
    canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignBottom, "/cidrchallengef0");
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "IPv4 subnetting trainer");
}

void cidr_draw_callback(Canvas* canvas, void* context) {
    CidrApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);

    switch(app->scene) {
    case SceneDifficulty:
        draw_difficulty(canvas, app);
        break;
    case SceneExamSize:
        draw_exam_size(canvas, app);
        break;
    case SceneQuiz:
        draw_quiz(canvas, app);
        break;
    case SceneResult:
        draw_result(canvas, app);
        break;
    case SceneStats:
        draw_stats(canvas, app);
        break;
    case SceneAbout:
        draw_about(canvas);
        break;
    default:
        draw_menu(canvas, app);
        break;
    }

    furi_mutex_release(app->mutex);
}

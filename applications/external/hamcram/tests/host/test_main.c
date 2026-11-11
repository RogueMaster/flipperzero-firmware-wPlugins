/**
 * Host-side tests for the parts of Ham Cram that do not need a Flipper:
 * the pool reader, saved progress, session building and the question view's
 * wrapping and scrolling. Built against tests/host/shim instead of the SDK.
 *
 *   host_tests dump <pool.hcp>             print the pool as the app reads it
 *   host_tests test <pool dir> <temp dir>  run the test suite
 *
 * Use tests/run_host_tests.py, which also compares the dump with the JSON.
 */
#include <furi.h>
#include <furi_hal_random.h>

#include "../../core/pool.h"
#include "../../core/progress.h"
#include "../../core/session.h"

/* Pull the view in as source so the tests can look at its model */
#include "../../views/quiz_view.c"

static int checks = 0;
static int failures = 0;

#define CHECK(condition)                                                             \
    do {                                                                             \
        checks++;                                                                    \
        if(!(condition)) {                                                           \
            failures++;                                                              \
            if(failures <= 20)                                                       \
                fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        }                                                                            \
    } while(0)

static const char* const pool_files[] = {"technician.hcp", "general.hcp", "extra.hcp"};
static char path_buffer[512];

static const char* join(const char* dir, const char* name) {
    snprintf(path_buffer, sizeof(path_buffer), "%s/%s", dir, name);
    return path_buffer;
}

/* ------------------------------------------------------------------ dump */

static int dump_pool(const char* path) {
    Pool* pool = pool_open(NULL, path);
    if(!pool) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    const PoolHeader* h = &pool->header;
    printf(
        "POOL\t%s\t%s\t%u\t%u\t%u\t%lu\t%lu\t%u\n",
        h->name,
        h->years,
        h->element,
        h->exam_questions,
        h->pass_correct,
        (unsigned long)h->valid_from,
        (unsigned long)h->valid_to,
        h->num_questions);
    for(uint8_t s = 0; s < h->num_subelements; s++) {
        const PoolSubelement* sub = &pool->subelements[s];
        printf("SUB\t%s\t%s\t%u\t%u\n", sub->id, sub->title, sub->num_groups, sub->num_questions);
    }
    for(uint16_t q = 0; q < h->num_questions; q++) {
        PoolQuestion question;
        if(!pool_read_question(pool, q, &question)) {
            fprintf(stderr, "cannot read question %u\n", q);
            return 1;
        }
        printf(
            "Q\t%s\t%u\t%u\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
            question.id,
            question.correct,
            question.flags,
            pool->groups[question.group].id,
            question.ref,
            question.question,
            question.answers[0],
            question.answers[1],
            question.answers[2],
            question.answers[3]);
    }
    pool_close(pool);
    return 0;
}

/* ------------------------------------------------------------------ pool */

static uint8_t* read_file(const char* path, size_t* size) {
    FILE* file = fopen(path, "rb");
    if(!file) return NULL;
    fseek(file, 0, SEEK_END);
    *size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t* data = calloc(1, *size + 1);
    if(fread(data, 1, *size, file) != *size) *size = 0;
    fclose(file);
    return data;
}

static void write_file(const char* path, const uint8_t* data, size_t size) {
    FILE* file = fopen(path, "wb");
    furi_check(file);
    fwrite(data, 1, size, file);
    fclose(file);
}

/* Damaged files must be rejected or read safely, never crash */
static void test_pool_corruption(const char* pool_dir, const char* temp_dir) {
    size_t size = 0;
    uint8_t* original = read_file(join(pool_dir, pool_files[0]), &size);
    CHECK(original && size > 4096);
    if(!original || size <= 4096) return;

    char temp[512];
    snprintf(temp, sizeof(temp), "%s/corrupt.hcp", temp_dir);
    uint8_t* copy = calloc(1, size);
    size_t tables = sizeof(PoolHeader) + 48 * 10 + 8 * 35 + 8 * 409;
    int rejected = 0;

    for(int round = 0; round < 600; round++) {
        memcpy(copy, original, size);
        size_t length = size;
        if(round < 40) {
            /* Truncate at assorted places, including inside the tables */
            length = (round < 20) ? (size_t)round * 211 : size - (size_t)(round - 19) * 97;
        } else {
            /* Flip bytes in the header and lookup tables */
            int flips = 1 + (int)(furi_hal_random_get() % 3);
            for(int i = 0; i < flips; i++) {
                size_t at = furi_hal_random_get() % tables;
                copy[at] ^= (uint8_t)(1u << (furi_hal_random_get() % 8));
            }
        }
        write_file(temp, copy, length);

        PoolHeader header;
        pool_read_header(NULL, temp, &header);
        Pool* pool = pool_open(NULL, temp);
        if(!pool) {
            rejected++;
            continue;
        }
        for(uint16_t q = 0; q < pool->header.num_questions; q++) {
            PoolQuestion question;
            if(pool_read_question(pool, q, &question)) {
                CHECK(question.correct < POOL_ANSWERS);
                CHECK(strlen(question.question) <= POOL_MAX_RECORD);
            }
        }
        pool_close(pool);
    }
    CHECK(rejected > 100);

    /* Not a pool at all */
    write_file(temp, (const uint8_t*)"hello", 5);
    CHECK(pool_open(NULL, temp) == NULL);
    CHECK(pool_open(NULL, join(temp_dir, "does-not-exist.hcp")) == NULL);
    remove(temp);
    free(copy);
    free(original);
}

static void test_pool_structure(Pool* pool) {
    const PoolHeader* h = &pool->header;
    CHECK(h->exam_questions == h->num_groups);
    CHECK(h->pass_correct == (h->exam_questions * 74 + 99) / 100);
    CHECK(h->valid_from < h->valid_to);

    PoolQuestion question;
    CHECK(!pool_read_question(pool, h->num_questions, &question));
    for(uint16_t q = 0; q < h->num_questions; q++) {
        CHECK(pool_read_question(pool, q, &question));
        const PoolGroup* group = &pool->groups[question.group];
        CHECK(strncmp(question.id, group->id, 3) == 0);
        CHECK(q >= group->first_question && q < group->first_question + group->num_questions);
        if(q > 0) CHECK(pool->index[q].code != pool->index[q - 1].code);
        for(uint8_t a = 0; a < POOL_ANSWERS; a++)
            CHECK(question.answers[a][0] != '\0');
        /* Reading is repeatable and independent of order */
        if(q % 97 == 0) {
            char first[sizeof(question.id)];
            memcpy(first, question.id, sizeof(first));
            PoolQuestion other;
            CHECK(pool_read_question(pool, h->num_questions - 1, &other));
            CHECK(pool_read_question(pool, q, &other));
            CHECK(strcmp(first, other.id) == 0);
        }
    }
}

/* --------------------------------------------------------------- session */

static bool perm_valid(uint8_t perm) {
    uint8_t seen = 0;
    for(uint8_t slot = 0; slot < 4; slot++)
        seen |= 1u << session_perm_get(perm, slot);
    return seen == 0x0F;
}

static void test_exam(Pool* pool) {
    const PoolHeader* h = &pool->header;
    Session* session = session_alloc(h->num_questions);
    uint8_t* picked = calloc(1, h->num_questions);
    uint32_t first_slot[4] = {0};
    bool shuffled = false;

    for(int round = 0; round < 3000; round++) {
        bool skip = (round % 2) == 0;
        CHECK(session_start_exam(session, pool, skip, true));
        CHECK(session->mode == SessionModeExam);
        CHECK(session->count == h->exam_questions);
        CHECK(session->pos == 0 && session->answered == 0 && session->correct == 0);
        for(uint16_t i = 0; i < session->count; i++) {
            const SessionItem* item = &session->items[i];
            const PoolIndex* entry = &pool->index[item->question];
            /* Exactly one question from each group, in group order */
            CHECK(entry->group == i);
            CHECK(item->answer == 0);
            CHECK(perm_valid(item->perm));
            if(skip) CHECK(!(entry->flags & POOL_FLAG_FIGURE));
            if(entry->flags & POOL_FLAG_PIN_LAST) CHECK(session_perm_get(item->perm, 3) == 3);
            if(item->perm != 0xE4) shuffled = true;
            first_slot[session_perm_get(item->perm, 0)]++;
            picked[item->question] = 1;
        }
    }
    CHECK(shuffled);
    /* Every answer position gets used: no letter is a safe guess */
    for(uint8_t a = 0; a < 4; a++)
        CHECK(first_slot[a] > 3000u * h->exam_questions / 10);
    /* Every question can come up */
    uint16_t never = 0;
    for(uint16_t q = 0; q < h->num_questions; q++)
        never += picked[q] ? 0 : 1;
    CHECK(never == 0);

    CHECK(session_start_exam(session, pool, true, false));
    for(uint16_t i = 0; i < session->count; i++)
        CHECK(session->items[i].perm == 0xE4);

    /* Take an exam: answer some right, some wrong, leave some blank */
    CHECK(session_start_exam(session, pool, true, true));
    uint16_t right = 0, wrong = 0;
    for(session->pos = 0; session->pos < session->count; session->pos++) {
        if(session->pos % 5 == 4) continue; /* blank */
        bool correct = session->pos % 3 != 0;
        session_answer(session, session->pos % 4, correct);
        if(correct)
            right++;
        else
            wrong++;
    }
    CHECK(session->answered == right + wrong && session->correct == right);
    /* Changing an answer must not double count */
    session->pos = 0;
    session_answer(session, 1, true);
    session_answer(session, 2, false);
    session_answer(session, 2, false);
    CHECK(session->answered == right + wrong && session->correct == right);
    CHECK(session_item_answer(&session->items[0]) == 2);

    uint16_t total = session->count;
    CHECK(session_start_exam_review(session));
    CHECK(session->mode == SessionModeExamReview && session->pos == 0);
    CHECK(session->count == total - right);
    for(uint16_t i = 0; i < session->count; i++) {
        CHECK(!session_item_correct(&session->items[i]));
        if(i > 0) CHECK(session->items[i].question > session->items[i - 1].question);
    }
    uint16_t before = session->count;
    CHECK(session_start_exam_review(session) && session->count == before);

    free(picked);
    session_free(session);
}

static void test_study_and_review(Pool* pool) {
    const PoolHeader* h = &pool->header;
    Session* session = session_alloc(h->num_questions);
    Progress* progress = progress_alloc(h->num_questions);
    const PoolGroup* group = &pool->groups[h->num_groups / 2];
    ProgressSummary summary;

    CHECK(!session_start_review(session, pool, progress, false, true));

    /* A fresh group is studied in pool order */
    CHECK(session_start_study(
        session, pool, progress, group->first_question, group->num_questions, false, true));
    CHECK(session->mode == SessionModeStudy && session->count == group->num_questions);
    for(uint16_t i = 0; i < session->count; i++) {
        CHECK(session->items[i].question == group->first_question + i);
    }

    /* Master the first three, miss the fourth */
    for(uint16_t i = 0; i < 3; i++)
        progress_record(progress, group->first_question + i, true);
    progress_record(progress, group->first_question + 3, false);
    CHECK(session_start_study(
        session, pool, progress, group->first_question, group->num_questions, false, true));
    CHECK(session->count == group->num_questions - 3);
    CHECK(session->items[0].question == group->first_question + 3);

    progress_summarize(
        progress, pool, group->first_question, group->num_questions, false, &summary);
    CHECK(summary.total == group->num_questions && summary.seen == 4);
    CHECK(summary.mastered == 3 && summary.review == 1);

    /* Once everything is mastered the whole group comes back */
    for(uint16_t i = 0; i < group->num_questions; i++) {
        progress_record(progress, group->first_question + i, true);
    }
    CHECK(session_start_study(
        session, pool, progress, group->first_question, group->num_questions, false, true));
    CHECK(session->count == group->num_questions);

    /* Whole pool, with and without figure questions */
    progress_reset(progress);
    CHECK(session_start_study(session, pool, progress, 0, h->num_questions, false, true));
    CHECK(session->count == h->num_questions);
    progress_summarize(progress, pool, 0, h->num_questions, true, &summary);
    CHECK(summary.total < h->num_questions);
    CHECK(session_start_study(session, pool, progress, 0, h->num_questions, true, true));
    CHECK(session->count == summary.total);
    /* Out-of-range requests are clamped, not trusted */
    CHECK(session_start_study(session, pool, progress, h->num_questions - 2, 500, false, true));
    CHECK(session->count == 2);
    CHECK(!session_start_study(session, pool, progress, h->num_questions, 5, false, true));

    /* Review: most-missed first, and it shrinks as answers come right */
    progress_reset(progress);
    progress_record(progress, 5, false); /* weight 2 */
    progress_record(progress, 9, false);
    progress_record(progress, 9, false); /* weight 4 */
    progress_record(progress, 20, false);
    progress_record(progress, 20, false);
    progress_record(progress, 20, false);
    progress_record(progress, 20, false); /* capped at 7 */
    progress_record(progress, 30, false);
    progress_record(progress, 31, true); /* never missed */
    CHECK(progress_weight(progress, 20) == PROGRESS_WEIGHT_MAX);
    CHECK(session_start_review(session, pool, progress, false, true));
    CHECK(session->mode == SessionModeReview && session->count == 4);
    CHECK(session->items[0].question == 20);
    CHECK(session->items[1].question == 9);
    CHECK(progress_weight(progress, session->items[2].question) == 2);
    CHECK(progress_weight(progress, session->items[3].question) == 2);

    progress_record(progress, 5, true);
    CHECK(session_start_review(session, pool, progress, false, true) && session->count == 4);
    progress_record(progress, 5, true); /* second right answer clears a single miss */
    CHECK(!progress_weight(progress, 5) && progress_mastered(progress, 5));
    CHECK(session_start_review(session, pool, progress, false, true) && session->count == 3);

    int rounds = 0;
    while(session_start_review(session, pool, progress, false, true) && rounds < 20) {
        for(uint16_t i = 0; i < session->count; i++) {
            progress_record(progress, session->items[i].question, true);
        }
        rounds++;
    }
    CHECK(rounds == 7); /* the question missed four times takes longest */
    progress_summarize(progress, pool, 0, h->num_questions, false, &summary);
    CHECK(summary.review == 0 && summary.mastered == 5);

    progress_free(progress);
    session_free(session);
}

/* -------------------------------------------------------------- progress */

static void test_progress_files(Pool* pool, const char* temp_dir) {
    const PoolHeader* h = &pool->header;
    char path[512], temp[520];
    snprintf(path, sizeof(path), "%s/progress-test.sav", temp_dir);
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    remove(path);
    remove(temp);

    Progress* progress = progress_alloc(h->num_questions);
    Progress* loaded = progress_alloc(h->num_questions);
    CHECK(!progress_load(loaded, NULL, path, pool));

    for(uint16_t q = 0; q < h->num_questions; q++) {
        if(q % 3 == 0) progress_record(progress, q, true);
        if(q % 7 == 0) progress_record(progress, q, false);
    }
    progress_record_exam(progress, 30, true);
    progress_record_exam(progress, 20, false);
    CHECK(progress->dirty);
    CHECK(progress_save(progress, NULL, path, pool));
    CHECK(!progress->dirty);

    CHECK(progress_load(loaded, NULL, path, pool));
    CHECK(memcmp(progress->stats, loaded->stats, h->num_questions) == 0);
    CHECK(loaded->exams_taken == 2 && loaded->exams_passed == 1);
    CHECK(loaded->best_score == 30 && loaded->last_score == 20);

    /* Saving again over an existing file works too */
    progress_record(progress, 1, false);
    CHECK(progress_save(progress, NULL, path, pool));
    CHECK(progress_load(loaded, NULL, path, pool));
    CHECK(memcmp(progress->stats, loaded->stats, h->num_questions) == 0);

    /* Interrupted save: old file removed, new one never renamed */
    progress_record(progress, 2, false);
    shim_storage_fail_renames(1);
    CHECK(!progress_save(progress, NULL, path, pool));
    CHECK(progress->dirty);
    CHECK(progress_load(loaded, NULL, path, pool));
    CHECK(memcmp(progress->stats, loaded->stats, h->num_questions) == 0);
    CHECK(progress_save(progress, NULL, path, pool));

    /* A pool edition with questions withdrawn keeps the remaining stats
     * attached to the right questions. Simulate by saving stats under a
     * shuffled, partly unknown set of question codes. */
    size_t size = 0;
    uint8_t* raw = read_file(path, &size);
    CHECK(raw && size == 20 + 3u * h->num_questions);
    if(raw && size == 20 + 3u * h->num_questions) {
        uint16_t n = h->num_questions;
        /* swap first and last entry, and make entry 4 an unknown question */
        uint8_t swap[3];
        memcpy(swap, raw + 20, 3);
        memcpy(raw + 20, raw + 20 + 3 * (n - 1), 3);
        memcpy(raw + 20 + 3 * (n - 1), swap, 3);
        raw[20 + 3 * 4] = 0xFF;
        raw[20 + 3 * 4 + 1] = 0xFF;
        write_file(path, raw, size);
        CHECK(progress_load(loaded, NULL, path, pool));
        for(uint16_t q = 0; q < n; q++) {
            if(q == 4) {
                CHECK(loaded->stats[q] == 0);
            } else {
                CHECK(loaded->stats[q] == progress->stats[q]);
            }
        }

        /* Truncated save: rejected, progress left empty */
        write_file(path, raw, size - 10);
        CHECK(!progress_load(loaded, NULL, path, pool));
        for(uint16_t q = 0; q < n; q++)
            CHECK(loaded->stats[q] == 0);

        /* A save from a different edition of the pool is not reused */
        raw[8] ^= 0x01; /* valid_from */
        write_file(path, raw, size);
        CHECK(!progress_load(loaded, NULL, path, pool));
    }
    free(raw);

    progress_reset(progress);
    CHECK(progress->exams_taken == 0 && progress->dirty);
    for(uint16_t q = 0; q < h->num_questions; q++)
        CHECK(progress->stats[q] == 0);
    progress_record(progress, h->num_questions, true); /* out of range: ignored */

    remove(path);
    remove(temp);
    progress_free(progress);
    progress_free(loaded);
}

/* ------------------------------------------------------------- quiz view */

static struct {
    int count;
    QuizViewEvent event;
    uint8_t slot;
} quiz_events;

static void quiz_event_callback(QuizViewEvent event, uint8_t slot, void* context) {
    UNUSED(context);
    quiz_events.count++;
    quiz_events.event = event;
    quiz_events.slot = slot;
}

/* What is on screen must agree with the model, and fit */
static void quiz_check_frame(QuizView* view, Canvas* canvas, uint8_t* line_seen) {
    QuizViewModel* model = view_get_model(view->view);
    shim_view_draw(view->view, canvas);
    CHECK(model->laid_out);
    CHECK(model->line_count > 0 && model->line_count <= QUIZ_MAX_LINES);
    CHECK(model->scroll <= quiz_view_max_scroll(model));
    CHECK(canvas->scrollbar == (quiz_view_shown_lines(model) > QUIZ_VISIBLE));

    for(size_t i = 0; i < canvas->string_count; i++) {
        const ShimString* string = &canvas->strings[i];
        int32_t width = canvas_string_width(canvas, string->text);
        CHECK(string->x >= 0 && string->x + width <= QUIZ_SCREEN_W);
        if(string->y > QUIZ_HEADER_H) {
            CHECK(string->x + width <= QUIZ_TEXT_W); /* clear of the scrollbar */
            CHECK(string->y <= QUIZ_SCREEN_H);
        }
    }
    for(uint8_t row = 0; row < QUIZ_VISIBLE && model->scroll + row < quiz_view_shown_lines(model);
        row++) {
        line_seen[model->scroll + row] = 1;
    }

    if(!model->reveal && model->selected >= 0) {
        /* The highlighted answer is on screen, completely if it can be */
        uint8_t first = model->block_first[model->selected + 1];
        uint8_t lines = model->block_lines[model->selected + 1];
        if(lines <= QUIZ_VISIBLE) {
            CHECK(first >= model->scroll);
            CHECK(first + lines <= model->scroll + QUIZ_VISIBLE);
        } else {
            CHECK(first < model->scroll + QUIZ_VISIBLE && first + lines > model->scroll);
        }
        CHECK(canvas->box_count >= 2); /* header bar + highlight */
    }
}

/* Wrapping must not lose, duplicate or reorder any text */
static void quiz_check_wrap(QuizView* view, const QuizViewData* data) {
    QuizViewModel* model = view_get_model(view->view);
    char rebuilt[QUIZ_TEXT_SIZE];
    for(uint8_t block = 0; block < QUIZ_BLOCKS; block++) {
        size_t used = 0;
        CHECK(model->block_lines[block] > 0);
        for(uint8_t i = 0; i < model->block_lines[block]; i++) {
            const QuizLine* line = &model->lines[model->block_first[block] + i];
            CHECK(line->block == block);
            CHECK(line->length > 0 && line->length <= QUIZ_LINE_CHARS);
            /* A break at a space drops the space; one after a hyphen drops nothing */
            if(i > 0 && model->text[line->start - 1] == ' ') {
                rebuilt[used++] = ' ';
            } else if(i > 0) {
                CHECK(model->text[line->start - 1] == '-');
            }
            memcpy(rebuilt + used, model->text + line->start, line->length);
            used += line->length;
        }
        rebuilt[used] = '\0';
        if(block == 0) {
            size_t tail = strlen(data->question);
            CHECK(used >= tail && strcmp(rebuilt + used - tail, data->question) == 0);
            if(data->note) CHECK(strncmp(rebuilt, data->note, strlen(data->note)) == 0);
        } else {
            CHECK(strcmp(rebuilt, data->answers[block - 1]) == 0);
        }
    }
}

static void test_quiz_view_pool(Pool* pool, QuizView* view) {
    Canvas canvas;
    uint8_t line_seen[QUIZ_MAX_LINES];
    QuizViewModel* model = view_get_model(view->view);
    uint16_t most_lines = 0;

    for(uint16_t q = 0; q < pool->header.num_questions; q++) {
        PoolQuestion question;
        CHECK(pool_read_question(pool, q, &question));
        char footer[80] = "";
        if(question.ref[0]) snprintf(footer, sizeof(footer), "FCC rule: %.48s", question.ref);
        QuizViewData data = {
            .footer = footer[0] ? footer : NULL,
            .left = "599/599",
            .right = question.id,
            .note = (question.flags & POOL_FLAG_FIGURE) ? "[Figure not shown: see ncvec.org]" :
                                                          NULL,
            .question = question.question,
            .answers =
                {question.answers[0],
                 question.answers[1],
                 question.answers[2],
                 question.answers[3]},
            .chosen = -1,
            .correct = -1,
        };
        quiz_view_set_question(view, &data);

        /* Input before the first draw is harmless */
        shim_view_input(view->view, InputKeyDown, InputTypeShort);
        CHECK(model->selected == -1 && model->scroll == 0);

        memset(line_seen, 0, sizeof(line_seen));
        quiz_check_frame(view, &canvas, line_seen);
        quiz_check_wrap(view, &data);
        if(model->line_count > most_lines) most_lines = model->line_count;

        /* Walk down to the last answer: everything must scroll past */
        int presses = 0;
        int8_t last_selected = -1;
        uint8_t last_scroll = 0;
        for(; presses < 400; presses++) {
            CHECK(shim_view_input(view->view, InputKeyDown, InputTypeShort));
            quiz_check_frame(view, &canvas, line_seen);
            CHECK(model->selected >= last_selected && model->scroll >= last_scroll);
            if(model->selected == last_selected && model->scroll == last_scroll) break;
            last_selected = model->selected;
            last_scroll = model->scroll;
        }
        CHECK(presses < 400);
        CHECK(model->selected == 3);
        CHECK(model->scroll == quiz_view_max_scroll(model));
        for(uint8_t i = 0; i < quiz_view_shown_lines(model); i++)
            CHECK(line_seen[i]);

        /* And back up to the top of the question */
        memset(line_seen, 0, sizeof(line_seen));
        quiz_check_frame(view, &canvas, line_seen);
        for(presses = 0; presses < 400; presses++) {
            shim_view_input(view->view, InputKeyUp, InputTypeRepeat);
            quiz_check_frame(view, &canvas, line_seen);
            CHECK(model->selected <= last_selected && model->scroll <= last_scroll);
            if(model->selected == last_selected && model->scroll == last_scroll) break;
            last_selected = model->selected;
            last_scroll = model->scroll;
        }
        CHECK(model->selected == -1 && model->scroll == 0);
        for(uint8_t i = 0; i < quiz_view_shown_lines(model); i++)
            CHECK(line_seen[i]);

        /* Answer with each slot in turn across the pool, then reveal */
        int8_t target = q % 4;
        quiz_events.count = 0;
        for(presses = 0; presses < 400 && model->selected < target; presses++) {
            /* From the question, OK steps down to the first answer */
            shim_view_input(
                view->view, model->selected < 0 ? InputKeyOk : InputKeyDown, InputTypeShort);
        }
        CHECK(quiz_events.count == 0 && model->selected == target);
        shim_view_input(view->view, InputKeyOk, InputTypePress); /* only Short counts */
        CHECK(quiz_events.count == 0);
        shim_view_input(view->view, InputKeyOk, InputTypeShort);
        CHECK(quiz_events.count == 1 && quiz_events.event == QuizViewEventAnswer);
        CHECK(quiz_events.slot == target);

        int8_t correct = (q + 2) % 4;
        quiz_view_reveal(view, target, correct, "Wrong");
        memset(line_seen, 0, sizeof(line_seen));
        quiz_check_frame(view, &canvas, line_seen);
        CHECK(model->reveal);
        /* The right answer has been scrolled into view */
        uint8_t first = model->block_first[correct + 1];
        CHECK(first >= model->scroll && first < model->scroll + QUIZ_VISIBLE);
        /* While revealed, Up/Down only scroll and OK means "next" */
        for(presses = 0; presses < 200; presses++) {
            shim_view_input(view->view, InputKeyUp, InputTypeShort);
        }
        CHECK(model->scroll == 0);
        memset(line_seen, 0, sizeof(line_seen));
        quiz_check_frame(view, &canvas, line_seen);
        for(presses = 0; presses < 200; presses++) {
            shim_view_input(view->view, InputKeyDown, InputTypeShort);
            quiz_check_frame(view, &canvas, line_seen);
        }
        CHECK(model->scroll == quiz_view_max_scroll(model));
        for(uint8_t i = 0; i < quiz_view_shown_lines(model); i++)
            CHECK(line_seen[i]);
        /* The rule citation is the last thing on screen, and only now */
        CHECK(quiz_view_shown_lines(model) == model->line_count);
        CHECK((model->block_lines[QUIZ_FOOTER] > 0) == (footer[0] != '\0'));
        if(footer[0] && canvas.string_count > 0) {
            const ShimString* last = &canvas.strings[canvas.string_count - 1];
            CHECK(strstr(footer, last->text) != NULL && last->x == 1);
        }
        shim_view_input(view->view, InputKeyOk, InputTypeShort);
        CHECK(quiz_events.count == 2 && quiz_events.event == QuizViewEventNext);
        shim_view_input(view->view, InputKeyLeft, InputTypeShort);
        CHECK(quiz_events.count == 3 && quiz_events.event == QuizViewEventPrev);
        shim_view_input(view->view, InputKeyRight, InputTypeShort);
        CHECK(quiz_events.count == 4 && quiz_events.event == QuizViewEventNext);
        /* Back is left for the app to handle */
        CHECK(!shim_view_input(view->view, InputKeyBack, InputTypeShort));
        CHECK(quiz_events.count == 4);
    }
    printf("    longest question: %u lines on screen\n", most_lines);
}

static void test_quiz_view_extremes(QuizView* view) {
    Canvas canvas;
    uint8_t line_seen[QUIZ_MAX_LINES];
    QuizViewModel* model = view_get_model(view->view);

    /* Text far beyond anything in a real pool is truncated, not overrun */
    char* huge = calloc(1, 6000);
    memset(huge, 'W', 5999);
    for(size_t i = 7; i < 5999; i += 8)
        huge[i] = ' ';
    char* solid = calloc(1, 400);
    memset(solid, 'M', 399); /* no spaces at all */
    QuizViewData data = {
        .left = "123456789012345678901234567890",
        .center = "123456789012345678901234567890",
        .right = "123456789012345678901234567890",
        .note = huge,
        .question = huge,
        .answers = {huge, solid, "", NULL},
        .chosen = 2,
        .correct = 1,
    };
    quiz_view_set_question(view, &data);
    memset(line_seen, 0, sizeof(line_seen));
    shim_view_draw(view->view, &canvas);
    CHECK(model->laid_out && model->line_count <= QUIZ_MAX_LINES);
    CHECK(strlen(model->text) < QUIZ_TEXT_SIZE);
    CHECK(strlen(model->left) < sizeof(model->left));
    for(int presses = 0; presses < 2000; presses++) {
        shim_view_input(view->view, InputKeyDown, InputTypeShort);
        shim_view_draw(view->view, &canvas);
        CHECK(model->scroll <= quiz_view_max_scroll(model));
        CHECK(model->selected >= -1 && model->selected <= 3);
    }
    for(int presses = 0; presses < 2000; presses++) {
        shim_view_input(view->view, InputKeyUp, InputTypeShort);
        shim_view_draw(view->view, &canvas);
        CHECK(model->scroll <= quiz_view_max_scroll(model));
    }
    CHECK(model->selected == -1 && model->scroll == 0);
    quiz_view_reveal(view, 7, 9, NULL); /* nonsense slots are ignored */
    shim_view_draw(view->view, &canvas);
    free(huge);
    free(solid);

    /* Everything missing */
    QuizViewData empty = {.chosen = -1, .correct = -1};
    quiz_view_set_question(view, &empty);
    shim_view_draw(view->view, &canvas);
    for(int presses = 0; presses < 10; presses++) {
        shim_view_input(view->view, InputKeyDown, InputTypeShort);
        shim_view_input(view->view, InputKeyOk, InputTypeShort);
        shim_view_draw(view->view, &canvas);
    }
}

/* ------------------------------------------------------------------ main */

static int run_tests(const char* pool_dir, const char* temp_dir) {
    shim_random_seed(20261110);

    printf("pool corruption handling\n");
    test_pool_corruption(pool_dir, temp_dir);

    QuizView* view = quiz_view_alloc();
    quiz_view_set_callback(view, quiz_event_callback, NULL);

    for(size_t i = 0; i < COUNT_OF(pool_files); i++) {
        PoolHeader header;
        const char* path = join(pool_dir, pool_files[i]);
        CHECK(pool_read_header(NULL, path, &header));
        Pool* pool = pool_open(NULL, path);
        CHECK(pool != NULL);
        if(!pool) continue;
        printf(
            "%s (%u questions, %u groups)\n",
            pool_files[i],
            pool->header.num_questions,
            pool->header.num_groups);
        printf("  structure\n");
        test_pool_structure(pool);
        printf("  practice exams\n");
        test_exam(pool);
        printf("  study and review\n");
        test_study_and_review(pool);
        printf("  saved progress\n");
        test_progress_files(pool, temp_dir);
        printf("  question screen\n");
        test_quiz_view_pool(pool, view);
        pool_close(pool);
    }

    printf("question screen extremes\n");
    test_quiz_view_extremes(view);
    quiz_view_free(view);

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}

int main(int argc, char** argv) {
    if(argc == 3 && strcmp(argv[1], "dump") == 0) return dump_pool(argv[2]);
    if(argc == 4 && strcmp(argv[1], "test") == 0) return run_tests(argv[2], argv[3]);
    fprintf(stderr, "usage: %s dump <pool.hcp> | test <pool dir> <temp dir>\n", argv[0]);
    return 2;
}

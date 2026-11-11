#include "../hamcram.h"

static void progress_cat_date(FuriString* text, uint32_t date) {
    furi_string_cat_printf(
        text,
        "%04lu-%02lu-%02lu",
        (unsigned long)(date / 10000),
        (unsigned long)(date / 100 % 100),
        (unsigned long)(date % 100));
}

void hamcram_scene_progress_on_enter(void* context) {
    HamCram* app = context;
    const Pool* pool = app->pool;
    const PoolHeader* header = &pool->header;
    const Progress* progress = app->progress;
    FuriString* text = app->text;
    ProgressSummary summary;

    progress_summarize(
        progress, pool, 0, header->num_questions, app->settings.skip_figures, &summary);

    furi_string_printf(text, "\e#%s %s\n", header->name, header->years);
    if(hamcram_pool_expired(app, app->pool_entry)) {
        furi_string_cat_str(text, "Expired ");
        progress_cat_date(text, header->valid_to);
        hamcram_cat_wrapped(text, "\nUpdate Ham Cram to get the current pool.\n");
    } else {
        furi_string_cat_str(text, "Valid ");
        progress_cat_date(text, header->valid_from);
        furi_string_cat_str(text, "\nthrough ");
        progress_cat_date(text, header->valid_to);
        furi_string_cat_str(text, "\n");
    }

    furi_string_cat_printf(
        text,
        "Mastered: %u of %u\n(%u%%)  Seen: %u\nTo review: %u\n",
        summary.mastered,
        summary.total,
        summary.total ? (unsigned)summary.mastered * 100 / summary.total : 0,
        summary.seen,
        summary.review);

    if(progress->exams_taken) {
        furi_string_cat_printf(
            text,
            "Exams taken: %u\nPassed: %u\nBest %u/%u  Last %u/%u\n",
            progress->exams_taken,
            progress->exams_passed,
            progress->best_score,
            header->exam_questions,
            progress->last_score,
            header->exam_questions);
    } else {
        furi_string_cat_printf(
            text,
            "No practice exams yet.\nPass mark: %u of %u\n",
            header->pass_correct,
            header->exam_questions);
    }

    furi_string_cat_str(text, "\n\e#By topic\n");
    for(uint8_t s = 0; s < header->num_subelements; s++) {
        const PoolSubelement* sub = &pool->subelements[s];
        progress_summarize(
            progress,
            pool,
            sub->first_question,
            sub->num_questions,
            app->settings.skip_figures,
            &summary);
        snprintf(app->line[0], sizeof(app->line[0]), "%s %s\n", sub->id, sub->title);
        hamcram_cat_wrapped(text, app->line[0]);
        furi_string_cat_printf(text, "  %u of %u mastered\n", summary.mastered, summary.total);
    }

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    view_dispatcher_switch_to_view(app->view_dispatcher, HamCramViewWidget);
}

bool hamcram_scene_progress_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void hamcram_scene_progress_on_exit(void* context) {
    HamCram* app = context;
    widget_reset(app->widget);
}

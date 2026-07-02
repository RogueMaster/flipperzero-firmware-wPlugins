#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum {
    FortuneMessageScene,
    SceneCount,
} FortuneScene;

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Widget* widget;
    uint32_t random_seed;
} App;

const char* fortune_messages[] = {
    "Believe in yourself!",
    "Every day is a new opportunity.",
    "Stay positive and strong.",
    "Success is not final, failure is not fatal.",
    "You are capable of amazing things.",
    "Dream it. Believe it. Achieve it.",
    "Challenges are what make life interesting.",
    "The only limit is your mind.",
    "Take a deep breath and keep going.",
    "Be the reason someone smiles today.",
    "You are stronger than you think.",
    "Small steps every day lead to big results.",
    "The best way out is always through.",
    "Don’t wait for opportunity. Create it.",
    "Believe you can and you’re halfway there.",
    "Your vibe attracts your tribe.",
    "Keep pushing forward no matter what.",
    "Every accomplishment starts with the decision to try.",
    "The wise warrior avoids the battle.",
    "To win without fighting is the highest victory.",
    "Victory belongs to the most persevering.",
    "Dream bigger. Do bigger.",
    "Don’t stop when you’re tired. Stop when you’re done.",
    "Dream it. Wish it. Do it.",
    "Your next big win is around the corner.",
    "Mistakes are proof you’re trying.",
    "Fortune favors the bold move.",
    "A little progress each day adds up.",
    "Stay curious; your mind is your compass.",
    "Kindness always circles back.",
    "Hard work beats talent when talent sleeps.",
    "Trust yourself—your instincts know more than you think.",
    "A clear mind reveals hidden paths.",
    "Great things never came from comfort zones.",
    "Bravery is a decision, not a feeling.",
    "Turn setbacks into comebacks.",
    "Opportunity dances with those already on the floor.",
    "Strength grows when you push past limits.",
    "Your potential is endless—tap into it.",
    "The best view comes after the hardest climb.",
    "Action conquers fear every time.",
    "Stay humble, hustle hard.",
    "Be so good they can’t ignore you.",
    "Every storm runs out of rain—keep moving.",
    "Your attitude determines your direction.",
    "Clarity comes from engagement, not thought.",
    "Success is built on the backbone of consistency.",
    "When one door closes, build a new one.",
    "Persist until something happens.",
    "Your ambition is your superpower.",
    "Doubt kills more dreams than failure ever will.",
    "If not now, when? If not you, who?",
    "Every midnight brings a new dawn.",
    "Win hearts, not just battles.",
    "You grow by lifting others up.",
    "Dreams demand hustle, not hope.",
    "Challenge yourself every single day.",
    "Small sparks can start big fires.",
    "Vision without action is daydreaming.",
    "Your grit defines your greatness.",
    "Be the energy you want to attract.",
    "Success starts with self-discipline.",
    "Live with intention, not by default.",
    "Effort is the currency of achievement.",
    "Progress is better than perfection.",
    "Focus beats talent when talent drifts.",
    "The grind never lies.",
    "Be relentless in your pursuit.",
    "Leadership starts with self-mastery.",
    "Create value before seeking value.",
    "Be the author of your story.",
    "Your legacy is written by today’s choices.",
    "Embrace discomfort to grow stronger.",
    "Obstacles are detours to new paths.",
    "Consistency compounds like interest.",
    "You are the architect of your future.",
    "Today’s efforts build tomorrow’s success.",
    "Grit outlasts genius when genius gloats.",
    "Ideas need action to have impact.",
    "Trust the timing of your life.",
    "Believe it before you see it.",
    "You’re one decision away from a new life.",
    "Mastery takes small, daily improvements.",
    "Your work ethic sets you apart.",
    "Stay patient. Results take time.",
    "Act with purpose and precision.",
    "Your reputation is your resume.",
    "Be the hard worker in the room.",
    "Lead by example, not by mandate.",
    "Take risks; regret nothing.",
    "Failures are lessons in disguise.",
    "Success is a marathon, not a sprint.",
    "Patience is power in disguise.",
    "Aim high. Missing high beats hitting low.",
    "Stay disciplined, not motivated.",
    "Your focus determines your reality.",
    "Hustle quietly, let success shout.",
    "Adapt or get left behind.",
    "Your network is your net worth.",
    "Solve problems; money follows.",
    "Execute while others debate.",
    "Embrace the process, not just the outcome.",
    "Discomfort is the price of progress.",
    "Work until your idols become rivals.",
    "Results speak louder than plans.",
    "Take ownership of every outcome.",
    "Fortune flows to those who flow.",
    "Be the constant in chaos.",
    "Innovation starts with curiosity.",
    "The journey shapes the destination.",
    "Gratitude fuels continuous growth.",
    "Lead with integrity, not ego.",
    "Quality over quantity, always.",
    "Your story is your currency.",
    "Dare to be different.",
    "Execute like it’s do or die.",
    "Ambition is contagious—spread it.",
    "Stop wishing, start doing.",
    "Attitude is a little thing that makes a big difference.",
    "The harder you work, the luckier you get.",
    "Feedback is a gift—unwrap it.",
    "Nobody ever drowned in sweat.",
    "Comfort kills creativity.",
    "The best investment is self-improvement.",
    "Be faster than yesterday.",
    "Outwork your doubts.",
    "Success is found outside your comfort zone.",
    "Your initiative is your edge.",
    "Elevate your standards, elevate your life.",
    "Take flights, not excuses.",
    "Energy flows where attention goes.",
    "Build habits that build you.",
    "Your discipline is your destiny.",
    "Lead yourself before leading others.",
    "You don’t find willpower, you create it.",
    "Perseverance outlasts resistance.",
    "Be bold in your pursuits.",
    "Results reward action.",
    "Your hustle defines your outcome.",
    "Grind while they rest.",
    "Be uncommon among the common.",
    "Let your ambition roar.",
    "Focus on solutions, not problems.",
    "Your grit becomes your greatness.",
    "Success is a series of small wins.",
    "Stay curious, stay foolish.",
    "Create your own luck.",
    "Be relentless, not ruthless.",
    "The only limit is effort.",
    "Walk through walls, ignore limits.",
    "Success demands sacrifice.",
    "Win internal battles first.",
    "Aim for progress, not perfection.",
    "Do the work others won’t.",
    "Turn intention into action.",
    "Be unstoppable in your pursuit.",
    "Your consistency builds empires.",
    "Go all in or get out.",
    "Effort never betrays you.",
    "From action comes clarity.",
    "Break rules, not promises.",
    "Discipline beats motivation every time.",
    "Your perseverance defines your path.",
    "Push harder when it hurts.",
    "Be the hardest worker you know.",
    "Action cures fear.",
    "Obsession breeds mastery.",
    "Show up, grind out, repeat.",
    "Greatness is a daily commitment.",
    "Choose growth over comfort.",
    "Struggle builds character.",
    "The winner stays in the game.",
    "Be your own hero.",
    "Execute relentlessly.",
    "Your results validate your methods.",
    "Build before you buy.",
    "Commit first, figure it out later.",
    "Be so hungry you scare yourself.",
    "The grind never stops.",
    "Chase progress, not applause.",
};
const uint8_t fortune_count = sizeof(fortune_messages) / sizeof(fortune_messages[0]);

const char* get_random_fortune(App* app) {
    app->random_seed = (app->random_seed + 1) % fortune_count;
    return fortune_messages[app->random_seed];
}

void add_word_wrapped_text(Widget* widget, const char* text, int max_chars_per_line) {
    char line_buffer[max_chars_per_line + 1];
    int line_length = 0;
    int y_offset = 15;

    while(*text) {
        const char* word_start = text;
        int word_length = 0;

        while(*text && *text != ' ' && *text != '\n') {
            word_length++;
            text++;
        }

        if(line_length + word_length > max_chars_per_line) {
            line_buffer[line_length] = '\0';
            widget_add_string_multiline_element(
                widget, 10, y_offset, AlignLeft, AlignTop, FontPrimary, line_buffer);
            y_offset += 15;
            line_length = 0;
        }

        if(line_length > 0) {
            line_buffer[line_length++] = ' ';
        }
        strncpy(&line_buffer[line_length], word_start, word_length);
        line_length += word_length;

        while(*text == ' ')
            text++;
    }

    if(line_length > 0) {
        line_buffer[line_length] = '\0';
        widget_add_string_multiline_element(
            widget, 10, y_offset, AlignLeft, AlignTop, FontPrimary, line_buffer);
    }
}

void fortune_message_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    const char* message = get_random_fortune(app);
    add_word_wrapped_text(app->widget, message, 20);

    view_dispatcher_switch_to_view(app->view_dispatcher, 0);
}

bool fortune_message_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void fortune_message_scene_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

void (*const scene_on_enter_handlers[])(void*) = {
    fortune_message_scene_on_enter,
};

bool (*const scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    fortune_message_scene_on_event,
};

void (*const scene_on_exit_handlers[])(void*) = {
    fortune_message_scene_on_exit,
};

static const SceneManagerHandlers scene_manager_handlers = {
    .on_enter_handlers = scene_on_enter_handlers,
    .on_event_handlers = scene_on_event_handlers,
    .on_exit_handlers = scene_on_exit_handlers,
    .scene_num = SceneCount,
};

bool fortune_back_event_callback(void* context) {
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static App* app_alloc() {
    App* app = malloc(sizeof(App));
    app->scene_manager = scene_manager_alloc(&scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, fortune_back_event_callback);

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, 0, widget_get_view(app->widget));

    app->random_seed = furi_get_tick();
    return app;
}

static void app_free(App* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->view_dispatcher, 0);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    widget_free(app->widget);
    free(app);
}

int32_t fortune_cookie_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, FortuneMessageScene);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}

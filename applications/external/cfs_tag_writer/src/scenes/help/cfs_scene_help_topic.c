#include "src/core/cfs_app_i.h"

// Detail page for one Help topic. The topic index is passed in via this scene's
// scene state (set by cfs_scene_help). Order matches HelpTopic in cfs_scene_help.c.
typedef struct {
    const char* title;
    const char* body;
} HelpPage;

static const HelpPage help_pages[] = {
    {"Reading tags",
     "Place a spool tag flat\n"
     "on the Flipper's back,\n"
     "then pick Read Tag.\n"
     "\n"
     "The result shows brand,\n"
     "material, color, weight\n"
     "and length, serial, and\n"
     "the tag UID.\n"
     "\n"
     "A read plays a sound.\n"
     "From there you can Save\n"
     "the spool, Write it\n"
     "(Clone or Edit), or open\n"
     "Raw for the decoded\n"
     "payload (ASCII + hex)."},
    {"Writing tags",
     "Pick Write Tag to build\n"
     "a spool from scratch.\n"
     "\n"
     "Steps: Brand, Material,\n"
     "Color, then Weight. The\n"
     "weight you choose sets\n"
     "the encoded filament\n"
     "length automatically.\n"
     "\n"
     "On Confirm, review the\n"
     "fields and press Write.\n"
     "Hold the tag on the back\n"
     "until it finishes and\n"
     "verifies."},
    {"Clone & Edit",
     "From a read result or a\n"
     "saved spool, choose\n"
     "Write for two options:\n"
     "\n"
     "Clone writes the fields\n"
     "exactly as they are.\n"
     "\n"
     "Edit & Write opens the\n"
     "wizard pre-filled so you\n"
     "can change a field\n"
     "before writing.\n"
     "\n"
     "Both keep the source\n"
     "serial unless Settings\n"
     "say otherwise."},
    {"Dual tag (x2)",
     "A CFS spool has two\n"
     "tags with identical data\n"
     "(front and back).\n"
     "\n"
     "Choose x2 on the Confirm\n"
     "screen to write both:\n"
     "\n"
     "1. Write tag 1 and hold\n"
     "   it until done.\n"
     "2. Remove tag 1, place\n"
     "   tag 2.\n"
     "3. Press OK to write\n"
     "   the second tag."},
    {"Saved Tags",
     "Save stores a spool as a\n"
     "Flipper .nfc file. The\n"
     "name is pre-filled from\n"
     "brand, material and\n"
     "color; edit it as you\n"
     "like.\n"
     "\n"
     "Saved Tags opens a file\n"
     "browser - navigate to\n"
     "any .nfc (incl. stock\n"
     "NFC app files). Each\n"
     "offers View, Write\n"
     "(clone), Edit & Write,\n"
     "and Delete."},
    {"Erasing",
     "Pick Erase Tag to wipe a\n"
     "tag back to blank\n"
     "(factory keys, empty\n"
     "data).\n"
     "\n"
     "Confirm, then hold the\n"
     "tag on the back until it\n"
     "completes.\n"
     "\n"
     "Use this to reset a tag\n"
     "before writing fresh\n"
     "data."},
    {"Settings",
     "Default brand: used when\n"
     "starting a from-scratch\n"
     "write.\n"
     "\n"
     "Serial mode: how the\n"
     "serial is chosen - Keep\n"
     "(source on clone),\n"
     "Random, Sequential, or\n"
     "Prompt.\n"
     "\n"
     "Weights: edit the length\n"
     "each weight maps to.\n"
     "\n"
     "Self-Test: runs a crypto\n"
     "and data check."},
    {"Safety & limits",
     "Only clone or write tags\n"
     "for spools you own. This\n"
     "app is for managing your\n"
     "own filament.\n"
     "\n"
     "Brand shown on read\n"
     "reflects the tag's data;\n"
     "generic spools often\n"
     "carry cloned Creality\n"
     "tags.\n"
     "\n"
     "Known limit: the batch\n"
     "field is fixed."},
};

void cfs_scene_help_topic_on_enter(void* context) {
    CfsApp* app = context;
    uint32_t topic = scene_manager_get_scene_state(app->scene_manager, CfsSceneHelpTopic);
    if(topic >= COUNT_OF(help_pages)) topic = 0;
    const HelpPage* page = &help_pages[topic];

    Widget* widget = app->widget;
    widget_reset(widget);
    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, page->title);
    widget_add_text_scroll_element(widget, 0, 16, 128, 48, page->body);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
}

bool cfs_scene_help_topic_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cfs_scene_help_topic_on_exit(void* context) {
    CfsApp* app = context;
    widget_reset(app->widget);
}

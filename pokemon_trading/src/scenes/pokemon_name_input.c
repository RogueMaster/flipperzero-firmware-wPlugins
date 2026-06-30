#include <ctype.h>
#include <furi.h>
#include <gui/modules/text_input.h>
#include <gui/view_dispatcher.h>
#include <stdlib.h>

#include <src/include/pokemon_app.h>
#include <src/include/pokemon_data.h>
#include <src/include/pokemon_char_encode.h>
#include <src/include/pokemon_attribute.h>

#include <src/scenes/include/pokemon_scene.h>

static char name_buf[LEN_NAME_BUF];

/* NOTE:
 * It would be nice if we could cleanly default to the pokemon's name as their
 * name. The issue is that if you enter a blank line to text input, it does
 * call this function, but returning true does nothing. However, I've found that
 * if you check for the first char of the buffer being \0, you can then set the
 * buffer and then return true. This has the effect of staying in the text_input
 * screen, but, prepopulating the text entry with the buffer AND staying on the
 * save button.
 */
static bool select_name_input_validator(const char* text, FuriString* error, void* context) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;
    uint32_t state =
        scene_manager_get_scene_state(pokemon_fap->scene_manager, PokemonSceneNickname);
    unsigned int i;

    /* A blank field for the pokemon nickname means revert to default name */
    if(text[0] == '\0' && state == PokemonSceneNickname) {
        /* Get the pokemon's name and populate our buffer with it */
        /* TODO: Nidoran M/F are still a problem with this. */
        pokemon_default_nickname_set(name_buf, pokemon_fap->pdata, sizeof(name_buf));
        return true;
    }

    /* Check to ensure no digits in OT name/nickname */
    for(i = 0; i < strlen(text); i++) {
        if(isdigit((unsigned int)text[i])) {
            furi_string_printf(error, "Name cannot\ncontain\nnumbers!");
            return false;
        }
    }

    /* Check for Unown setting is a character */
    if(state == PokemonSceneUnownForm) {
        if(!isalpha((int)text[0])) {
            furi_string_printf(error, "Form must\nbe a single\nletter!");
            return false;
        }
    }

    switch(state) {
    case PokemonSceneNickname:
        pokemon_name_set(pokemon_fap->pdata, STAT_NICKNAME, (char*)text);
        break;
    case PokemonSceneOTName:
        pokemon_name_set(pokemon_fap->pdata, STAT_OT_NAME, (char*)text);
        break;
    case PokemonSceneUnownForm:
        unown_form_set(pokemon_fap->pdata, text[0]);
        break;
    default:
        furi_crash("Invalid scene");
        break;
    }

    return true;
}

static void select_name_input_callback(void* context) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;

    view_dispatcher_send_custom_event(pokemon_fap->view_dispatcher, PokemonSceneBack);
}

void pokemon_scene_select_name_on_enter(void* context) {
    PokemonFap* pokemon_fap = (PokemonFap*)context;
    uint32_t state =
        scene_manager_get_scene_state(pokemon_fap->scene_manager, PokemonSceneNickname);
    char* header;
    int len;
    DataStat stat;

    switch(state) {
    case PokemonSceneNickname:
        header = "Nickname (none for default)";
        len = LEN_NICKNAME;
        stat = STAT_NICKNAME;
        break;
    case PokemonSceneOTName:
        header = "Enter OT Name";
        len = LEN_OT_NAME;
        stat = STAT_OT_NAME;
        break;
    case PokemonSceneUnownForm:
        header = "Enter Unown Letter Form";
        len = 2;
        stat = STAT_OT_NAME;
        break;
    default:
        furi_crash("Name: invalid state");
        break;
    }

    if(state == PokemonSceneUnownForm) {
        /* Put the current letter in the buffer */
        name_buf[0] = unown_form_get(pokemon_fap->pdata);
        name_buf[1] = '\0';
    } else {
        pokemon_name_get(pokemon_fap->pdata, stat, name_buf, len);
    }

    text_input_reset(pokemon_fap->text_input);
    text_input_set_validator(pokemon_fap->text_input, select_name_input_validator, pokemon_fap);
    text_input_set_result_callback(
        pokemon_fap->text_input, select_name_input_callback, pokemon_fap, name_buf, len, true);
    text_input_set_header_text(pokemon_fap->text_input, header);

    view_dispatcher_switch_to_view(pokemon_fap->view_dispatcher, AppViewTextInput);
}

bool pokemon_scene_select_name_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    PokemonFap* pokemon_fap = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event & PokemonSceneBack) {
        scene_manager_previous_scene(pokemon_fap->scene_manager);
        consumed = true;
    }

    return consumed;
}

void pokemon_scene_select_name_on_exit(void* context) {
    UNUSED(context);
}

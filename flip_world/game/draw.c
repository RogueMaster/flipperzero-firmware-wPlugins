#include <game/draw.h>

// Global variables to store camera position
int draw_camera_x = 0;
int draw_camera_y = 0;

// Draw the user stats (health, xp, and level)
void draw_user_stats(Canvas *canvas, Vector pos, GameManager *manager)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    PlayerContext *player = entity_context_get(game_context->player);

    // first draw a black rectangle to make the text more readable
    canvas_invert_color(canvas);
    canvas_draw_box(canvas, pos.x - 1, pos.y - 7, 34, 21);
    canvas_invert_color(canvas);

    char health[32];
    char xp[32];
    char level[32];

    snprintf(health, sizeof(health), "HP : %ld", player->health);
    snprintf(level, sizeof(level), "LVL: %ld", player->level);

    if (player->xp < 10000)
        snprintf(xp, sizeof(xp), "XP : %ld", player->xp);
    else
        snprintf(xp, sizeof(xp), "XP : %ldK", player->xp / 1000);

    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, pos.x, pos.y, health);
    canvas_draw_str(canvas, pos.x, pos.y + 7, xp);
    canvas_draw_str(canvas, pos.x, pos.y + 14, level);
}

void draw_username(Canvas *canvas, Vector pos, char *username)
{
    // first draw a black rectangle to make the text more readable
    // draw box around the username
    canvas_invert_color(canvas);
    canvas_draw_box(canvas, pos.x - draw_camera_x - (strlen(username) * 2) - 1, pos.y - draw_camera_y - 14, strlen(username) * 4 + 1, 8);
    canvas_invert_color(canvas);

    // draw username over player's head
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, pos.x - draw_camera_x - (strlen(username) * 2), pos.y - draw_camera_y - 7, username);
}

static void draw_menu(GameManager *manager, Canvas *canvas)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    PlayerContext *player_context = entity_context_get(game_context->player);

    // draw background rectangle
    canvas_draw_icon(
        canvas,
        0,
        0,
        &I_icon_menu_128x64px);

    if (game_context->game_mode == GAME_MODE_STORY && game_context->story_step < STORY_TUTORIAL_STEPS)
    {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 45, 15, "Tutorial");
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str(canvas, 24, 35, "Press BACK to exit");
    }
    else
    {
        // draw menu options
        switch (game_context->menu_screen)
        {
        case GAME_MENU_INFO:
            // draw info
            // first option is highlighted
            char health[32];
            char xp[32];
            char level[32];
            char strength[32];

            snprintf(level, sizeof(level), "Level   : %ld", player_context->level);
            snprintf(health, sizeof(health), "Health  : %ld", player_context->health);
            snprintf(xp, sizeof(xp), "XP      : %ld", player_context->xp);
            snprintf(strength, sizeof(strength), "Strength: %ld", player_context->strength);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 7, 16, player_context->username);
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 7, 30, level);
            canvas_draw_str(canvas, 7, 37, health);
            canvas_draw_str(canvas, 7, 44, xp);
            canvas_draw_str(canvas, 7, 51, strength);

            // draw a box around the selected option
            canvas_draw_frame(canvas, 80, 18, 36, 30);
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 86, 30, "Info");
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 86, 42, "More");
            break;
        case GAME_MENU_MORE:
            // draw settings
            switch (game_context->menu_selection)
            {
            case 0:
                // first option is highlighted
                break;
            case 1:
                // second option is highlighted
                break;
            default:
                break;
            }

            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 7, 16, VERSION_TAG);
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str_multi(canvas, 7, 25, "Developed by\nJBlanked and Derek \nJamison. Graphics\nfrom Pr3!\n\nwww.github.com/jblanked");

            // draw a box around the selected option
            canvas_draw_frame(canvas, 80, 18, 36, 30);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 86, 30, "Info");
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 86, 42, "More");
            break;
        case GAME_MENU_NPC:
            // draw NPC dialog
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 7, 16, game_context->message);
            break;
        default:
            break;
        }
    }
}

void draw_background_render(Canvas *canvas, GameManager *manager)
{
    if (!canvas || !manager)
        return;

    GameContext *game_context = game_manager_game_context_get(manager);
    PlayerContext *player_context = entity_context_get(game_context->player);
    if (!game_context->is_menu_open)
    {
        // get player position
        Vector posi = entity_pos_get(game_context->player);

        // draw username over player's head
        draw_username(canvas, posi, player_context->username);

        if (game_context->is_switching_level)
            // draw switch world icon
            canvas_draw_icon(canvas, 0, 0, &I_icon_world_change_128x64px);
        else
            // Draw user stats
            draw_user_stats(canvas, (Vector){0, 50}, manager);
    }
    else
    {
        // draw menu
        draw_menu(manager, canvas);
    }
};

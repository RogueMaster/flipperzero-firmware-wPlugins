#include "ui_camera.h"

static uint8_t camera_clamp_x(int16_t x) {
    int16_t max_x = FR_MAP_W - APP_VIEW_W;
    if(x < 0) return 0;
    if(x > max_x) return (uint8_t)max_x;
    return (uint8_t)x;
}

static uint8_t camera_clamp_y(int16_t y) {
    int16_t max_y = FR_MAP_H - APP_VIEW_H;
    if(y < 0) return 0;
    if(y > max_y) return (uint8_t)max_y;
    return (uint8_t)y;
}

void camera_center_on_player(AppContext* app) {
    const FrGame* game = app->game;
    int16_t cam_x = (int16_t)game->player.x - APP_VIEW_W / 2;
    int16_t cam_y = (int16_t)game->player.y - APP_VIEW_H / 2;
    app->camera_x = camera_clamp_x(cam_x);
    app->camera_y = camera_clamp_y(cam_y);
    app->camera_valid = true;
}

void camera_update(AppContext* app) {
    const FrGame* game = app->game;
    if(!app->camera_valid) camera_center_on_player(app);

    int16_t cam_x = app->camera_x;
    int16_t cam_y = app->camera_y;
    const uint8_t margin_x = 5;
    const uint8_t margin_y = 2;

    if(game->player.x < cam_x + margin_x) {
        cam_x = (int16_t)game->player.x - margin_x;
    } else if(game->player.x >= cam_x + APP_VIEW_W - margin_x) {
        cam_x = (int16_t)game->player.x - (APP_VIEW_W - margin_x - 1);
    }

    if(game->player.y < cam_y + margin_y) {
        cam_y = (int16_t)game->player.y - margin_y;
    } else if(game->player.y >= cam_y + APP_VIEW_H - margin_y) {
        cam_y = (int16_t)game->player.y - (APP_VIEW_H - margin_y - 1);
    }

    app->camera_x = camera_clamp_x(cam_x);
    app->camera_y = camera_clamp_y(cam_y);
}

void clamp_cursor_to_view(AppContext* app) {
    camera_update(app);
    uint8_t cam_x = app->camera_x;
    uint8_t cam_y = app->camera_y;
    if(app->cursor_x < cam_x) app->cursor_x = cam_x;
    if(app->cursor_y < cam_y) app->cursor_y = cam_y;
    if(app->cursor_x >= cam_x + APP_VIEW_W) app->cursor_x = (uint8_t)(cam_x + APP_VIEW_W - 1);
    if(app->cursor_y >= cam_y + APP_VIEW_H) app->cursor_y = (uint8_t)(cam_y + APP_VIEW_H - 1);
}

void cursor_move(AppContext* app, int8_t dx, int8_t dy) {
    int16_t nx = (int16_t)app->cursor_x + dx;
    int16_t ny = (int16_t)app->cursor_y + dy;
    if(nx < 0) nx = 0;
    if(ny < 0) ny = 0;
    if(nx >= FR_MAP_W) nx = FR_MAP_W - 1;
    if(ny >= FR_MAP_H) ny = FR_MAP_H - 1;
    app->cursor_x = (uint8_t)nx;
    app->cursor_y = (uint8_t)ny;
    clamp_cursor_to_view(app);
}

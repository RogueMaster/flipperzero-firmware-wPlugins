#pragma once

#include <furi.h>
#include "../util/list.h"
#include "main_screen.h"
#include "intro_animation.h"
#include "play_screen.h"
#include "solve_screen.h"
#include "result_screen.h"
#include "falling_card.h"

typedef struct {
    void (*start)(void* data);
    void (*render)(void* data);

    void (*update)(void* data);

    void (*input)(void* data, InputKey key, InputType type);
} GameLogic;

GameLogic main_screen = (GameLogic){
    .start = start_main_screen,
    .render = render_main_screen,
    .update = update_main_screen,
    .input = input_main_screen};

GameLogic intro_screen = (GameLogic){
    .start = start_intro_screen,
    .render = render_intro_screen,
    .update = update_intro_screen,
    .input = input_intro_screen};

GameLogic play_screen = (GameLogic){
    .start = start_play_screen,
    .render = render_play_screen,
    .update = update_play_screen,
    .input = input_play_screen};

GameLogic solve_screen = (GameLogic){
    .start = start_solve_screen,
    .render = render_solve_screen,
    .update = update_solve_screen,
    .input = input_solve_screen};

GameLogic falling_screen = (GameLogic){
    .start = start_falling_screen,
    .render = render_falling_screen,
    .update = update_falling_screen,
    .input = input_falling_screen};

GameLogic result_screen = (GameLogic){
    .start = start_result_screen,
    .render = render_result_screen,
    .update = update_result_screen,
    .input = input_result_screen};

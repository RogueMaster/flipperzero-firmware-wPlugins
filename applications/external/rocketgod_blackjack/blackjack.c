#include "blackjack.h"
#include "card_graphics.h"

void deal_card(Hand* hand) {
    if(hand->card_count >= 10) return;
    Card card;
    card.rank = (rand() % 13) + 1;
    card.suit = rand() % 4;
    hand->cards[hand->card_count] = card;
    hand->card_count++;
    hand->total = 0;
    int aces = 0;
    for(int i = 0; i < hand->card_count; i++) {
        int value = hand->cards[i].rank;
        if(value > 10) value = 10;
        if(value == 1) aces++;
        hand->total += value;
    }
    while(hand->total <= 11 && aces > 0) {
        hand->total += 10;
        aces--;
    }
}

void input_callback(InputEvent* input_event, void* ctx) {
    BlackjackApp* app = ctx;
    if(input_event->type == InputTypeShort) {
        if(input_event->key == InputKeyBack) {
            app->exit = true;
        } else if(app->state == StateMenu) {
            if(input_event->key == InputKeyOk) {
                app->state = StateGame;
                srand(furi_get_tick());
                app->player.card_count = 0;
                app->dealer.card_count = 0;
                deal_card(&app->player);
                deal_card(&app->player);
                deal_card(&app->dealer);
                deal_card(&app->dealer);
                app->game_result = 0;

                if(app->dealer.total == 21 && app->player.total == 21) {
                    app->game_result = 2;
                } else if(app->dealer.total == 21) {
                    app->game_result = -1;
                } else if(app->player.total == 21) {
                    app->game_result = 1;
                }
            }
        } else if(app->state == StateGame) {
            if(app->game_result == 0) {
                if(input_event->key == InputKeyUp) {
                    deal_card(&app->player);
                    if(app->player.total > 21) {
                        app->game_result = -1;
                    }
                } else if(input_event->key == InputKeyDown) {
                    while(app->dealer.total < 17) {
                        deal_card(&app->dealer);
                    }
                    if(app->dealer.total > 21 || app->player.total > app->dealer.total) {
                        app->game_result = 1;
                    } else if(app->player.total < app->dealer.total) {
                        app->game_result = -1;
                    } else {
                        app->game_result = 2;
                    }
                }
            } else if(input_event->key == InputKeyOk) {
                app->state = StateMenu;
                app->player.card_count = 0;
                app->dealer.card_count = 0;
                app->game_result = 0;
            }
        }
    }
}

void render_callback(Canvas* canvas, void* ctx) {
    BlackjackApp* app = ctx;

    canvas_clear(canvas);

    if(app->state == StateMenu) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 34, 10, "Blackjack");

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 5, 25, "Controls:");
        canvas_draw_str(canvas, 5, 35, "UP: Hit");
        canvas_draw_str(canvas, 5, 45, "DOWN: Stand");
        canvas_draw_str(canvas, 5, 55, "OK: Start Game");
    } else if(app->state == StateGame) {
        canvas_set_font(canvas, FontSecondary);

        int card_width = 24;
        int card_height = 36;
        int overlap_offset = 16;

        int dealer_start_x = 5;
        int player_start_x = 56;
        int y = 10;

        if(app->dealer.card_count > 0) {
            for(int i = 0; i < app->dealer.card_count; i++) {
                int x = dealer_start_x + i * overlap_offset;
                bool hidden = (i == 0 && app->game_result == 0);
                draw_card(canvas, x, y, &app->dealer.cards[i], hidden, card_width, card_height);
            }
        }

        if(app->player.card_count > 0) {
            for(int i = 0; i < app->player.card_count; i++) {
                int x = player_start_x + i * overlap_offset;
                draw_card(canvas, x, y, &app->player.cards[i], false, card_width, card_height);
            }
        }

        char player_total_buf[20];
        snprintf(player_total_buf, sizeof(player_total_buf), "Player: %d", app->player.total);
        canvas_draw_str(canvas, player_start_x, 55, player_total_buf);

        if(app->game_result != 0) {
            char dealer_total_buf[20];
            snprintf(dealer_total_buf, sizeof(dealer_total_buf), "Dealer: %d", app->dealer.total);
            canvas_draw_str(canvas, dealer_start_x, 55, dealer_total_buf);

            canvas_set_font(canvas, FontPrimary);
            if(app->game_result == 1) {
                canvas_draw_str(canvas, 40, 40, "You Win!");
            } else if(app->game_result == -1) {
                canvas_draw_str(canvas, 40, 40, "You Lose!");
            } else {
                canvas_draw_str(canvas, 50, 40, "Push!");
            }
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 30, 64, "Press OK to continue");
        }
    }
}

int32_t blackjack_rg_app(void* p) {
    UNUSED(p);
    BlackjackApp app;
    app.state = StateMenu;
    app.exit = false;
    app.view_port = view_port_alloc();
    view_port_draw_callback_set(app.view_port, render_callback, &app);
    view_port_input_callback_set(app.view_port, input_callback, &app);
    app.gui = furi_record_open("gui");
    gui_add_view_port(app.gui, app.view_port, GuiLayerFullscreen);
    while(!app.exit) {
        furi_delay_ms(100);
    }
    gui_remove_view_port(app.gui, app.view_port);
    view_port_free(app.view_port);
    furi_record_close("gui");
    return 0;
}

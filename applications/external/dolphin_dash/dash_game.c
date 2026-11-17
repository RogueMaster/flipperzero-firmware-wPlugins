#include "dash_game.h"

void dash_start(DashGame* game, uint32_t random) {
    *game = (DashGame){.phase = DashSwimming, .lane = 1, .best = game->best};
    for(unsigned i = 0; i < ReefCount; i++) {
        game->reefs[i] = (Reef){
            .x = 121 + (int)i * ReefSpacing,
            .lane = random % LaneCount,
            .has_bubble = true,
        };
        random /= LaneCount;
    }
}

void dash_move(DashGame* game, int direction) {
    int lane = (int)game->lane + direction;
    if(game->phase == DashSwimming && lane >= 0 && lane < LaneCount) {
        game->lane = (unsigned)lane;
    }
}

static bool touches_dolphin(int x, int radius) {
    return x + radius >= DolphinLeft && x - radius <= DolphinRight;
}

void dash_tick(DashGame* game, unsigned (*next_lane)(void)) {
    if(game->phase != DashSwimming) return;

    game->frame++;
    unsigned level = game->rocks_passed / 6;
    int speed = 2 + (int)(level > 3 ? 3 : level);
    for(unsigned i = 0; i < ReefCount; i++) {
        Reef* reef = &game->reefs[i];
        int old_right = reef->x + RockRadius;
        reef->x -= speed;

        if(reef->lane == game->lane) {
            if(touches_dolphin(reef->x, RockRadius)) {
                game->phase = DashOver;
                if(game->score > game->best) game->best = game->score;
                return;
            }
            if(reef->has_bubble && touches_dolphin(reef->x - BubbleLead, BubbleRadius)) {
                reef->has_bubble = false;
                game->score += 3;
            }
        }
        // Count a pass only on the tick when the whole rock clears the dolphin.
        if(old_right >= DolphinLeft && reef->x + RockRadius < DolphinLeft) {
            game->score++;
            game->rocks_passed++;
        }
        if(reef->x + RockRadius < 0) {
            reef->x += ReefCount * ReefSpacing;
            reef->lane = next_lane();
            reef->has_bubble = true;
        }
    }
}

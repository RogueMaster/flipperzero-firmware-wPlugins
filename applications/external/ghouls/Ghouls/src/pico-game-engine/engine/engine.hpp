#pragma once
#include "game.hpp"
#include ENGINE_DELAY_INCLUDE

class GameEngine
{
private:
    float fps;  // The frames per second of the game engine.
    Game *game; // The game to run.

public:
    GameEngine(Game *game, float fps)
        : fps(fps), game(game)
    {
    }

    inline void run()
    {
        // Initialize the game if not already active.
        if (!game->is_active)
        {
            game->start();
        }

        while (game->is_active)
        {
            // Update the game
            game->update();

            // Render the game
            game->render();

            ENGINE_DELAY_MS(1000 / fps);
        }

        this->stop();
    }

    inline void runAsync(bool shouldDelay = true)
    {
        // Initialize the game if not already active.
        if (!game->is_active)
        {
            game->start();
        }

        // Update the game
        game->update();

        // Render the game
        game->render();

        if (shouldDelay)
        {
            // Delay to control the frame rate
            ENGINE_DELAY_MS(1000 / fps);
        }
    }

    inline void stop()
    {
        // Stop the game
        game->stop();

        // clear the screen
        game->draw->fillScreen(game->bg_color);

        ENGINE_MEM_DELETE game;
        game = nullptr;
    }

    inline void updateGameInput(uint8_t input)
    {
        if (game && game->is_active)
        {
            game->input = input;
        }
    }

    inline Game *getGame() const { return game; }
};

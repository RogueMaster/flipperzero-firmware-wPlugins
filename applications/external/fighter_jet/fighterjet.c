/* Copyright (c) 2025 Eero Prittinen */

#include <stdlib.h>
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dolphin/dolphin.h>
#include "graphics.h"
#include "game_settings.h"
#include "autopilot.h"

typedef enum {
  EVENT_KEYDOWN,
  EVENT_KEYUP,
  EVENT_UPDATE,
  EVENT_QUIT,
} EventType;

typedef enum {
  KEY_DOWN,
  KEY_UP,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_BACK,
  KEY_OK,
  KEY_ESCAPE, // Mapped to a long press on back key
} InputEventKey;

typedef struct {
  EventType type;
  InputEventKey key;
} GameEvent;

FuriMutex *mutex;
game_state_t gameState;
bool run = true;
uint32_t runTime = 0;

void initRound(int round) {
  gameState.round = round;
  gameState.enemyCount = ENEMY_COUNT_START + round * ADD_ENEMIES_PER_ROUND;
  if (gameState.enemyCount > MAX_ENEMY_COUNT) {
    gameState.enemyCount = MAX_ENEMY_COUNT;
  }
  for (size_t i = 0; i < gameState.enemyCount; i++) {
    gameState.enemies[i].dir = (double) (furi_hal_random_get() % 600) / 100;
    gameState.enemies[i].ownDir = (double) (furi_hal_random_get() % 600) / 100;
    gameState.enemies[i].dist = MAX_ENEMY_DISTANCE;
    gameState.enemies[i].health = ENEMY_HEALTH;
    gameState.enemies[i].autopilot = autopilotFollower;
  }
  gameState.roundStartTs = runTime;
}

void updateGameState() {
  runTime++; // Keep track of game time
  if (gameState.health <= 0) {
    return;
  }
  // Update self direction
  if (gameState.turning == TURNING_LEFT) {
    gameState.dir += TURNING_SPEED;
    while (gameState.dir > PI2) {
      gameState.dir -= PI2;
    }
  } else if (gameState.turning == TURNING_RIGHT) {
    gameState.dir -= TURNING_SPEED;
    while (gameState.dir < 0) {
      gameState.dir += PI2;
    }
  }

  double speed = gameState.braking ? BRAKE_SPEED : SPEED;
  // Calculate new enemy positions
  double deltaX = cos(gameState.dir) * speed;
  double deltaY = sin(gameState.dir) * speed;

  for (size_t i = 0; i < gameState.enemyCount; i++) {
    if (gameState.enemies[i].dist < 1) {
      gameState.enemies[i].dir += PI2 / 2;
      gameState.enemies[i].dist = 1;
    }
    // No need to count dead enemies
    if (gameState.enemies[i].health <= 0) {
      continue;
    }

    turning_t enemyTurn = gameState.enemies[i].autopilot(
      gameState.enemies[i].dist,
      gameState.dir,
      gameState.enemies[i].dir,
      gameState.enemies[i].ownDir
    );
    if (enemyTurn == TURNING_LEFT) {
      gameState.enemies[i].ownDir += ENEMY_TURNING_SPEED;
      while (gameState.enemies[i].ownDir > PI2) {
        gameState.enemies[i].ownDir -= PI2;
      }
    } else if (enemyTurn == TURNING_RIGHT) {
      gameState.enemies[i].ownDir -= ENEMY_TURNING_SPEED;
      while (gameState.enemies[i].ownDir < 0) {
        gameState.enemies[i].ownDir += PI2;
      }
    }

    double deltaEnemyX = cos(gameState.enemies[i].ownDir) * ENEMY_SPEED;
    double deltaEnemyY = sin(gameState.enemies[i].ownDir) * ENEMY_SPEED;

    double enemyX = cos(gameState.enemies[i].dir) * gameState.enemies[i].dist;
    double enemyY = sin(gameState.enemies[i].dir) * gameState.enemies[i].dist;

    double newX = enemyX - deltaX + deltaEnemyX;
    double newY = enemyY - deltaY + deltaEnemyY;

    gameState.enemies[i].dist = sqrt((newX * newX) + (newY * newY));
    gameState.enemies[i].dir = atan2(newY, newX);
    while (gameState.enemies[i].dir < 0) {
      gameState.enemies[i].dir += PI2;
    }

    // Maximum distance, do not loose the enemies
    if (gameState.enemies[i].dist > MAX_ENEMY_DISTANCE) {
      gameState.enemies[i].dist = MAX_ENEMY_DISTANCE;
    }

    // Enemy shooting at you
    if (gameState.enemies[i].dist < ENEMY_SHOOTING_DISTANCE) {
      double enemyAimDir = gameState.enemies[i].ownDir - gameState.enemies[i].dir - PI2 / 2;
      if (enemyAimDir < (PI2 / (2 * gameState.enemies[1].dist))
          && enemyAimDir > -(PI2 / (2 * gameState.enemies[1].dist))) {
        gameState.health -= gameState.health > 0 ? 1 : 0;
      }
    }
  }

  if (gameState.shooting) {
    for (size_t i = 0; i < gameState.enemyCount; i++) {
      if (gameState.enemies[i].health <= 0) {
        continue;
      }
      double turningCorrection;
      if (gameState.turning == TURNING_LEFT) {
        turningCorrection = FOV / 4;
      } else if (gameState.turning == TURNING_RIGHT) {
        turningCorrection = -FOV / 4;
      } else {
        turningCorrection = 0;
      }

      if (gameState.enemies[i].dir > gameState.dir + turningCorrection - (PI2 / (2 * gameState.enemies[1].dist))
          && gameState.enemies[i].dir < gameState.dir + turningCorrection + (PI2 / (2 * gameState.enemies[1].dist))) {
        gameState.enemies[i].health--;
      }
    }
  }

  // Check if all enemies are destroyed
  bool enemiesLeft = false;
  for (size_t i = 0; i < gameState.enemyCount; i++) {
    if (gameState.enemies[i].health > 0) {
      enemiesLeft = true;
      break;
    }
  }

  if (!enemiesLeft) {
    initRound(gameState.round + 1);
  }
}

static void update_timer_callback(void *ctx) {
  furi_assert(ctx);
  FuriMessageQueue *event_queue = ctx;

  GameEvent event = { .type = EVENT_UPDATE };
  furi_message_queue_put(event_queue, &event, 0);
}

static void render_callback(Canvas *const canvas, void *ctx) {
  furi_assert(ctx);
  furi_mutex_acquire(mutex, FuriWaitForever);
  canvas_set_bitmap_mode(canvas, true);

  // Horizon
  canvas_draw_line(canvas, 0, 40, 127, 40); // TODO: Draw mountains

  // direction indicator
  drawCompass(canvas, gameState.dir);

  if (runTime - gameState.roundStartTs < ROUND_TITLE_TIME) {
    char roundStr[] = "ROUND    ";
    snprintf(roundStr + 6 * sizeof(char), 4, "%d", (uint8_t) gameState.round + 1); // round 0 -> print round 1
    canvas_draw_str_aligned(canvas, 64, 25, AlignCenter, AlignCenter, roundStr);
  }
  if (gameState.health <= 0) {
    // Game Over
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "GAME OVER");
    char roundStr[] = "survived      rounds"; // Reserving space
    snprintf(roundStr + 9 * sizeof(char), 4, "%d", (uint8_t) gameState.round);
    snprintf(roundStr + strlen(roundStr) * sizeof(char), 8, " rounds");
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, roundStr);
  }

  // Draw enemies
  // Minimap
  drawMinimap(canvas, gameState.dir, gameState.enemies, gameState.enemyCount);

  for (size_t i = 0; i < gameState.enemyCount; i++) {
    drawEnemy(canvas, gameState.dir, gameState.enemies[i]);
  }

  // Health
  char healthStr[5];
  memset(healthStr, 0, sizeof(healthStr));
  snprintf(healthStr, 4, "%d", (uint8_t) ((gameState.health * 100) / MAX_HEALTH));
  healthStr[strlen(healthStr)] = '%';
  canvas_draw_str(canvas, 0, 64, healthStr);

  // Fighter jet
  if (gameState.health > 0) {
    drawJet(canvas, gameState.turning, gameState.shooting);
  } else {
    drawExplosion(canvas);
  }

  furi_mutex_release(mutex);
}

static void input_callback(InputEvent *input_event, void *ctx) {
  furi_assert(ctx);
  FuriMessageQueue *event_queue = ctx;
  GameEvent event;

  switch (input_event->key) {
    case InputKeyUp:
      event.key = KEY_UP;
      break;
    case InputKeyDown:
      event.key = KEY_DOWN;
      break;
    case InputKeyRight:
      event.key = KEY_RIGHT;
      break;
    case InputKeyLeft:
      event.key = KEY_LEFT;
      break;
    case InputKeyOk:
      event.key = KEY_OK;
      break;
    case InputKeyBack:
      event.key = KEY_BACK;
      break;
    default:
      return;
  }

  switch (input_event->type) {
    case InputTypePress:
      event.type = EVENT_KEYDOWN;
      break;
    case InputTypeRelease:
      event.type = EVENT_KEYUP;
      break;
    case InputTypeLong:
      if (input_event->key == InputKeyBack) {
        event.type = EVENT_QUIT;
      }
      break;
    default:
      return;
  }
  furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

int fighterjet_app(void *p) {
  UNUSED(p);

  mutex = furi_mutex_alloc(FuriMutexTypeNormal);
  if (mutex == NULL) {
    FURI_LOG_E("Jet fighter", "cannot create mutex\r\n");
    return 255;
  }
  ViewPort *view_port = view_port_alloc();
  FuriMessageQueue *event_queue = furi_message_queue_alloc(8, sizeof(GameEvent));
  view_port_draw_callback_set(view_port, render_callback, NULL);
  view_port_input_callback_set(view_port, input_callback, event_queue);
  Gui *gui = furi_record_open(RECORD_GUI);
  gui_add_view_port(gui, view_port, GuiLayerFullscreen);

  furi_hal_random_init();
  FuriTimer *timer = furi_timer_alloc(update_timer_callback, FuriTimerTypePeriodic, event_queue);
  furi_timer_start(timer, furi_kernel_get_tick_frequency() / FRAMERATE);

  // Set player parameters
  gameState.dir = PI2 / 4; // To North
  gameState.health = MAX_HEALTH;

  initRound(0);
  GameEvent event;
  while (run) {
    FuriStatus event_status = furi_message_queue_get(event_queue, &event, FuriWaitForever);
    if (event_status != FuriStatusOk) {
      continue;
    }
    furi_mutex_acquire(mutex, FuriWaitForever);

    switch (event.type) {
      case EVENT_UPDATE:
        updateGameState();
        view_port_update(view_port);
        break;
      case EVENT_KEYDOWN:
        if (event.key == KEY_LEFT && gameState.turning == TURNING_NONE) {
          gameState.turning = TURNING_LEFT;
        } else if (event.key == KEY_RIGHT && gameState.turning == TURNING_NONE) {
          gameState.turning = TURNING_RIGHT;
        } else if (event.key == KEY_OK) {
          gameState.shooting = true;
        } else if (event.key == KEY_DOWN) {
          gameState.braking = true;
        }
        break;
      case EVENT_KEYUP:
        if (event.key == KEY_LEFT || event.key == KEY_RIGHT) {
          gameState.turning = TURNING_NONE;
        } else if (event.key == KEY_OK) {
          gameState.shooting = false;
        } else if (event.key == KEY_DOWN) {
          gameState.braking = false;
        }
        break;
      case EVENT_QUIT:
        run = false;
        break;
    }
    furi_mutex_release(mutex);
  }

  furi_timer_stop(timer);
  furi_timer_free(timer);
  furi_mutex_acquire(mutex, FuriWaitForever); // Make sure, not graphics updates are ongoing
  view_port_enabled_set(view_port, false);
  gui_remove_view_port(gui, view_port);
  furi_record_close(RECORD_GUI);
  view_port_free(view_port);
  furi_message_queue_free(event_queue);
  furi_mutex_release(mutex);
  furi_mutex_free(mutex);
  return 0;
}

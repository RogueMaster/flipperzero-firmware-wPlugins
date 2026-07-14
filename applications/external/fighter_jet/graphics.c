/* Copyright (c) 2025 Eero Prittinen */

#include "graphics.h"
#include "game_settings.h"

#define compass_width 5
#define compass_height 5
static uint8_t compass_n_bits[] = { 0xf1, 0xf3, 0xf5, 0xf9, 0xf1 };
static uint8_t compass_e_bits[] = { 0xee, 0xe2, 0xe6, 0xe2, 0xee };
static uint8_t compass_s_bits[] = { 0xee, 0xe2, 0xe4, 0xe8, 0xee };
static uint8_t compass_w_bits[] = { 0xf1, 0xf1, 0xf5, 0xf5, 0xea };

#define explosion_width 27
#define explosion_height 13
static unsigned char explosion_bits[] = {
  0x80, 0x00, 0x02, 0xf8, 0x20, 0x20, 0x21, 0xf8, 0x00, 0x25, 0x10, 0xfa, 0x21, 0x02, 0x89, 0xf9, 0x06, 0xb0,
  0x60, 0xf8, 0x18, 0x81, 0x10, 0xf8, 0x60, 0x4e, 0xec, 0xfb, 0xa0, 0x50, 0x00, 0xf8, 0x46, 0x92, 0x72, 0xf9,
  0x00, 0x9c, 0x00, 0xf8, 0x00, 0x13, 0x1d, 0xf8, 0x40, 0x88, 0x60, 0xf8, 0x18, 0x8c, 0xc0, 0xf9
};

#define fighter_jet_straight_width 27
#define fighter_jet_straight_height 13
static unsigned char fighter_jet_straight_bits[] = {
  0x00, 0x20, 0x00, 0xf8, 0x00, 0x50, 0x00, 0xf8, 0x00, 0xa8, 0x00, 0xf8, 0x00, 0x74, 0x01, 0xf8, 0x00, 0x72,
  0x03, 0xf8, 0x00, 0x01, 0x04, 0xf8, 0xc0, 0x24, 0x19, 0xf8, 0x30, 0x54, 0x61, 0xf8, 0x0c, 0x04, 0x81, 0xf9,
  0x02, 0x8e, 0x03, 0xfa, 0xff, 0x71, 0xfc, 0xff, 0x00, 0x51, 0x04, 0xf8, 0x00, 0x8e, 0x03, 0xf8,
};

#define turn_right_width 18
#define turn_right_height 20
static unsigned char turn_right_bits[] = {
  0x07, 0x00, 0xfc, 0x3b, 0x00, 0xfc, 0xe2, 0x00, 0xfc, 0x84, 0xff, 0xfd, 0x08, 0x00, 0xfd,
  0x10, 0x72, 0xfd, 0x70, 0x61, 0xfd, 0x90, 0x40, 0xfd, 0x10, 0x0d, 0xfd, 0x20, 0x09, 0xfd,
  0xc0, 0xa1, 0xfc, 0x00, 0x97, 0xfc, 0x00, 0x89, 0xfc, 0x00, 0x91, 0xfd, 0x00, 0x92, 0xfd,
  0x00, 0x2c, 0xfd, 0x00, 0x40, 0xff, 0x00, 0x80, 0xfe, 0x00, 0x00, 0xff, 0x00, 0x00, 0xfe,
};

#define turn_left_width 18
#define turn_left_height 20
static unsigned char turn_left_bits[] = {
  0x00, 0x80, 0x03, 0x00, 0x70, 0x03, 0x00, 0x1c, 0x01, 0xfe, 0x87, 0x00, 0x02, 0x40, 0x00,
  0x3a, 0x21, 0x00, 0x1a, 0x3a, 0x00, 0x0a, 0x24, 0x00, 0xc2, 0x22, 0x00, 0x42, 0x12, 0x00,
  0x14, 0x0e, 0x00, 0xa4, 0x03, 0x00, 0x44, 0x02, 0x00, 0x26, 0x02, 0x00, 0x26, 0x01, 0x00,
  0xd2, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x05, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00,
};

typedef struct {
  size_t width;
  size_t height;
  unsigned char *bits;
} enemy_xbm_t;

#define enemy_left_0_width 3
#define enemy_left_0_height 2
static unsigned char enemy_left_0_bits[] = { 0xfc, 0xff };

static enemy_xbm_t enemyLeft0 = {
  .width = enemy_left_0_width,
  .height = enemy_left_0_height,
  .bits = enemy_left_0_bits,
};

#define enemy_left_1_width 7
#define enemy_left_1_height 3
unsigned char enemy_left_1_bits[] = { 0xa0, 0xbe, 0xff };

static enemy_xbm_t enemyLeft1 = {
  .width = enemy_left_1_width,
  .height = enemy_left_1_height,
  .bits = enemy_left_1_bits,
};

#define enemy_left_2_width 10
#define enemy_left_2_height 5
static unsigned char enemy_left_2_bits[] = { 0x00, 0xfd, 0x80, 0xfd, 0x7c, 0xfd, 0x02, 0xfe, 0xff, 0xff };
// TODO: Consts for optimization
static enemy_xbm_t enemyLeft2 = {
  .width = enemy_left_2_width,
  .height = enemy_left_2_height,
  .bits = enemy_left_2_bits,
};

#define enemy_left_3_width 15
#define enemy_left_3_height 7
static unsigned char enemy_left_3_bits[] = { 0x00, 0xb0, 0xf8, 0xaf, 0x3c, 0xa0, 0x82,
                                             0xca, 0x7f, 0xe0, 0x80, 0xa3, 0x00, 0xbc };
enemy_xbm_t enemyLeft3 = {
  .width = enemy_left_3_width,
  .height = enemy_left_3_height,
  .bits = enemy_left_3_bits,
};

#define enemy_right_0_width 3
#define enemy_right_0_height 2
static unsigned char enemy_right_0_bits[] = { 0xf9, 0xff };

enemy_xbm_t enemyRight0 = {
  .width = enemy_right_0_width,
  .height = enemy_right_0_height,
  .bits = enemy_right_0_bits,
};

#define enemy_right_1_width 7
#define enemy_right_1_height 3
static unsigned char enemy_right_1_bits[] = { 0x82, 0xbe, 0xff };

enemy_xbm_t enemyRight1 = {
  .width = enemy_right_1_width,
  .height = enemy_right_1_height,
  .bits = enemy_right_1_bits,
};

#define enemy_right_2_width 10
#define enemy_right_2_height 5
static unsigned char enemy_right_2_bits[] = { 0x02, 0xfc, 0x06, 0xfc, 0xfe, 0xfc, 0x03, 0xfd, 0xff, 0xff };

enemy_xbm_t enemyRight2 = {
  .width = enemy_right_2_width,
  .height = enemy_right_2_height,
  .bits = enemy_right_2_bits,
};

#define enemy_right_3_width 15
#define enemy_right_3_height 7
static unsigned char enemy_right_3_bits[] = { 0x06, 0x80, 0xfa, 0x8f, 0x02, 0x9e, 0xa9,
                                              0xa0, 0x03, 0xff, 0xe2, 0x80, 0x1e, 0x80 };

enemy_xbm_t enemyRight3 = {
  .width = enemy_right_3_width,
  .height = enemy_right_3_height,
  .bits = enemy_right_3_bits,
};

#define enemy_back_0_width 3
#define enemy_back_0_height 2
static unsigned char enemy_back_0_bits[] = { 0xfa, 0xff };

enemy_xbm_t enemyBack0 = {
  .width = enemy_back_0_width,
  .height = enemy_back_0_height,
  .bits = enemy_back_0_bits,
};

#define enemy_back_1_width 5
#define enemy_back_1_height 2
static unsigned char enemy_back_1_bits[] = {
  0xea,
  0xff,
};

enemy_xbm_t enemyBack1 = {
  .width = enemy_back_1_width,
  .height = enemy_back_1_height,
  .bits = enemy_back_1_bits,
};

#define enemy_back_2_width 9
#define enemy_back_2_height 4
static unsigned char enemy_back_2_bits[] = {
  0x44, 0xfe, 0x7c, 0xfe, 0xd7, 0xff, 0x28, 0xfe,
};

enemy_xbm_t enemyBack2 = {
  .width = enemy_back_2_width,
  .height = enemy_back_2_height,
  .bits = enemy_back_2_bits,
};

#define enemy_back_3_width 13
#define enemy_back_3_height 5
static unsigned char enemy_back_3_bits[] = {
  0x10, 0xe1, 0x50, 0xe1, 0xfc, 0xe7, 0x4f, 0xfe, 0xb0, 0xe1,
};

enemy_xbm_t enemyBack3 = {
  .width = enemy_back_3_width,
  .height = enemy_back_3_height,
  .bits = enemy_back_3_bits,
};

#define enemy_back_4_width 15
#define enemy_back_4_height 6
static unsigned char enemy_back_4_bits[] = { 0x20, 0x82, 0xe0, 0x83, 0xf0, 0x87, 0xcf, 0xf9, 0xc8, 0x89, 0x30, 0x86 };

enemy_xbm_t enemyBack4 = {
  .width = enemy_back_4_width,
  .height = enemy_back_4_height,
  .bits = enemy_back_4_bits,
};

#define enemy_front_2_width 9
#define enemy_front_2_height 4
static unsigned char enemy_front_2_bits[] = { 0x44, 0xfe, 0x7c, 0xfe, 0xff, 0xff, 0x10, 0xfe };

enemy_xbm_t enemyFront2 = {
  .width = enemy_front_2_width,
  .height = enemy_front_2_height,
  .bits = enemy_front_2_bits,
};

#define enemy_front_3_width 13
#define enemy_front_3_height 5
static unsigned char enemy_front_3_bits[] = { 0x10, 0xe1, 0x50, 0xe1, 0xf8, 0xe3, 0xff, 0xff, 0xe0, 0xe0 };

enemy_xbm_t enemyFront3 = {
  .width = enemy_front_3_width,
  .height = enemy_front_3_height,
  .bits = enemy_front_3_bits,
};

#define enemy_front_4_width 15
#define enemy_front_4_height 6
static unsigned char enemy_front_4_bits[] = { 0x20, 0x82, 0xa0, 0x82, 0xe0, 0x83, 0xff, 0xff, 0xf0, 0x87, 0xc0, 0x81 };

enemy_xbm_t enemyFront4 = {
  .width = enemy_front_4_width,
  .height = enemy_front_4_height,
  .bits = enemy_front_4_bits,
};

#define enemy_dot_width 1
#define enemy_dot_height 1
static unsigned char enemy_dot_bits[] = {
  0xff,
};

enemy_xbm_t enemyDot = {
  .width = enemy_dot_width,
  .height = enemy_dot_height,
  .bits = enemy_dot_bits,
};

// List of enemy renders, indexing (direction, distance)
// Directions = back, left, front, right
// Distance, biggest to smallest
enemy_xbm_t *enemyRenders[4][6] = {
  { &enemyBack4, &enemyBack3, &enemyBack2, &enemyBack1, &enemyBack0, &enemyDot },
  { &enemyLeft3, &enemyLeft3, &enemyLeft2, &enemyLeft1, &enemyLeft0, &enemyDot },
  { &enemyFront4, &enemyFront3, &enemyFront2, &enemyBack1, &enemyBack0, &enemyDot },
  { &enemyRight3, &enemyRight3, &enemyRight2, &enemyRight1, &enemyRight0, &enemyDot },
};

int renderingDistances[] = {
  10, // render biggest (4)
  20, // render 3
  30, // render 2
  50, // render 1
  100, // render 0
  MAX_ENEMY_DISTANCE,
};

void drawEnemy(Canvas *canvas, double dir, fighter_jet_t enemy) {
  if (enemy.health <= 0) {
    return;
  }
  double enemyDir = dir - enemy.dir;
  while (enemyDir > PI2 / 2) {
    enemyDir -= PI2;
  }
  while (enemyDir < -PI2 / 2) {
    enemyDir += PI2;
  }
  if (enemyDir > FOV && enemyDir < -FOV) {
    return;
  }

  int drawX = (enemyDir + FOV) / (FOV * 2) * 128;

  unsigned int distanceIndex = 0;
  for (size_t i = 0; i < sizeof(renderingDistances) / sizeof(*renderingDistances); i++) {
    distanceIndex = i;
    if (enemy.dist <= renderingDistances[i]) {
      break;
    }
  }

  // Calculate orientation
  double enemyOrientation = enemy.ownDir - enemy.dir + PI2 / 8;
  while (enemyOrientation < 0) {
    enemyOrientation += PI2;
  }
  while (enemyOrientation > PI2) {
    enemyOrientation -= PI2;
  }

  int dirIndex = enemyOrientation > 0 && enemyOrientation < PI2 / 4 ? 0
    : enemyOrientation > PI2 / 4 && enemyOrientation < PI2 / 2      ? 1
    : enemyOrientation > PI2 / 2 && enemyOrientation < 3 * PI2 / 4  ? 2
                                                                    : 3;
  enemy_xbm_t *enemyXbm = enemyRenders[dirIndex][distanceIndex];

  int drawY = enemy.dist < 3 ? 10 * enemy.dist : 35;
  canvas_draw_xbm(canvas, drawX - (enemyXbm->width / 2), drawY, enemyXbm->width, enemyXbm->height, enemyXbm->bits);
}

void drawCompass(Canvas *canvas, double direction) {
  canvas_draw_line(canvas, (64 - COMPASS_WIDTH / 2), 0, (64 - COMPASS_WIDTH / 2), 5);
  canvas_draw_line(canvas, (64 - COMPASS_WIDTH / 2) + COMPASS_WIDTH, 0, (64 - COMPASS_WIDTH / 2) + COMPASS_WIDTH, 5);
  for (size_t i = 0; i < COMPASS_WIDTH; i++) {
    if (((int) (((direction * COMPASS_WIDTH) / PI2)) + i) % 3 == 0) {
      canvas_draw_dot(canvas, (64 - COMPASS_WIDTH / 2) + i, 3);
    }
  }
  // Normalize direction to 0 -PI2

  if (direction >= PI2 / 8 && direction <= 3 * PI2 / 8) { // North
    canvas_draw_xbm(
      canvas,
      (64 - COMPASS_WIDTH / 2) + (direction - PI2 / 8) * (COMPASS_WIDTH - compass_width) / (PI2 / 4),
      0,
      compass_width,
      compass_height,
      compass_n_bits
    );
  } else if (direction >= 3 * PI2 / 8 && direction <= 5 * PI2 / 8) {
    canvas_draw_xbm(
      canvas,
      (64 - COMPASS_WIDTH / 2) + (direction - 3 * PI2 / 8) * (COMPASS_WIDTH - compass_width) / (PI2 / 4),
      0,
      compass_width,
      compass_height,
      compass_w_bits
    );
  } else if (direction >= 5 * PI2 / 8 && direction <= 7 * PI2 / 8) {
    canvas_draw_xbm(
      canvas,
      (64 - COMPASS_WIDTH / 2) + (direction - 5 * PI2 / 8) * (COMPASS_WIDTH - compass_width) / (PI2 / 4),
      0,
      compass_width,
      compass_height,
      compass_s_bits
    );
  } else if (direction >= 7 * PI2 / 8) {
    canvas_draw_xbm(
      canvas,
      (64 - COMPASS_WIDTH / 2) + (direction - 7 * PI2 / 8) * (COMPASS_WIDTH - compass_width) / (PI2 / 4),
      0,
      compass_width,
      compass_height,
      compass_e_bits
    );
  } else if (direction <= PI2 / 8) {
    canvas_draw_xbm(
      canvas,
      (64 - COMPASS_WIDTH / 2) + (direction + PI2 / 8) * (COMPASS_WIDTH - compass_width) / (PI2 / 4),
      0,
      compass_width,
      compass_height,
      compass_e_bits
    );
  }
}

void drawMinimap(Canvas *canvas, double dir, fighter_jet_t *enemies, size_t enemiesCount) {
  canvas_draw_circle(canvas, MINIMAP_X + MINIMAP_R, MINIMAP_Y + MINIMAP_R, MINIMAP_R);
  canvas_draw_line(
    canvas,
    MINIMAP_X + MINIMAP_R - 1,
    MINIMAP_Y + MINIMAP_R,
    MINIMAP_X + MINIMAP_R + 1,
    MINIMAP_Y + MINIMAP_R
  );
  canvas_draw_line(
    canvas,
    MINIMAP_X + MINIMAP_R,
    MINIMAP_Y + MINIMAP_R - 1,
    MINIMAP_X + MINIMAP_R,
    MINIMAP_Y + MINIMAP_R + 1
  );

  for (size_t i = 0; i < enemiesCount; i++) {
    if (enemies[i].health <= 0) {
      continue;
    }
    if (enemies[i].dist < MAX_ENEMY_DISTANCE) {
      double eX = cos(dir - enemies[i].dir - PI2 / 4) * enemies[i].dist;
      double eY = sin(dir - enemies[i].dir - PI2 / 4) * enemies[i].dist;
      canvas_draw_dot(
        canvas,
        1 + MINIMAP_X + MINIMAP_R + (eX / MAX_ENEMY_DISTANCE * MINIMAP_R),
        1 + MINIMAP_Y + MINIMAP_R + (eY / MAX_ENEMY_DISTANCE * MINIMAP_R)
      );
    }
  }
}

void drawJet(Canvas *canvas, turning_t turning, bool shooting) {
  // TODO: Draw transparently
  if (turning == TURNING_LEFT) {
    canvas_draw_xbm(canvas, 64 - (turn_left_width / 2), 43, turn_left_width, turn_left_height, turn_left_bits);
    if (shooting) {
      canvas_draw_line(canvas, 68, 45, 51, 38);
      canvas_draw_line(canvas, 55, 55, 51, 39);
    }
  } else if (turning == TURNING_RIGHT) {
    canvas_draw_xbm(canvas, 64 - (turn_right_width / 2), 43, turn_right_width, turn_right_height, turn_right_bits);
    if (shooting) {
      canvas_draw_line(canvas, 60, 45, 75, 38);
      canvas_draw_line(canvas, 73, 55, 75, 39);
    }
  } else if (turning == TURNING_NONE) {
    canvas_draw_xbm(
      canvas,
      64 - (fighter_jet_straight_width / 2),
      43,
      fighter_jet_straight_width,
      fighter_jet_straight_height,
      fighter_jet_straight_bits
    );
    if (shooting) {
      canvas_draw_line(canvas, 56, 48, 64, 39);
      canvas_draw_line(canvas, 72, 48, 64, 39);
    }
  }
}

void drawExplosion(Canvas *canvas) {
  canvas_draw_xbm(canvas, 64 - (explosion_width / 2), 43, explosion_width, explosion_height, explosion_bits);
}

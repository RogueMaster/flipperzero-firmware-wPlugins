#include <lib/Arduboy2.h>

//     0   1   2   3   4   5   6   7   8   9   0   1   2   3   4   5   6   7   8   9   0   1   2  3
const uint8_t speed[] PROGMEM = {1, 1,  2,  3,  4,  4,  5,  5,  6,  6,  7,  8,
                                 9, 10, 12, 13, 14, 15, 16, 17, 18, 20, 21, 22};

// Load a new wave of enemies and replenish barriers.

void invader_NewWave(Invader_General2& general2) {
    FX::seekData(FX::readIndexedUInt24(Levels::Level_Items, 0) + 16);

    for(uint8_t x = 0; x < 21 + 16; x++) { // enemies + barriers

        Item& item = level.getItem(Constants::Invaders_Enemy_Row_1_Start + x);
        item.itemType = static_cast<ItemType>(FX::readPendingUInt8());
        FX::readBytes((uint8_t*)&item.data.rawData, sizeof(item.data.rawData));

        if(x < 21) {
            item.data.invader_Enemy.y = item.data.invader_Enemy.y - 24 + general2.launchOffset;
        }
    }

    FX::readEnd();

#ifndef SAVE_MEMORY_SOUND
    setSound(SoundIndex::Invader_Wave_Start);
#endif

    general2.bulletCountdown = 0;
}

uint8_t invader_EnemiesAlive() {
    uint8_t count = 0;

    for(uint8_t x = Constants::Invaders_Enemy_Row_1_Start;
        x <= Constants::Invaders_Enemy_Row_3_End;
        x++) {
        Invader_Enemy& enemy = level.getItem(x).data.invader_Enemy;
        if(enemy.status != Status::Dead) count++;
    }

    return count;
}

uint8_t invader_getSpeed() {
    uint8_t x = invader_EnemiesAlive();
    return pgm_read_byte(&speed[x]);
}

void invader_RenderHUD(Invader_General& general) {
    FX::drawFrame(Invaders_HUD_frame);
    FX::drawBitmap(123, 1, Images::HUD_Spaceship, general.lives, dbmNormal);
    FX::drawBitmap(123, 48, Images::Numbers_Small, general.score / 100, dbmNormal);
    FX::drawBitmap(123, 56, Images::Numbers_Small, general.score % 100, dbmNormal);
}

void invader_RenderEnemies(int yOffset = 0) {
    for(uint8_t x = Constants::Invaders_Enemy_Row_1_Start;
        x <= Constants::Invaders_Enemy_Row_3_End;
        x++) {
        uint8_t image =
            ((x - static_cast<uint8_t>(Constants::Invaders_Enemy_Row_1_Start)) / 7) * 6;
        uint8_t frame = 0;

        Invader_Enemy& enemy = level.getItem(x).data.invader_Enemy;

        // Calculate frame ..

        switch(enemy.status) {
        case Status::Active:
            frame = (arduboy.getFrameCountHalf(80) ? (x % 2) : 1 - (x % 2));
            break;

        case Status::Exploding1... Status::Exploding4:
            frame = static_cast<uint8_t>(enemy.status) - 3;
            break;

        default:
            continue;
        }

        if(enemy.status != Status::Dead) {
            FX::drawBitmap(
                enemy.x - 8, enemy.y + yOffset - 8, Images::Invaders, image + frame, dbmMasked);
        }
    }
}

void invader_RenderBarriers(int yOffset = 0) {
    for(uint8_t x = Constants::Invaders_Barrier_Start; x <= Constants::Invaders_Barrier_End; x++) {
        Invader_Barrier& barrier = level.getItem(x).data.invader_Barrier;
        FX::drawBitmap(barrier.x, barrier.y + yOffset, Images::Barriers, barrier.value, dbmMasked);
    }
}

void invader_RenderPlayer(Invader_Player& player, int yOffset = 0, bool show = false) {
    uint8_t frame = 0;

    switch(player.status) {
    case Status::Safe:
        if(!show && arduboy.getFrameCountHalf(32)) return;
        break;

    case Status::Exploding1... Status::Exploding4:
        frame = static_cast<uint8_t>(player.status) - 4;
        break;

    case Status::Dead:
        return;

    default: // case Status::Active, EnemiesAppearing
        break;
    }

    FX::drawBitmap(player.x - 8, player.y + yOffset - 8, Images::Player, frame, dbmMasked);
}

void invader_RenderPlayerBullet(Invader_Bullet& bullet) {
    FX::drawBitmap(bullet.x, bullet.y, Images::Bullet, 0, dbmNormal);
}

void invader_RenderEnemyBullets() {
    for(uint8_t x = Constants::Invaders_Enemy_Bullet_Start;
        x <= Constants::Invaders_Enemy_Bullet_End;
        x++) {
        Invader_Bullet& bullet = level.getItem(x).data.invader_Bullet;

        if(bullet.y < 64) {
            FX::drawBitmap(bullet.x, bullet.y, Images::Bullet, 0, dbmNormal);
        }
    }
}

void invader_MoveEnemies_Down(Invader_Player& player) {
    if(player.status == Status::Active || player.status == Status::EnemiesAppearing) {
        for(uint8_t x = Constants::Invaders_Enemy_Row_1_Start;
            x <= Constants::Invaders_Enemy_Row_3_End;
            x++) {
            Invader_Enemy& enemy = level.getItem(x).data.invader_Enemy;
            enemy.y++;
        }
    }
}

void invader_MoveEnemies_Left(Invader_General& general, Invader_Player& player, uint8_t test) {
    Invader_Enemy& testItem = level.getItem(test).data.invader_Enemy;

    for(uint8_t x = Constants::Invaders_Enemy_Row_1_Start;
        x <= Constants::Invaders_Enemy_Row_3_End;
        x++) {
        Invader_Enemy& enemy = level.getItem(x).data.invader_Enemy;
        enemy.x = enemy.x - 2;
    }

    // Move enemies down ?

    if(testItem.x <= 5) {
        general.direction = Direction::Right;
        invader_MoveEnemies_Down(player);
    }
}

void invader_MoveEnemies_Right(Invader_General& general, Invader_Player& player, uint8_t test) {
    Invader_Enemy& testItem = level.getItem(test).data.invader_Enemy;

    for(uint8_t x = Constants::Invaders_Enemy_Row_1_Start;
        x <= Constants::Invaders_Enemy_Row_3_End;
        x++) {
        Invader_Enemy& enemy = level.getItem(x).data.invader_Enemy;
        enemy.x = enemy.x + 2;
    }

    // Move enemies down ?

    if(testItem.x >= 103) {
        general.direction = Direction::Left;
        invader_MoveEnemies_Down(player);
    }
}

void invader_UpdateEnemy(
    Invader_General& general,
    Invader_General2& general2,
    Invader_Player& player) {
    uint8_t frameCount = arduboy.getFrameCount(invader_getSpeed());

    // Work out left and right limits of enemies ..

    general.right = 0;
    general.left = 6;

    for(uint8_t x = Constants::Invaders_Enemy_Row_1_Start;
        x <= Constants::Invaders_Enemy_Row_1_End;
        x++) {
        for(uint8_t y = 0; y < 3; y++) {
            Invader_Enemy& enemy = level.getItem(x + (y * 7)).data.invader_Enemy;

            if(enemy.status == Status::Active) {
                if(x > general.right) general.right = x;
                if(x < general.left) general.left = x;
            }

            // Update explosions ..

            if(enemy.status >= Status::Exploding1 && enemy.status < Status::Dead) {
                enemy.status = static_cast<Status>(static_cast<uint8_t>(enemy.status) + 1);
            }
        }
    }

    general.right = general.right - Constants::Invaders_Enemy_Row_1_Start;
    general.left = general.left - Constants::Invaders_Enemy_Row_1_Start;

    // Move enemies ..

    if(player.status != Status::EnemiesAppearing && frameCount == 0) {
        general2.speed = invader_getSpeed();

        switch(general.direction) {
        case Direction::Left:

            invader_MoveEnemies_Left(
                general, player, Constants::Invaders_Enemy_Row_1_Start + general.left);
            break;

        case Direction::Right:

            invader_MoveEnemies_Right(
                general, player, Constants::Invaders_Enemy_Row_1_Start + general.right);
            break;

        default:
            break;
        }
    }
}

void invader_UpdatePlayer(
    Invader_General& general,
    Invader_General2& general2,
    Invader_Player& player) {
    if(player.status >= Status::Exploding1 && player.status < Status::Dead) {
        player.status = static_cast<Status>(static_cast<uint8_t>(player.status) + 1);

        if(player.status == Status::Dead) {
            if(general.lives > 0) general.lives--;
            general2.deathCountdown = 48;

            if(general.lives == 0) {
#ifndef SAVE_MEMORY_SOUND
                setSound(SoundIndex::Invader_End_of_Game);
#endif
            }
        }
    }
}

void invader_EnemyDropsBullet(Invader_General2& general2, Invader_Player& player) {
    // Are we still in the pause period between bullets ?

    if(general2.bulletCountdown > 0 || player.status != Status::Active) {
        general2.bulletCountdown--;
        return;
    }

    // Do we have a spare bullet?

    uint8_t found = Constants::NoItemFound;

    for(uint8_t x = Constants::Invaders_Enemy_Bullet_Start;
        x <= Constants::Invaders_Enemy_Bullet_End;
        x++) {
        Invader_Bullet& bullet = level.getItem(x).data.invader_Bullet;

        if(bullet.y < 0 || bullet.y >= 64) {
            found = x;
            break;
        }
    }

    // If we found a bullet then launch it!

    if(found != Constants::NoItemFound) {
        bool dropped = false;

        Invader_Bullet& bullet = level.getItem(found).data.invader_Bullet;

        if(arduboy.randomLFSR(0, 2) == 0) {
            // Drop overhead ..

            for(uint8_t x = Constants::Invaders_Enemy_Row_3_End;
                x > Constants::Invaders_Enemy_Row_1_Start;
                x--) {
                Invader_Enemy& enemy = level.getItem(x).data.invader_Enemy;

                if(enemy.status == Status::Active && abs(enemy.x - player.x) < 16) {
                    bullet.x = enemy.x + 4;
                    bullet.y = enemy.y + 8;
                    general2.bulletCountdown =
                        4 + arduboy.randomLFSR(general2.speed * 4, general2.speed * 8);
                    dropped = true;

#ifndef SAVE_MEMORY_SOUND
                    setSound(SoundIndex::Invader_Enemy_Fires_Bullet);
#endif

                    break;
                }
            }
        }

        // If we did not drop an overhead bullet, then drop a random one ..

        if(!dropped) {
            for(uint8_t x = 0; x < 60; x++) {
                Invader_Enemy& enemy =
                    level
                        .getItem(Constants::Invaders_Enemy_Row_1_Start + arduboy.randomLFSR(0, 22))
                        .data.invader_Enemy;

                if(enemy.status == Status::Active && abs(enemy.x - player.x) < 16) {
                    bullet.x = enemy.x + 4;
                    bullet.y = enemy.y + 8;
                    general2.bulletCountdown =
                        8 + arduboy.randomLFSR(general2.speed * 4, general2.speed * 6);

#ifndef SAVE_MEMORY_SOUND
                    setSound(SoundIndex::Invader_Enemy_Fires_Bullet);
#endif

                    break;
                }
            }
        }
    }
}

void invader_HasBulletHitBarrier(Invader_General2& general2, Invader_Bullet& bullet) {
    // Hit barrier?

    uint8_t barrier_X = Constants::NoItemFound;
    uint8_t barrier_B;
    uint8_t barrier_Y;

    switch(bullet.x) {
    case 26 ... 41:

        switch(bullet.y) {
        case 46 ... 51:

            barrier_X = (bullet.x - 26) / 4;
            barrier_B = (bullet.x - 26) % 4;
            barrier_Y = (bullet.y <= 48) ? 0 : 1;
            break;
        }

        break;

    case 78 ... 93:

        switch(bullet.y) {
        case 46 ... 51:

            barrier_X = ((bullet.x - 78) / 4) + 4;
            barrier_B = (bullet.x - 78) % 4;
            barrier_Y = (bullet.y <= 48) ? 0 : 1;
            break;
        }

        break;
    }

    if(barrier_X != Constants::NoItemFound) {
        Invader_Barrier& barrier =
            level.getItem(Constants::Invaders_Barrier_Start + (barrier_X * 2) + barrier_Y)
                .data.invader_Barrier;

        if((barrier.value & (1 << barrier_B)) == 0) {
#ifndef SAVE_MEMORY_SOUND
            setSound(SoundIndex::Invader_Hit_Barrier);
#endif

            barrier.value = (barrier.value | (1 << barrier_B));
            bullet.y = -4;
            general2.bulletPlayerCountdown = 5;
        }
    }
}

void invader_DetectPlayerBulletHit(
    Invader_General& general,
    Invader_General2& general2,
    Invader_Bullet& bullet) {
    // Only test for active bullets ..

    if(bullet.y == -4) return;

    Point bulletPoint = {bullet.x, bullet.y};

    for(uint8_t x = Constants::Invaders_Enemy_Row_1_Start;
        x <= Constants::Invaders_Enemy_Row_3_End;
        x++) {
        Invader_Enemy& enemy = level.getItem(x).data.invader_Enemy;

        if(enemy.status == Status::Active) {
            Rect enemyRect = {enemy.x + 1, enemy.y, 7, 7};

            if(arduboy.collide(bulletPoint, enemyRect)) {
                enemy.status = Status::Exploding1;
                bullet.y = -4;
                general.score++;

#ifndef SAVE_MEMORY_SOUND
                setSound(SoundIndex::Invader_Enemy_Explosion);
#endif
            }

            invader_HasBulletHitBarrier(general2, bullet);
        }
    }
}

void invader_UpdateEnemyBullets(Invader_General2& general2, Invader_Player& player) {
    for(uint8_t x = Constants::Invaders_Enemy_Bullet_Start;
        x <= Constants::Invaders_Enemy_Bullet_End;
        x++) {
        Invader_Bullet& bullet = level.getItem(x).data.invader_Bullet;

        if(bullet.y >= -3 && bullet.y < 64) {
            bullet.y++;

            if(player.status == Status::Active) {
                Point bulletPoint = {bullet.x, bullet.y};
                Rect playerRect = {player.x, player.y + 2, 9, 7};

                if(arduboy.collide(bulletPoint, playerRect)) {
                    player.status = Status::Exploding1;
                    bullet.y = 64;

#ifndef SAVE_MEMORY_SOUND
                    setSound(SoundIndex::Invader_Player_Explosion);
#endif

                } else {
                    invader_HasBulletHitBarrier(general2, bullet);
                }
            }

            if(bullet.y >= 64) {
                bullet.y = -4;
            }
        }
    }
}

void invader_PlayGame() {
    auto pressed = arduboy.pressedButtons();

    Invader_General& general = level.getItem(Constants::Invaders_General).data.invader_General;
    Invader_General2& general2 = level.getItem(Constants::Invaders_General2).data.invader_General2;
    Invader_Player& player = level.getItem(Constants::Invaders_Player).data.invader_Player;
    Invader_Bullet& bullet = level.getItem(Constants::Invaders_Player_Bullet).data.invader_Bullet;

    // Update bullets ..

    invader_UpdateEnemy(general, general2, player);
    invader_UpdatePlayer(general, general2, player);
    invader_UpdateEnemyBullets(general2, player);
    invader_EnemyDropsBullet(general2, player);

    if(bullet.y > -4) {
        bullet.y = bullet.y - 2;
    }

    invader_DetectPlayerBulletHit(general, general2, bullet);

    // if enemies reappearing, them move into position ..

    if(general2.appearCountdown > 0) {
        if(arduboy.getFrameCount(2) == 0) {
            invader_MoveEnemies_Down(player);
            general2.appearCountdown--;

            if(general2.appearCountdown == 0) {
                player.status = Status::Active;
                arduboy.setFrameCount(5);
            }
        }
    }

    // Handle movements ..

    if(player.status <= Status::Safe || player.status == Status::EnemiesAppearing) {
        if(pressed & LEFT_BUTTON && player.x > 4) {
            player.status = Status::Active;
            player.x--;
        }

        if(pressed & RIGHT_BUTTON && player.x < 104) {
            player.status = Status::Active;
            player.x++;
        }

        if(pressed & B_BUTTON && player.status != Status::EnemiesAppearing &&
           general2.bulletPlayerCountdown == 0) {
            player.status = Status::Active;

            if(bullet.y == -4) {
#ifndef SAVE_MEMORY_SOUND
                setSound(SoundIndex::Invader_Player_Fires_Bullet);
#endif

                bullet.x = player.x + 4;
                bullet.y = player.y - 2;
            }
        }
    }

    invader_RenderBarriers();
    invader_RenderEnemies();

    if(player.status != Status::Dead || general.lives != 0) {
        invader_RenderPlayer(player);
    }

    invader_RenderPlayerBullet(bullet);
    invader_RenderEnemyBullets();
    invader_RenderHUD(general);

    switch(player.status) {
    case Status::Active:

        if(invader_EnemiesAlive() == 0) {
            if(general2.launchOffset < 18) general2.launchOffset = general2.launchOffset + 2;
            invader_NewWave(general2);
            player.status = Status::EnemiesAppearing;
            general2.appearCountdown = 24;
            general2.speed = 17;

#ifndef SAVE_MEMORY_SOUND
            setSound(SoundIndex::Invader_Wave_Success);
#endif
        }

        break;

    case Status::Safe:

        switch(general.lives) {
        case 3:
            FX::drawBitmap(
                35, 25, Images::Levels, arduboy.getFrameCountHalf(96) ? 3 : 4, dbmMasked);
            break;

        default:
            FX::drawBitmap(35, 25, Images::Levels, general.lives, dbmMasked);
            break;
        }

        break;

    case Status::Dead:

        if(general2.deathCountdown > 0) {
            general2.deathCountdown--;
        } else {
            if(general.lives > 0) {
                player.status = Status::Safe;

            } else {
                FX::drawBitmap(35, 25, Images::Levels, general.lives, dbmMasked);
            }
        }

        break;

    default:
        break;
    }

    // Housekeeping ..

    if(general2.bulletPlayerCountdown > 0) general2.bulletPlayerCountdown--;

    // Has the enemy hit the player ?  If so, game over ..

    Rect playerRect = {player.x, player.y, 9, 9};

    for(uint8_t x = Constants::Invaders_Enemy_Row_1_Start;
        x <= Constants::Invaders_Enemy_Row_3_End;
        x++) {
        Invader_Enemy& enemy = level.getItem(x).data.invader_Enemy;

        if(enemy.status == Status::Active) {
            Rect enemyRect = {enemy.x, enemy.y, 8, 7};

            if(arduboy.collide(playerRect, enemyRect)) {
                enemy.status = Status::Exploding1;
                player.status = Status::Exploding1;

                general.lives = 0;

#ifndef SAVE_MEMORY_SOUND
                setSound(SoundIndex::Invader_End_of_Game);
#endif
            }
        }
    }
}

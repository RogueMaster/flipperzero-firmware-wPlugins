#include "src/utils/Arduboy2Ext.h"
#include <lib/ArduboyFX.h>

#include "src/utils/Constants.h"
#include "src/utils/Stack.h"
#include "src/entities/GamePlay.h"
#include "src/entities/Entities.h"
#include "src/fonts/Font3x5.h"

// Returns true if we just entered a new room.

bool testScroll(GamePlay& gamePlay, Prince& prince, Level& level) {
    bool result = false;

    // Have we scrolled to another screen ?

    if(prince.getY() - level.getYOffset() >= 56 + Constants::TileHeight) {
        if(gamePlay.level == 6 && level.getYLocation() == 6) {
            prince.clear();
            prince.push(Stance::Leave_Gate_14_End);
            level.setYLocation(12);

        } else {
            prince.incY(-Constants::TileHeight * 3);
            level.setYLocation(level.getYLocation() + 3);
            level.loadMap(gamePlay);
            level.setYOffset(0);
            level.setYOffsetDir(Direction::None);

            result = true;
        }

    } else if(static_cast<int8_t>(prince.getY() - level.getYOffset()) < 0) {
        prince.incY(Constants::TileHeight * 3);
        level.setYLocation(level.getYLocation() - 3);
        level.loadMap(gamePlay);
        level.setYOffset(Constants::TileHeight);
        level.setYOffsetDir(Direction::None);

        result = true;

    } else if(prince.getX() < 0) {
        prince.incX(Constants::TileWidth * Constants::ScreenWidthInTiles);
        level.setXLocation(level.getXLocation() - 10);
        level.loadMap(gamePlay);

        if(gamePlay.level == 13 && level.getXLocation() == 0 && level.getYLocation() == 0) {
            gamePlay.gameState = GameState::Title;
            cookie.setMode(TitleScreenMode::CutScene_End);

            // Has a new high score been set?  If so, save it.

            if((cookie.highMin < gamePlay.timer_Min) ||
               ((cookie.highMin == gamePlay.timer_Min) && (cookie.highSec < gamePlay.timer_Sec))) {
                cookie.hasSavedScore = true;
                cookie.highMin = gamePlay.timer_Min;
                cookie.highSec = gamePlay.timer_Sec;
                cookie.highSaves = gamePlay.saves;
                cookie.hasSavedLevel = false;
                saveCookie(false);
            }

#ifndef SAVE_MEMORY_SOUND
            setSound(SoundIndex::Victory);
#endif
        }

        result = true;

    } else if(prince.getX() > Constants::TileWidth * Constants::ScreenWidthInTiles) {
        prince.incX(-Constants::TileWidth * Constants::ScreenWidthInTiles);
        level.setXLocation(level.getXLocation() + 10);
        level.loadMap(gamePlay);

        if(gamePlay.level == 8 && level.getXLocation() == 40 && level.getYLocation() == 3 &&
           mouse.counter == 0) {
            mouse.exits++;

            if(mouse.exits > 2) {
                Item gate = level.getItemByIndex(ItemType::Gate, ItemType::Gate_StayOpen, 6);

                if(gate.data.gate.position == 0) {
                    mouse.counter = arduboy.randomLFSR(170, 240);
                }
            }
        }

        result = true;
    }

    // Calculate screen offset ..

    if(prince.getYPrevious() <= 56 && prince.getY() > 56) {
        level.setYOffsetDir(Direction::Down);
    } else if(prince.getYPrevious() > 56 && prince.getY() <= 56) {
        level.setYOffsetDir(Direction::Up);
    }

    return result;
}

void pushSequence() {
    uint16_t s1 = FX::readPendingUInt16();
    uint16_t s2 = FX::readPendingUInt16();
    uint16_t s3 = FX::readPendingUInt16();

    if(s1 != Stance::None) {
        prince.pushSequence(s1, s2, s3);
    }
}

void processJump(uint24_t pos) {
    FX::seekData(pos);
    pushSequence();
    pushSequence();
    uint16_t collision = FX::readPendingUInt16();

    if(collision == 1) {
        prince.setIgnoreWallCollisions(true);
    }

    uint16_t counter = FX::readPendingLastUInt16();

    if(counter > 0) {
        prince.setHangingCounter(static_cast<uint8_t>(counter));
    }
}

void processRunJump(Prince& prince, Level& level, bool testEnemy) {
    RunningJumpResult jumpResult = RunningJumpResult::None;

#ifndef SAVE_MEMORY_ENEMY

    if(testEnemy && enemy.getHealth() > 0) {
        int16_t xDelta = prince.getPosition().x - enemy.getPosition().x;

        if(xDelta > -30 && xDelta < 30) {
            if(enemy.getPosition().x < prince.getPosition().x && xDelta >= 0)
                goto goodToGo; // Attempting to jump while running away to the right ..
            if(enemy.getPosition().x > prince.getPosition().x && xDelta <= 0)
                goto goodToGo; // Attempting to jump while running away to the left ..

            prince.pushSequence(Stance::Run_Repeat_1_Start, Stance::Run_Repeat_4);
            return;
        }
    }

#endif

goodToGo:

    // Level 6 exit ..

    if(gamePlay.level == 6 && level.getXLocation() == 0) {
        jumpResult = level.canRunningJump(prince, Action::RunJump_Level6Exit);

    } else {
        jumpResult = level.canRunningJump(prince, Action::RunJump_Normal);
    }

    if(jumpResult != RunningJumpResult::KeepRunning) {
        uint24_t pos =
            RunningJumpStances + static_cast<uint24_t>(static_cast<uint16_t>(jumpResult) * 16);
        processJump(pos);

    } else {
        prince.pushSequence(Stance::Run_Repeat_5_Mid, Stance::Run_Repeat_8_End);
    }
}

void processStandingJump(Prince& prince, Level& level) {
    StandingJumpResult standingJumpResult = level.canStandingJump(prince);

    uint24_t pos = StandingJumpStances +
                   static_cast<uint24_t>(static_cast<uint16_t>(standingJumpResult) * 16);
    processJump(pos);
}

void initFlash(Prince& prince, Level& level, FlashType flashType) {
    Flash& flash = level.getFlash();
    flash.frame = 7;
    flash.type = flashType;
    flash.x = level.coordToTileIndexX(prince.getX()) + level.getXLocation();
    flash.y = level.coordToTileIndexY(prince.getY()) + level.getYLocation();
}

void initFlash(Enemy& enemy, Level& level, FlashType flashType) {
    Flash& flash = level.getFlash();
    flash.frame = 5;
    flash.type = flashType;
    flash.x = level.coordToTileIndexX(enemy.getX());
    flash.y = level.coordToTileIndexY(enemy.getY());
}

uint8_t activateSpikes(Prince& prince, Level& level) {
    int8_t tileXIdx = level.coordToTileIndexX(prince.getPosition().x);
    int8_t tileYIdx = level.coordToTileIndexY(prince.getPosition().y);
    uint8_t itemIdx = level.getItem(ItemType::Spikes, tileXIdx, tileYIdx);

    // Withdraw spikes after a few second..

    if(itemIdx != Constants::NoItemFound) {
        Item& spikes = level.getItem(itemIdx);
        activateSpikes_Helper(spikes);
    }

    uint8_t itemIdx2 = level.getItem(ItemType::Spikes, tileXIdx - 1, tileYIdx);

    if(itemIdx2 != Constants::NoItemFound) {
        Item& spikes = level.getItem(itemIdx2);
        activateSpikes_Helper(spikes);
    }

    itemIdx2 = level.getItem(ItemType::Spikes, tileXIdx + 1, tileYIdx);

    if(itemIdx2 != Constants::NoItemFound) {
        Item& spikes = level.getItem(itemIdx2);
        activateSpikes_Helper(spikes);
    }

    return itemIdx;
}

void activateSpikes_Helper(Item& spikes) {
    if(spikes.data.spikes.closingDelay == 0) {
        spikes.data.spikes.closingDelay = Constants::SpikeClosingDelay;
    }
}

void pushJumpUp_Drop(Prince& prince) {
    prince.pushSequence(
        Stance::Jump_Up_Drop_A_1_Start, Stance::Jump_Up_Drop_A_5_End, Stance::Upright);
    prince.pushSequence(Stance::Jump_Up_A_1_Start, Stance::Jump_Up_A_14_End);
}

bool leaveLevel(Prince& prince, Level& level) {
    int8_t tileXIdx = level.coordToTileIndexX(prince.getPosition().x);
    int8_t tileYIdx = level.coordToTileIndexY(prince.getPosition().y);

    Item& exitGate = level.getItem(Constants::Item_ExitDoor);

    // Are we close to the exist gate?  If so, exit scene ..

    if(tileXIdx == exitGate.data.location.x - 1 && tileYIdx == exitGate.data.location.y &&
       exitGate.data.exitDoor.direction == Direction::Up) {
        switch(prince.getDirection()) {
        case Direction::Left:

            prince.pushSequence(
                Stance::Standing_Turn_1_Start, Stance::Standing_Turn_5_End, Stance::Upright_Turn);
            break;

        case Direction::Right:

            prince.pushSequence(Stance::Leave_Gate_01_Start, Stance::Leave_Gate_14_End);

            switch(level.distToEdgeOfTile(
                prince.getDirection(),
                (level.getXLocation() * Constants::TileWidth) + prince.getX())) {
            case 2:
                prince.pushSequence(Stance::Small_Step_1_Start, Stance::Small_Step_6_End);
                break;

            case 6:
                prince.pushSequence(Stance::Single_Step_2, Stance::Single_Step_8_End);
                break;

            case 10:
                prince.pushSequence(Stance::Small_Step_1_Start, Stance::Small_Step_6_End);
                prince.pushSequence(Stance::Single_Step_2, Stance::Single_Step_8_End);
                break;

            default:
                break;
            }

            return true;

        default:
            break;
        }

        return true;

    } else if(
        tileXIdx == exitGate.data.location.x && tileYIdx == exitGate.data.location.y &&
        exitGate.data.exitDoor.direction == Direction::Up) {
        prince.pushSequence(
            Stance::Standing_Turn_1_Start, Stance::Standing_Turn_5_End, Stance::Upright_Turn);

        switch(prince.getDirection()) {
        case Direction::Left:

            switch(level.distToEdgeOfTile(
                prince.getDirection(),
                (level.getXLocation() * Constants::TileWidth) + prince.getX())) {
            case 6:
                prince.pushSequence(Stance::Small_Step_1_Start, Stance::Small_Step_6_End);
                break;

            case 10:
                prince.pushSequence(Stance::Single_Step_1_Start, Stance::Single_Step_8_End);
                break;

            default:
                break;
            }

            break;

        case Direction::Right:

            prince.pushSequence(Stance::Leave_Gate_01_Start, Stance::Leave_Gate_14_End);

            switch(level.distToEdgeOfTile(
                prince.getDirection(),
                (level.getXLocation() * Constants::TileWidth) + prince.getX())) {
            case 2:
                prince.pushSequence(Stance::Single_Step_8_End, Stance::Single_Step_1_Start);
                break;

            case 6:
                prince.pushSequence(Stance::Small_Step_6_End, Stance::Small_Step_1_Start);
                break;

            default:
                break;
            }

            break;

        default:
            break;
        }

        return true;
    }

    return false;
}

void pushDead(Prince& entity, Level& level, GamePlay& gamePlay, bool clear, DeathType deathType) {
#ifndef SAVE_MEMORY_SOUND
    setSound(SoundIndex::Dead);
#endif

    if(clear) prince.clear();

    switch(deathType) {
    case DeathType::Falling:
        entity.pushSequence(Stance::Falling_Dead_1_Start, Stance::Falling_Dead_3_End);
        break;

    default: // case DeathType::Blade, Spikes, SwordFight
        entity.pushSequence(Stance::Falling_Dead_Blade_1_Start, Stance::Falling_Dead_Blade_2_End);
        break;
    }

    entity.setHealth(0);

    //if (gamePlay.isGameOver()) { // in case GameOver Gamestate is changed and end text is shown instead

    //    showSign(entity, level, SignType::GameOver);
    //
    //}
    //else {
    //
    //    showSign(entity, level, SignType::PressA);
    //
    //}
    showSign(entity, level);
}

void pushDead(Enemy& entity, bool clear) {
#ifndef SAVE_MEMORY_SOUND
    setSound(SoundIndex::Triumph);
#endif

    if(clear) entity.clear();
    entity.pushSequence(Stance::Falling_Dead_1_Start, Stance::Falling_Dead_3_End);
    entity.setHealth(0);
    entity.setMoveCount(64);
}

//void showSign(Prince &prince, Level &level, SignType signType) {
void showSign(Prince& prince, Level& level) {
    Sign& sign = level.getSign();
    //sign.type = signType;
    sign.counter = 20;

    switch(prince.getY()) {
    case 25:
        sign.y = 48;
        break;

    case 56:
    case 87:
        sign.y = 1;
        break;
    }
}

void playGrab() {
#ifndef SAVE_MEMORY_SOUND
    setSound(static_cast<SoundIndex>(gamePlay.getGrab() + 1));
#endif
}

#ifndef SAVE_MEMORY_ENEMY

void isEnemyVisible(
    Prince& prince,
    bool swapEnemies,
    bool& isVisible,
    bool& sameLevelAsPrince,
    bool justEnteredRoom) { // Should we swap active enemies (no if stack is not empty)

    bool enemyIsVisible = false;
    bool onSameLevelAsPrince = false;

    uint8_t currentEnemy = enemy.getActiveEnemy();

    for(uint8_t i = 0; i < enemy.getEnemyCount(); i++) {
        enemy.setActiveEnemy(i);

        if(enemy.getStatus() == Status::Active || enemy.getStatus() == Status::Dormant ||
           enemy.getStatus() == Status::Dormant_ActionReady) {
            uint8_t tileXIdx = level.coordToTileIndexX(enemy.getPosition().x);
            uint8_t tileYIdx = level.coordToTileIndexY(enemy.getPosition().y);

            if(tileXIdx >= level.getXLocation() &&
               tileXIdx < level.getXLocation() + Constants::ScreenWidthInTiles &&
               tileYIdx >= level.getYLocation() &&
               tileYIdx < level.getYLocation() + Constants::ScreenHeightInTiles) {
                enemyIsVisible = true;

                int8_t princeTileYIdx = level.coordToTileIndexY(prince.getPosition().y);

                if(princeTileYIdx == tileYIdx) {
                    onSameLevelAsPrince = true;
                }
            }

            if(enemy.getHealth() == 0 && enemy.getMoveCount() > 0) {
                enemy.decMoveCount();
            }
        }

        if(enemyIsVisible) break;
    }

    if(!enemyIsVisible || !swapEnemies) {
        if(justEnteredRoom) {
            enemy.clear();
        } else {
            enemy.setActiveEnemy(currentEnemy);
        }
    }

    if(justEnteredRoom && enemyIsVisible) {
        BaseEntity& base = enemy.getActiveBase();
        int16_t xDelta = prince.getPosition().x - base.getPosition().x;

        if(base.getHealth() > 0) {
            enemy.getPosition().x = enemy.getX();
            enemy.clear();
            enemy.setStance(Stance::Upright);

            if(xDelta < 0) {
                enemy.setX((level.getXLocation() * Constants::TileWidth) + base.getX_LeftEntry());
                enemy.setDirection(Direction::Left);

            } else {
                enemy.setX((level.getXLocation() * Constants::TileWidth) + base.getX_RightEntry());
                enemy.setDirection(Direction::Right);
            }
        }
    }

    isVisible = enemyIsVisible;
    sameLevelAsPrince = isVisible ? onSameLevelAsPrince : false;
}

#endif

void fixPosition() {
    switch(level.distToEdgeOfTile(
        prince.getDirection(), (level.getXLocation() * Constants::TileWidth) + prince.getX())) {
    case 0:
        prince.incX(2);
        break;

    case 1:
    case 5:
    case 9:
        prince.incX(1);
        break;

    case 3:
    case 7:
    case 11:
        prince.incX(-1);
        break;

    case 2:
    case 6:
    case 10:
        break;

    case 4:
    case 8:
        prince.incX(2);
        break;
    }

    prince.updateLocation(level.getXLocation(), level.getYLocation());
}

uint8_t getImageIndexFromStance(uint16_t stance) {
    FX::seekData(Constants::StanceToImageXRefFX + stance);
    uint8_t image = FX::readEnd();

    return image;
}

void getStance_Offsets(Direction direction, Point& offset, int16_t stance) {
    FX::seekData(Constants::Stance_XYOffsetsFX + ((stance - 1) * 2));
    offset.x = static_cast<int8_t>(FX::readPendingUInt8()) *
               (direction == Direction::Left ? -1 : 1) * (stance < 0 ? -1 : 1);
    ;
    offset.y = static_cast<int8_t>(FX::readEnd()) * (stance < 0 ? -1 : 1);
}

void processRunningTurn() {
    if(level.canMoveForward(prince, Action::RunningTurn, prince.getDirection(), 0)) {
        prince.pushSequence(
            Stance::Running_Turn_1_Start,
            Stance::Running_Turn_13_End,
            Stance::Run_Repeat_8_End_Turn);

    } else {
        prince.pushSequence(Stance::Stopping_1_Start, Stance::Stopping_5_End, Stance::Upright);
    }
}

void saveCookie(bool enableLEDs) {
#ifdef USE_LED
    if(enableLEDs) {
#ifndef MICROCADE
        arduboy.setRGBled(RED_LED, 32);
#else
        arduboy.setRGBledGreenOff();
        arduboy.setRGBledBlueOff();
#endif
    }
#endif

    FX::saveGameState(cookie);

#ifdef USE_LED
    if(enableLEDs) {
#ifndef MICROCADE
        arduboy.setRGBled(RED_LED, 0);
        arduboy.setRGBled(GREEN_LED, 32);
#else
        arduboy.setRGBledRedOff();
        arduboy.setRGBledGreenOn();
#endif
    }
#endif
}

void bindRuntimeStacks() {
    if(prince.getStack() != &princeStack) {
        prince.setStack(&princeStack);
    }

#ifndef SAVE_MEMORY_ENEMY
    if(enemy.getStack() != &enemyStack) {
        enemy.setStack(&enemyStack);
    }
#endif
}

void restoreRuntimeAfterLoad() {
    bindRuntimeStacks();
    prince.clear();
    prince.push(prince.getStance());
    prince.updateLocation(level.getXLocation(), level.getYLocation());

#ifndef SAVE_MEMORY_ENEMY
    enemy.clear();
    if(enemy.getEnemyCount() > 0) {
        enemy.push(enemy.getStance());
    }
#endif
}

#ifndef SAVE_MEMORY_SOUND

void setSound(SoundIndex index) {
    bool playSound = true;

    switch(index) {
    case SoundIndex::Override_Start... SoundIndex::Overrride_End:
        break;

    case SoundIndex::Suppress_Start... SoundIndex::Suppress_End:

        playSound = !sound.playing();
        break;
    }

    if(playSound) {
        switch(prince.getStance()) {
        case Stance::Falling_Dead_1_Start... Stance::Falling_Dead_3_End:
        case Stance::Falling_Dead_Blade_1_Start... Stance::Falling_Dead_Blade_2_End:

            switch(index) {
            case SoundIndex::Suppress_Start... SoundIndex::Suppress_End:
                playSound = false;

            default:
                break;
            }

        default:
            break;
        }
    }

    if(playSound) {
        sound.tonesFromFX(FX::readIndexedUInt24(Sounds::Table, (uint8_t)index));
    }
}

#endif

void handleBlades() {
    uint8_t princeLX;
    uint8_t princeRX;

    int8_t tileXIdx = level.coordToTileIndexX(prince.getPosition().x);
    int8_t tileYIdx = level.coordToTileIndexY(prince.getPosition().y);

    if(prince.getDirection() == Direction::Right) {
        tileXIdx++;
    }

    ImageDetails imageDetails;
    prince.getImageDetails(imageDetails);

    princeLX = prince.getX() + imageDetails.reach;
    princeRX = prince.getX() + imageDetails.heel;

    if(princeLX == 0 && princeRX != 0) princeLX = princeRX;
    if(princeLX != 0 && princeRX == 0) princeRX = princeLX;

    if(princeRX < princeLX) {
        uint8_t temp = princeRX;
        princeRX = princeLX;
        princeLX = temp;
    }

    handleBlade_Single(tileXIdx, tileYIdx, princeLX, princeRX);
    handleBlade_Single(tileXIdx - 1, tileYIdx, princeLX, princeRX);
}

void handleBlade_Single(int8_t tileXIdx, int8_t tileYIdx, uint8_t princeLX, uint8_t princeRX) {
    uint8_t itemIdx = level.getItem(ItemType::Blade, tileXIdx, tileYIdx);

    if(itemIdx != Constants::NoItemFound) {
        int8_t chopperX = ((tileXIdx - level.getXLocation()) * Constants::TileWidth) + 3;
        Item& item = level.getItem(itemIdx);

        if(chopperX > princeLX && chopperX < princeRX && abs(item.data.blade.position) <= 5) {
#if defined(DEBUG) && defined(DEBUG_PUSH_DEAD)
            DEBUG_PRINTLN(F("pushDead 15"));
#endif

            pushDead(prince, level, gamePlay, true, DeathType::Blade);
        }
    }
}

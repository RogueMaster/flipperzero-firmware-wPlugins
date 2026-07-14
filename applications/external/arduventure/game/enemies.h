#ifndef ENEMIES_H
#define ENEMIES_H

#include "globals.h"

/* Enemy Data
 *  Element type determines head image.
 *  Attack, defense, and 
 */

//byte enemyAmount = 3;

//////// BOSS functions ////////////////////
////////////////////////////////////////////
struct Enemies {
    byte type;
    int health;
    byte defense;
    byte specDefense;
    byte attack;
    byte images;
    byte level;
    byte speed;
    byte defendsLeft;
};

//Enemies enemy[5];
Enemies enemy; // only one at a time for now

/*
 * Create an enemy based on the given player level.
 * Can also specify the enemies level directly as well
 * as it's stat type and element type.
 * 
 * If lvl is given, stattype, and type must also be given.
 * 
 * takes:
 *  player_level  - the level of the player to base the monsters stats off of.
 *  lvl           - override the level of the monster.
 *  stattype      - override the stat typing of the monster.
 *    0: neutral, 1: offense, 2: defense.
 *  type          - override the element type of the monster.
 *    0: normal, 1: water, 2: leaf, 3: fire.
 *    
 * returns void.
 * 
 * Level Ranges:
 *  Area 1:
 *  1-5
 *  Area 2:
 *  6-13
 *  Area 3:
 *  11-18
 *  Area 4:
 *  16-23
 */
void createEnemy(byte player_level, byte lvl = 0, byte stattype = 4, byte type = 4) {
    byte region = player.currentRegion - REGION_YOUR_GARDEN; // 1-4 (in)
    byte lvlRange = (region) * 2; // 2-8 (in)
    byte monster = generateRandomNumber(min(7, lvlRange)); // 0-6 (in)
    byte statType = stattype;
    // get random level offset
    enemy.level = lvl;
    enemy.type = type;
    enemy.defendsLeft = 4;
    if(lvl == 0) {
        //enemy.level = generateRandomNumber(lvlRange); // 0-7
        if(region == 1)
            enemy.level = generateRandomNumber(min(player_level, 5)); // 0-4
        else
            enemy.level = generateRandomNumber(lvlRange); // 0-7
        statType = generateRandomNumber(3);
        enemy.level++; // 1-8 areas 2-4, 1-5 area 1
        --region; // 0-3
        enemy.level += region * 6; // 1-26
        enemy.type = monster / 2;
        enemy.images = ((enemy.level / 7) + statType * 4) | (monster << 4);
    } else
        --region;

    enemy.defense = 3;
    enemy.specDefense = 3;
    enemy.health = 3;
    enemy.attack = 6;
    enemy.speed = 5;
    enemy.health *= enemy.level;
    enemy.defense *= enemy.level;
    enemy.specDefense *= enemy.level;
    enemy.attack *= enemy.level;
    enemy.speed *= enemy.level;
    switch(statType) {
    /*case 0: // neutral
    enemy.defense = 3;
    enemy.specDefense = 3;
    enemy.health = 3;
    enemy.attack = 3;
    enemy.speed = 5;
    break;*/
    case 1: // high specDefense, high attack, low defense and health
        enemy.defense -= 2 * region;
        enemy.specDefense += 2 * region;
        enemy.health -= 2 * region;
        enemy.attack += 2 * region;
        enemy.speed += 2 * region;
        break;
    case 2: // high defense and health, low specDefense and attack
        enemy.defense += 2 * region;
        enemy.specDefense -= 2 * region;
        enemy.health += 2 * region;
        enemy.attack -= 2 * region;
        enemy.speed -= 2 * region;
        break;
    }
}

void damageEnemy(
    uint16_t player_attack,
    uint16_t player_attack_addition,
    uint16_t player_level,
    bool magic = false) {
    //lastDamageDealt = (byte)min(max((player_attack + player_attack_addition) * player_level / ((magic == false) ? enemy.defense : enemy.specDefense), 1), 255);
    int dmg = (player_attack + player_attack_addition) * player_level /
              ((magic == false) ? enemy.defense : enemy.specDefense);
    lastDamageDealt = (((dmg < 1) ? 1 : dmg) > 255) ? 255 : dmg;
    int16_t ehp = (int16_t)enemy.health - lastDamageDealt;
    enemy.health = max(ehp, 0);
}

void drawEnemies(int8_t yoffset) {
    sprites.drawOverwrite(57, 16 + yoffset, enemyHeads, enemy.images & 0x0F);
    sprites.drawOverwrite(56, 24 + yoffset, enemyFeet, enemy.images >> 4);
}

byte getEnemyName() {
    if(!isBoss)
        return (enemy.images >> 4) + 221;
    else
        return (player.lastDoor + 210);
}

/*void updateEnemies()
{
  
}*/

void drawBoss(int8_t yoffset) {
    sprites.drawOverwrite(52, 16 + yoffset, bossSprites, enemy.images);
}

#endif

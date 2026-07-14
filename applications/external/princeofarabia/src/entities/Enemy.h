#pragma once

#include <lib/Arduboy2.h>
#include "../utils/Constants.h"
#include "../utils/Stack.h"
#include "../entities/Structs.h"
#include "BaseEntity.h"
#include "BaseStack.h"

class Enemy : public BaseStack {
private:
    BaseEntity base[Constants::EnemyCount];

    uint8_t count = 0;
    uint8_t activeEnemy = 0;
    uint8_t moveCount = 0;
    Direction moveDirection = Direction::Forward;

public:
    uint8_t getEnemyCount() {
        return this->count;
    }
    uint8_t getMoveCount() {
        return this->moveCount;
    }
    uint8_t getActiveEnemy() {
        return this->activeEnemy;
    }
    Direction getMoveDirection() {
        return this->moveDirection;
    }

    void setEnemyCount(uint8_t val) {
        this->count = val;
    }
    void setMoveCount(uint8_t val) {
        this->moveCount = val;
    }
    void setActiveEnemy(uint8_t val) {
        this->activeEnemy = val;
    }
    void setMoveDirection(Direction val) {
        this->moveDirection = val;
    }

    void decMoveCount() {
        if(this->moveCount > 0) this->moveCount--;
    }

    void init() {
        this->count = 0;
    }

    void init(
        EnemyType enemyType,
        int16_t x,
        uint8_t x_Tile,
        uint8_t x_LeftEntry,
        uint8_t x_RightEntry,
        uint8_t x_LeftExtent,
        uint8_t x_RightExtent,
        int16_t y,
        Direction direction,
        uint16_t stance,
        uint8_t health,
        Status status) {
        this->base[this->count].setEnemyType(enemyType);
        this->base[this->count].setX(x);
        this->base[this->count].setX_Tile(x_Tile);
        this->base[this->count].setX_LeftEntry(x_LeftEntry);
        this->base[this->count].setX_RightEntry(x_RightEntry);
        this->base[this->count].setX_LeftExtent(x_LeftExtent);
        this->base[this->count].setX_RightExtent(x_RightExtent);
        this->base[this->count].setY(y);
        this->base[this->count].setDirection(direction);
        this->base[this->count].setStance(stance);
        this->base[this->count].setPrevStance(stance);
        this->base[this->count].setHealth(health);
        this->base[this->count].setHealthMax(health);
        this->base[this->count].setStatus(status);
        this->count++;
    }

    void update() {
        this->stack->update();

        for(uint8_t i = 0; i < Constants::EnemyCount; i++) {
            this->base[i].getPosition().x = this->base[i].getX();
            this->base[i].getPosition().y = this->base[i].getY();
        }
    }

    uint16_t getStance() {
        return this->base[this->activeEnemy].getStance();
    }
    uint16_t getPrevStance() {
        return this->base[this->activeEnemy].getPrevStance();
    }
    int16_t getX() {
        return this->base[this->activeEnemy].getX();
    }
    int16_t getY() {
        return this->base[this->activeEnemy].getY();
    }
    int16_t getYPrevious() {
        return this->base[this->activeEnemy].getYPrevious();
    }
    int16_t getXImage() {
        return this->base[this->activeEnemy].getXImage();
    }
    int16_t getYImage() {
        return this->base[this->activeEnemy].getYImage();
    }
    uint8_t getHealthMax() {
        return this->base[this->activeEnemy].getHealthMax();
    }
    Status getStatus() {
        return this->base[this->activeEnemy].getStatus();
    }
    EnemyType getEnemyType() {
        return this->base[this->activeEnemy].getEnemyType();
    }

    Direction getDirection() {
        return this->base[this->activeEnemy].getDirection();
    }

    void setStance(uint16_t val) {
        this->base[this->activeEnemy].setStance(val);
    }
    void setPrevStance(uint16_t val) {
        this->base[this->activeEnemy].setPrevStance(val);
    }
    void setX(int16_t val) {
        this->base[this->activeEnemy].setX(val);
    }
    void setY(int16_t val) {
        this->base[this->activeEnemy].setY(val);
    }
    void setYPrevious(int16_t val) {
        this->base[this->activeEnemy].setYPrevious(val);
    }
    void setDirection(Direction val) {
        this->base[this->activeEnemy].setDirection(val);
    }
    void setHealth(uint8_t val) {
        this->base[this->activeEnemy].setHealth(val);
    }
    void setHealthMax(uint8_t val) {
        this->base[this->activeEnemy].setHealthMax(val);
    }
    void setStatus(Status val) {
        this->base[this->activeEnemy].setStatus(val);
    }
    void setEnemyType(EnemyType val) {
        this->base[this->activeEnemy].setEnemyType(val);
    }

    uint8_t decHealth(uint8_t val) {
        return this->base[this->activeEnemy].decHealth(val);
    }
    void incHealth(int8_t val) {
        this->base[this->activeEnemy].incHealth(val);
    }

    Point& getPosition() {
        return this->base[this->activeEnemy].getPosition();
    }
    uint8_t getHealth() {
        return this->base[this->activeEnemy].getHealth();
    }

    BaseEntity& getActiveBase() {
        return this->base[this->activeEnemy];
    }

    Direction getOppositeDirection() {
        return this->base[this->activeEnemy].getOppositeDirection();
    }

    int8_t getDirectionOffset(int8_t val) {
        return this->base[this->activeEnemy].getDirectionOffset(val);
    }

    void incX(int8_t inc) {
        this->base[this->activeEnemy].incX(inc);
    }

    void incY(int8_t inc) {
        this->base[this->activeEnemy].incY(inc);
    }

    void changeDirection() {
        this->base[this->activeEnemy].changeDirection();
    }

    bool activateEnemy(uint8_t x, uint8_t y) {
        for(uint8_t i = 0; i < this->getEnemyCount(); i++) {
            if((this->base[i].getStatus() == Status::Dormant) &&
               (this->base[i].getX() / Constants::TileWidth == x) &&
               (this->base[i].getY() / Constants::TileHeight == y)) {
                this->base[i].setStatus(Status::Active);
                return true;
            }
        }

        return false;
    }
};

#pragma once

#include <lib/Arduboy2.h>
#include "../utils/Constants.h"
#include "../utils/Stack.h"
#include "../entities/Structs.h"

class BaseEntity {
protected:
    uint16_t stance = Stance::Upright;
    uint16_t prevStance = Stance::Upright;
    Direction direction = Direction::Left;

    int16_t x = 0;
    int16_t y = 0;
    int16_t prevY = 0;

    uint8_t x_Tile = 0;
    uint8_t x_LeftEntry = 0;
    uint8_t x_RightEntry = 0;
    uint8_t x_LeftExtent = 0;
    uint8_t x_RightExtent = 0;

    uint8_t health = 0;
    uint8_t healthMax = 0;
    Status status; // Active or Dormant (for skeletons), can be active but dead (health = 0)
    EnemyType enemyType;

    Point location;

public:
    uint16_t getStance() {
        return this->stance;
    }
    uint16_t getPrevStance() {
        return this->prevStance;
    }
    int16_t getX() {
        return this->x;
    }
    uint8_t getX_Tile() {
        return this->x_Tile;
    }
    uint8_t getX_LeftEntry() {
        return this->x_LeftEntry;
    }
    uint8_t getX_RightEntry() {
        return this->x_RightEntry;
    }
    uint8_t getX_LeftExtent() {
        return this->x_LeftExtent;
    }
    uint8_t getX_RightExtent() {
        return this->x_RightExtent;
    }
    int16_t getY() {
        return this->y;
    }
    int16_t getYPrevious() {
        return this->prevY;
    }
    int16_t getXImage() {
        return this->x - 18 + 3;
    } // Image is 36 x 36 hence - 18, the plus 3 is due to the orthagonal tiles offset.
    int16_t getYImage() {
        return this->y - 31;
    } // -31 moves the player up 5 pixels on the orthagonal tiles ?
    uint8_t getHealth() {
        return this->health;
    }
    uint8_t getHealthMax() {
        return this->healthMax;
    }

    Status getStatus() {
        return this->status;
    }
    EnemyType getEnemyType() {
        return this->enemyType;
    }

    Direction getDirection() {
        return this->direction;
    }

    void setStance(uint16_t val) {
        this->stance = val;
    }
    void setPrevStance(uint16_t val) {
        this->prevStance = val;
    }
    void setX(int16_t val) {
        this->x = val;
    }
    void setX_Tile(uint8_t val) {
        this->x_Tile = val;
    }
    void setX_LeftEntry(uint8_t val) {
        this->x_LeftEntry = val;
    }
    void setX_RightEntry(uint8_t val) {
        this->x_RightEntry = val;
    }
    void setX_LeftExtent(uint8_t val) {
        this->x_LeftExtent = val;
    }
    void setX_RightExtent(uint8_t val) {
        this->x_RightExtent = val;
    }
    void setY(int16_t val) {
        this->y = val;
    }
    void setYPrevious(int16_t val) {
        this->prevY = val;
    }
    void setDirection(Direction val) {
        this->direction = val;
    }
    void setHealth(uint8_t val) {
        this->health = val;
    }
    void setHealthMax(uint8_t val) {
        this->healthMax = val;
    }
    void setStatus(Status val) {
        this->status = val;
    }
    void setEnemyType(EnemyType val) {
        this->enemyType = val;
    }

    uint8_t decHealth(uint8_t val) {
        this->health >= val ? this->health = this->health - val : 0;
        return this->health;
    }
    void incHealth(int8_t val) {
        this->health = this->health + val > this->healthMax ? this->healthMax : this->health + val;
    }
    void incHealthMax(int8_t val) {
        this->healthMax = this->healthMax + val;
    }

    Point& getPosition() {
        return this->location;
    }

    Point getPosition(int8_t x, int8_t y = 0) {
        Point newPoint;

        newPoint.x = this->location.x + x;
        newPoint.y = this->location.y + y;
        return newPoint;
    }

    Direction getOppositeDirection() {
        return (this->direction == Direction::Left ? Direction::Right : Direction::Left);
    }

    int8_t getDirectionOffset(int8_t val) {
        if(this->direction == Direction::Left) {
            return -val;
        } else {
            return val;
        }
    }

    void incX(int8_t inc) {
        this->x = this->x + inc;
    }

    void incY(int8_t inc) {
        this->prevY = y;
        this->y = this->y + inc;
    }

    void changeDirection() {
        this->direction = this->getOppositeDirection();
    }

    // ----------------------------------------------------------------------------------------------------------

    bool isFootDown() {
        ImageDetails imageDetails;
        this->getImageDetails(imageDetails);
        return (
            imageDetails.toe != Constants::InAir &&
            imageDetails.toe != Constants::InAir_DoNotFall);
    }

    bool inAir() {
        ImageDetails imageDetails;
        this->getImageDetails(imageDetails);
        return (imageDetails.toe == Constants::InAir);
    }

    void getImageDetails(ImageDetails& imageDetails) {
        uint8_t imageIndex = getImageIndexFromStance(this->stance);
        uint24_t startPos =
            static_cast<uint24_t>(Constants::Prince_ImageDetails + ((imageIndex - 1) * 3));
        int8_t direction = this->getDirection() == Direction::Left ? -1 : 1;

        FX::seekData(startPos);
        uint16_t data = FX::readPendingUInt16();
        imageDetails.reach = static_cast<int8_t>((data >> 8) * direction);
        imageDetails.toe = static_cast<int8_t>((data & 0xFF) * direction);
        imageDetails.heel = static_cast<int8_t>(FX::readEnd() * direction);

#ifdef DEBUG_IMAGE_DETAILS
        DEBUG_PRINT(F("ImageIndex: "));
        DEBUG_PRINT(imageIndex);
        DEBUG_PRINT(F(", startPos: "));
        DEBUG_PRINT((int32_t)startPos);
        DEBUG_PRINT(F(", direction: "));
        DEBUG_PRINT((uint8_t)direction);
        DEBUG_PRINT(F(", reach: "));
        DEBUG_PRINT(imageDetails.reach);
        DEBUG_PRINT(F(", toe: "));
        DEBUG_PRINT(imageDetails.toe);
        DEBUG_PRINT(F(", heel: "));
        DEBUG_PRINTLN(imageDetails.heel);
#endif

        if(imageDetails.toe == -Constants::InAir) imageDetails.toe = Constants::InAir;
        if(imageDetails.toe == -Constants::InAir_DoNotFall)
            imageDetails.toe = Constants::InAir_DoNotFall;
    }

    uint8_t getImageIndexFromStance(uint16_t stance) {
        FX::seekData(Constants::StanceToImageXRefFX + stance);
        uint8_t image = FX::readEnd();

        return image;
    }
};

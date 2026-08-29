#pragma once

#include <stdint.h>
#include "game/Entity.h"

class Player : public Entity
{
public:
	uint8_t angle;
	int16_t velocityX, velocityY;
	int8_t angularVelocity;

	uint8_t shakeTime;
	uint8_t damageTime;
	uint8_t reloadTime;

	static constexpr uint8_t maxHP = 100;
	static constexpr uint8_t maxMana = 100;
	static constexpr uint8_t manaFireCost = 20;
	static constexpr uint8_t manaRechargeRate = 1;
	static constexpr uint8_t attackStrength = 10;
	static constexpr uint8_t collisionSize = 48;
	static constexpr uint8_t lookAheadDistance = 60;
	static constexpr uint8_t potionStrength = 25;

	uint8_t hp;
	uint8_t mana;

	void Init();
	void NextLevel();
	void Tick();
	void Fire();
	void Move(int16_t deltaX, int16_t deltaY);
	bool CheckCollisions();
	void Damage(uint8_t amount);
	bool IsWorldColliding() const;
};

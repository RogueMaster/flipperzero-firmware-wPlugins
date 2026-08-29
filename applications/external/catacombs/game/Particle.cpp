#include "game/Particle.h"
#include "game/FixedMath.h"
#include "game/Platform.h"

ParticleSystem ParticleSystemManager::systems[MAX_SYSTEMS];

void ParticleSystem::Init() {
    life = 0;
}

void ParticleSystem::Step() {
    for(Particle& p : particles) {
        if(p.IsActive()) {
            p.velY += gravity;

            if(p.x + p.velX < -127 || p.x + p.velX > 127 || p.y + p.velY < -127) {
                p.x = -128;
                continue;
            }

            if(p.y + p.velY >= 128) {
                p.velY = p.velX = 0;
                p.y = 127;
            }

            p.x += p.velX;
            p.y += p.velY;
        }
    }

    life--;
}

void ParticleSystem::Draw(int x, int halfScale) {
    int scale = 2 * halfScale;
    int8_t horizon = Renderer::GetHorizon(x);
    uint8_t colour = isWhite ? COLOUR_WHITE : COLOUR_BLACK;

    for(Particle& p : particles) {
        if(p.IsActive()) {
            int outX = x + ((p.x * scale) >> 8);
            int outY = horizon + ((p.y * scale) >> 8);

            if(outX >= 0 && outY >= 0 && outX < DISPLAY_WIDTH - 1 && outY < DISPLAY_HEIGHT - 1 &&
               halfScale >= Renderer::wBuffer[outX]) {
                Platform::PutPixel(outX, outY, colour);
                Platform::PutPixel(outX + 1, outY, colour);
                Platform::PutPixel(outX + 1, outY + 1, colour);
                Platform::PutPixel(outX, outY + 1, colour);
            }
        }
    }
}

void ParticleSystem::Explode() {
    for(Particle& p : particles) {
        p.x = (Random() & 31) - 16;
        p.y = (Random() & 31) - 16;

        p.velX = (Random() & 31) - 16;
        p.velY = (Random() & 31) - 25;
    }

    life = 22;
}

void ParticleSystemManager::Draw() {
    for(ParticleSystem& system : systems) {
        if(system.IsActive()) {
            int16_t screenX, screenW;

            if(Renderer::TransformAndCull(system.worldX, system.worldY, screenX, screenW)) {
                QueuedDrawable* drawable = Renderer::CreateQueuedDrawable((uint8_t)screenW);
                if(drawable) {
                    drawable->type = DrawableType::ParticleSystem;
                    drawable->x = (int8_t)screenX;
                    drawable->inverseCameraDistance = (uint8_t)screenW;
                    drawable->particleSystem = &system;
                }
            }
        }
    }
}

void ParticleSystemManager::Init() {
    for(ParticleSystem& system : systems) {
        system.Init();
    }
}

void ParticleSystemManager::Update() {
    for(ParticleSystem& system : systems) {
        if(system.IsActive()) {
            system.Step();
        }
    }
}

void ParticleSystemManager::CreateExplosion(int16_t worldX, int16_t worldY, bool isWhite) {
    ParticleSystem* newSystem = nullptr;
    for(ParticleSystem& system : systems) {
        if(!system.IsActive()) {
            newSystem = &system;
            break;
        }
    }

    if(!newSystem) {
        newSystem = &systems[0];

        for(uint8_t n = 1; n < MAX_SYSTEMS; n++) {
            if(systems[n].life < newSystem->life) {
                newSystem = &systems[n];
            }
        }
    }

    newSystem->worldX = worldX;
    newSystem->worldY = worldY;
    newSystem->isWhite = isWhite;
    newSystem->Explode();
}

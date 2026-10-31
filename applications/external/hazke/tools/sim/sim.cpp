// Desktop simulator: runs the flight code and the wireframe renderer on a
// Mac and writes frames as PBM images, so the 128x64 view can be checked
// without a Flipper. Build and run with tools/sim/run.sh.

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "Galaxy.h"
#include "SolarSystem.h"
#include "GameState.h"
#include "Flight.h"
#include "World.h"

static std::vector<std::string> shots;

static void save(const char* name) {
    std::string path = std::string("out/") + name + ".pbm";
    FILE* f = fopen(path.c_str(), "wb");
    fprintf(f, "P4\n%d %d\n", Fb::W, Fb::H);
    // PBM rows are MSB-first; the frame buffer is in display pages.
    for(int y = 0; y < Fb::H; y++) {
        for(int b = 0; b < Fb::W / 8; b++) {
            uint8_t r = 0;
            for(int k = 0; k < 8; k++) {
                int x = b * 8 + k;
                if(Fb::buf[(y >> 3) * Fb::W + x] & (1u << (y & 7))) r |= (uint8_t)(0x80u >> k);
            }
            fputc(r, f);
        }
    }
    fclose(f);
    shots.push_back(name);
}

static GameState game;

static void run(float secs, const InputState& in) {
    const float dt = 1.0f / 30.0f;
    for(float t = 0; t < secs; t += dt) {
        SimClock::ms += 33;
        World::animT += dt;
        SystemFlight::step(game, in, dt);
    }
}

static void frame(const char* name) {
    World::drawFlight(game);
    save(name);
}

int main() {
    Galaxy::generate();
    game.reset();
    InputState none = {};

    // 1. Arrival at the gate of system 0, planet I marked.
    SystemFlight::enter(0);
    SystemFlight::state.zoneToast = SystemFlight::ZoneToastTime;
    World::scatterDust();
    frame("01_arrival");

    // 2. Warp to planet I and drop out.
    SystemFlight::engage();
    for(int i = 0; i < 400 && SystemFlight::state.warping; i++)
        run(1.0f / 30.0f, none);
    run(0.5f, none);
    frame("02_after_warp");

    // 3. Closing on the planet.
    run(3.0f, none);
    frame("03_approach");

    // 4. A pirate dead ahead at 1400, marked, lasers firing.
    {
        auto& s = SystemFlight::state;
        auto& sh = NPCShip::ships[0];
        sh.active = true;
        sh.role = NPCShip::Role::Pirate;
        sh.modelId = Ship3D::Interceptor;
        sh.wx = s.px + s.fx * 1400.0f + s.ux * 120.0f;
        sh.wy = s.py + s.fy * 1400.0f + s.uy * 120.0f;
        sh.wz = s.pz + s.fz * 1400.0f + s.uz * 120.0f;
        sh.yaw = 2.4f;
        sh.pitch = 0.2f;
        sh.shields = 1.0f;
        sh.hull = 1.0f;
        s.lockedNPC = 0;
        s.targetIdx = -1;
        game.speed = 0.0f;
        Combat::playerFlash.t = Combat::FlashLifetime;
        Combat::playerFlash.shipIdx = 0;
        frame("04_dogfight");
        // Same ship far away.
        sh.wx = s.px + s.fx * 9000.0f;
        sh.wy = s.py + s.fy * 9000.0f;
        sh.wz = s.pz + s.fz * 9000.0f;
        Combat::playerFlash.t = 0.0f;
        frame("05_far_ship");
        // Behind the player: off-screen.
        sh.wx = s.px - s.fx * 2000.0f + s.ux * 600.0f;
        sh.wy = s.py - s.fy * 2000.0f + s.uy * 600.0f;
        sh.wz = s.pz - s.fz * 2000.0f + s.uz * 600.0f;
        frame("06_ship_behind");
    }

    // 7. Sun close up.
    {
        auto& s = SystemFlight::state;
        s.px = 0.0f;
        s.py = 0.0f;
        s.pz = -11000.0f;
        s.fx = 0.0f;
        s.fy = 0.0f;
        s.fz = 1.0f;
        s.ux = 0.0f;
        s.uy = 1.0f;
        s.uz = 0.0f;
        frame("07_sun");
    }

    // Planet I from three and six radii out, nose on it.
    for(int k = 0; k < 2; k++) {
        auto& s = SystemFlight::state;
        int pi = SystemFlight::firstPlanet();
        const auto& p = SystemFlight::layout.poi[pi];
        float d = (float)p.radius * (k == 0 ? 3.0f : 6.0f);
        s.px = (float)p.x - d * 0.6f;
        s.py = (float)p.y + d * 0.3f;
        s.pz = (float)p.z - d * 0.74f;
        float fx = (float)p.x - s.px, fy = (float)p.y - s.py, fz = (float)p.z - s.pz;
        float l = sqrtf(fx * fx + fy * fy + fz * fz);
        s.fx = fx / l;
        s.fy = fy / l;
        s.fz = fz / l;
        s.ux = 0.0f;
        s.uy = 1.0f;
        s.uz = 0.0f;
        SystemFlight::renormalize();
        s.targetIdx = pi;
        s.lockedNPC = -1;
        NPCShip::ships[0].active = false;
        frame(k == 0 ? "11_planet_3r" : "12_planet_6r");
    }

    // 8. System map, 9. title ship, 10. hyperspace.
    World::drawSystemMap(1);
    save("08_map");
    World::drawTitleShip(1.3f);
    save("09_title");
    World::drawHyperspace(0.6f);
    save("10_hyperspace");

    for(auto& n : shots)
        printf("%s\n", n.c_str());
    return 0;
}

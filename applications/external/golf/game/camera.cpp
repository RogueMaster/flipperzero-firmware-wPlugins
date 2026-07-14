#include "game.hpp"

dvec3 cam;
uint16_t yaw;
int16_t pitch;

struct ctrl {
    int16_t dydt;
    int16_t step(int16_t y, int16_t x, uint8_t speed) {
        int16_t diff = x - y;
        int16_t d2ydt = diff / 4 - dydt;
        y += mul_f8_s16(dydt, speed);
        dydt += mul_f8_s16(d2ydt, speed);
        return y;
    }
};

static ctrl ctrl_cam[5];

void update_camera(
    dvec3 tcam,
    uint16_t tyaw,
    int16_t tpitch,
    uint8_t move_speed,
    uint8_t look_speed) {
    cam.x = ctrl_cam[0].step(cam.x, tcam.x, move_speed);
    cam.y = ctrl_cam[1].step(cam.y, tcam.y, move_speed);
    cam.z = ctrl_cam[2].step(cam.z, tcam.z, move_speed);
    yaw = ctrl_cam[3].step(yaw, tyaw, look_speed);
    pitch = ctrl_cam[4].step(pitch, tpitch, look_speed);
}

void update_camera_look_at(
    dvec3 tlookat,
    uint16_t tyaw,
    int16_t tpitch,
    uint16_t dist,
    uint8_t move_speed,
    uint8_t look_speed) {
    dvec3 tcam = tlookat;
    {
        mat3 m;
        rotation_phys(m, uint8_t(-(yaw >> 8)), int8_t(-uint16_t(pitch >> 8)));
        dvec3 dv = matvec(m, dvec3{0, 0, int16_t(dist)});
        tcam.x += dv.x;
        tcam.y += dv.y;
        tcam.z += dv.z;
    }
    update_camera(tcam, tyaw, tpitch, move_speed, look_speed);
}

void update_camera_look_at_fastangle(
    dvec3 tlookat,
    uint16_t tyaw,
    int16_t tpitch,
    uint16_t dist,
    uint8_t move_speed,
    uint8_t look_speed) {
    dvec3 tcam = tlookat;
    {
        mat3 m;
        rotation_phys(m, uint8_t(-(tyaw >> 8)), int8_t(-uint16_t(tpitch >> 8)));
        dvec3 dv = matvec(m, dvec3{0, 0, int16_t(dist)});
        tcam.x += dv.x;
        tcam.y += dv.y;
        tcam.z += dv.z;
    }
    update_camera(tcam, tyaw, tpitch, move_speed, look_speed);
}

uint16_t yaw_to_flag() {
    dvec3 flag = levelext.flag_pos;
    return atan2(flag.z - ball.z, flag.x - ball.x) + 16384;
}

void update_camera_follow_ball(uint16_t dist, uint8_t move_speed, uint8_t look_speed) {
    uint16_t tyaw = yaw_to_flag();
    int16_t tpitch = 6500; // TODO
    dvec3 tlookat = ball;

    update_camera_look_at(tlookat, tyaw, tpitch, dist, move_speed, look_speed);
}

void update_camera_reset_velocities() {
    for(auto& c : ctrl_cam)
        c.dydt = 0;
}

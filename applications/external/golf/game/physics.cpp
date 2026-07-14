#include "game.hpp"

dvec3 ball;

// velocity, scaled 256x unit: range is (-0.5, +0.5) units per step
// range is OK since the ball should never be moving 0.5 units per step anyway
dvec3 ball_vel;

dvec3 ball_vel_ang;

static uint16_t steps;
static constexpr uint16_t MAX_STEPS = 30 * 60;

// 0.25 units per step
static constexpr int16_t MAX_VEL = 256 * 64;

// gravity acceleration: y vel units per step
static constexpr int16_t GRAVITY = 48;

// ball restitution as a fraction of 256
static constexpr uint8_t RESTITUTION = 256 * 0.75;

static uint8_t stop_vel_steps;
static constexpr int16_t STOP_VEL = 256;
static constexpr uint8_t STOP_VEL_MAX_STEPS = 30;

// square unsigned (x >= 0)
static constexpr uint32_t FORCEINLINE squ(int16_t x) {
    return uint32_t(int32_t(x) * x);
}

static constexpr uint32_t BALL_RADIUS_SQ = squ(BALL_RADIUS);

uint32_t sqdist_point_aabb(dvec3 size, dvec3 pt) {
    uint32_t d = 0;
    {
        int16_t pv = pt.x, bv = size.x;
        if(pv < -bv) d += squ(pv + bv);
        if(pv > bv) d += squ(pv - bv);
    }
    {
        int16_t pv = pt.y, bv = size.y;
        if(pv < -bv) d += squ(pv + bv);
        if(pv > bv) d += squ(pv - bv);
    }
    {
        int16_t pv = pt.z, bv = size.z;
        if(pv < -bv) d += squ(pv + bv);
        if(pv > bv) d += squ(pv - bv);
    }
    return d;
}

/*

Symbols:

en = normal elasticity = 1/2
et = frictional elasticity = 1
m  = ball mass = 1
I  = ball moment of inertia, I = about m/8 = 1/8

w1 = ball pre collision angular velocity
w2 = ball post collision angular velocity
v1 = ball pre collision velocity
v2 = ball post collision velocity
vp = ball contact point pre collision velocity
n  = collision normal
t  = frictional direction (unit)
rp = vector from ball center to collision point
Jn = normal impulse
Jt = frictional impulse
Jv = total impulse vector

Equations:

    vp = v1 + cross(w1, rp)
    Jn = -((1 + en) * dot(vp, n)) / (1/m + norm2(cross(rp, n))/I)
    Jt = -((1 + et) * dot(vp, t)) / (1/m + norm2(cross(rp, t))/I)
    Jv = Jn*n+Jt*t
    v2 = v1 + Jv/m
    w2 = w1 + cross(rp, Jv)/I

If m = 1, I = 1/8, en = 1/2, et = 1, rp = -(1/2)n, and t is orthogonal to rp:

    vp = v1 - 1/2cross(w1, n)
    Jv = -3/2*dot(vp, n)*n -1/3*dot(vp, t)*t    [note: factors are adjustable]
    v2 = v1 + Jv
    w2 = w1 + 4*cross(Jv, n)

*/

static dvec3 cross(dvec3 a, dvec3 b) {
    dvec3 r;
    r.x = mul_f8_s16(a.y, b.z) - mul_f8_s16(a.z, b.y);
    r.y = mul_f8_s16(a.z, b.x) - mul_f8_s16(a.x, b.z);
    r.z = mul_f8_s16(a.x, b.y) - mul_f8_s16(a.y, b.x);
    return r;
}

static void physics_collision(phys_box const& b) {
    mat3 m;

    dvec3 pt;
    pt.x = ball.x - b.pos.x * BOX_POS_FACTOR;
    pt.y = ball.y - b.pos.y * BOX_POS_FACTOR;
    pt.z = ball.z - b.pos.z * BOX_POS_FACTOR;

    if(b.yaw != 0 || b.pitch != 0) {
        rotation_phys(m, b.yaw, b.pitch);
        // rotate pt
        pt = matvec_t(m, pt);
    }

    dvec3 bsize;
    bsize.x = b.size.x * BOX_SIZE_FACTOR;
    bsize.y = b.size.y * BOX_SIZE_FACTOR;
    bsize.z = b.size.z * BOX_SIZE_FACTOR;

    // early exit if no collision
    if(sqdist_point_aabb(bsize, pt) > BALL_RADIUS_SQ) return;

    // find contact point on box
    dvec3 cpt;
    cpt.x = tclamp<int16_t>(pt.x, -bsize.x, bsize.x);
    cpt.y = tclamp<int16_t>(pt.y, -bsize.y, bsize.y);
    cpt.z = tclamp<int16_t>(pt.z, -bsize.z, bsize.z);

    // collision normal is difference between contact point and ball center
    dvec3 normal;
    normal.x = pt.x - cpt.x;
    normal.y = pt.y - cpt.y;
    normal.z = pt.z - cpt.z;

    // ball center should never be able to be inside box
    myassert((normal.x | normal.y | normal.z) != 0);

    normal = normalized(normal);

    if(b.yaw != 0 || b.pitch != 0) {
        // rotate normal
        normal = matvec(m, normal);
    }

    int16_t normdot = dot(ball_vel, normal);

    // ignore collision if normal is aligned with current velocity; this
    // happens in case collision has been processed but failed to
    // resolve penetration
    if(normdot > 0) return;

    if(normdot < -256 * 4) {
        play_tone(120, 50);
    }

    // attempt to resolve penetration
#if 0
    ball.x -= mul_f16_s16(normal.x, normdot);
    ball.y -= mul_f16_s16(normal.y, normdot);
    ball.z -= mul_f16_s16(normal.z, normdot);
#endif

    // tangent vector is rejection of velocity from normal
    dvec3 tangent;
    tangent.x = ball_vel.x - mul_f8_s16(normdot, normal.x);
    tangent.y = ball_vel.y - mul_f8_s16(normdot, normal.y);
    tangent.z = ball_vel.z - mul_f8_s16(normdot, normal.z);
    tangent = normalized(tangent);

    // contact point velocity
    dvec3 vp;
    {
        dvec3 t = cross(ball_vel_ang, normal);
        vp.x = ball_vel.x - t.x / 2;
        vp.y = ball_vel.y - t.y / 2;
        vp.z = ball_vel.z - t.z / 2;
    }

    // impulse factor
    dvec3 Jv;
    {
        int16_t t0 = -dot(vp, normal);
        int16_t t1 = -dot(vp, tangent);
        t0 += mul_f8_s16(t0, RESTITUTION);
        t1 = mul_f8_s16(t1, uint8_t(256 / 3));
        Jv.x = mul_f8_s16(t0, normal.x);
        Jv.y = mul_f8_s16(t0, normal.y);
        Jv.z = mul_f8_s16(t0, normal.z);
        Jv.x += mul_f8_s16(t1, tangent.x);
        Jv.y += mul_f8_s16(t1, tangent.y);
        Jv.z += mul_f8_s16(t1, tangent.z);
    }

    // velocity update
    ball_vel.x += Jv.x;
    ball_vel.y += Jv.y;
    ball_vel.z += Jv.z;

    // angular velocity update
    {
        dvec3 t = cross(Jv, normal);
        ball_vel_ang.x += t.x * 4;
        ball_vel_ang.y += t.y * 4;
        ball_vel_ang.z += t.z * 4;
    }
}

static void main_step() {
    ball_vel.y -= GRAVITY;

    ball_vel.x = tclamp<int16_t>(ball_vel.x, -MAX_VEL, MAX_VEL);
    ball_vel.y = tclamp<int16_t>(ball_vel.y, -MAX_VEL, MAX_VEL);
    ball_vel.z = tclamp<int16_t>(ball_vel.z, -MAX_VEL, MAX_VEL);

    ball.x += int8_t(uint16_t(ball_vel.x + 0x80) >> 8);
    ball.y += int8_t(uint16_t(ball_vel.y + 0x80) >> 8);
    ball.z += int8_t(uint16_t(ball_vel.z + 0x80) >> 8);

    // check collision and perform restitution
    uint8_t num_boxes = levelext.num_boxes;
    for(uint8_t i = 0; i < num_boxes; ++i)
        physics_collision(buf_boxes[i]);
}

bool physics_step() {
    for(uint8_t i = 0; i < 4; ++i) {
        main_step();

        ball_vel_ang.x = (int16_t)((((int32_t)ball_vel_ang.x * 255) + 0x80) >> 8);
        ball_vel_ang.y = (int16_t)((((int32_t)ball_vel_ang.y * 255) + 0x80) >> 8);
        ball_vel_ang.z = (int16_t)((((int32_t)ball_vel_ang.z * 255) + 0x80) >> 8);
    }

    if(tmax(tabs(ball_vel.x), tabs(ball_vel.y), tabs(ball_vel.z)) < STOP_VEL)
        ++stop_vel_steps;
    else
        stop_vel_steps = 0;

    bool done = steps >= MAX_STEPS || stop_vel_steps >= STOP_VEL_MAX_STEPS;

    if(done) {
        steps = 0;
        stop_vel_steps = 0;
        ball_vel = {};
    }

    return done;
}

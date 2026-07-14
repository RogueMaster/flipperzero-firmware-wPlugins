#include "game.hpp"

/*
Rotation matrix:

A = pitch, B = yaw

     cosB      0       sinB
 sinAsinB   cosA  -sinAcosB
-cosAsinB   sinA   cosAcosB

*/

void rotation(mat3& m, uint8_t yaw, int8_t pitch) {
    int8_t sinA = fsin((uint8_t)pitch);
    int8_t cosA = fcos((uint8_t)pitch);
    int8_t sinB = fsin(yaw);
    int8_t cosB = fcos(yaw);

    m[0] = cosB;
    m[1] = 0;
    m[2] = sinB;

    m[3] = fmuls8(sinA, sinB);
    m[4] = cosA;
    m[5] = -fmuls8(sinA, cosB);

    m[6] = -fmuls8(cosA, sinB);
    m[7] = sinA;
    m[8] = fmuls8(cosA, cosB);
}

void rotation16(dmat3& m, uint16_t yaw, int16_t pitch) {
    int16_t sinA = fsin16((uint16_t)pitch);
    int16_t cosA = fcos16((uint16_t)pitch);
    int16_t sinB = fsin16(yaw);
    int16_t cosB = fcos16(yaw);

    m[0] = cosB;
    m[1] = 0;
    m[2] = sinB;

    m[3] = mul_f15_s16(sinA, sinB);
    m[4] = cosA;
    m[5] = -mul_f15_s16(sinA, cosB);

    m[6] = -mul_f15_s16(cosA, sinB);
    m[7] = sinA;
    m[8] = mul_f15_s16(cosA, cosB);
}

/*
Rotation matrix:

A = pitch, B = yaw

 cosB   sinAsinB   cosAsinB
 0      cosA      -sinA
-sinB   sinAcosB   cosAcosB

*/

void rotation_phys(mat3& m, uint8_t yaw, int8_t pitch) {
    int8_t sinA = fsin((uint8_t)pitch);
    int8_t cosA = fcos((uint8_t)pitch);
    int8_t sinB = fsin(yaw);
    int8_t cosB = fcos(yaw);

    m[0] = cosB;
    m[1] = fmuls8(sinA, sinB);
    m[2] = fmuls8(cosA, sinB);

    m[3] = 0;
    m[4] = cosA;
    m[5] = -sinA;

    m[6] = -sinB;
    m[7] = fmuls8(sinA, cosB);
    m[8] = fmuls8(cosA, cosB);
}

dvec3 matvec(mat3 const& m, vec3 v) {
    dvec3 r;
    r.x = v.x * m[0] + v.y * m[1] + v.z * m[2];
    r.y = v.x * m[3] + v.y * m[4] + v.z * m[5];
    r.z = v.x * m[6] + v.y * m[7] + v.z * m[8];
    return r;
}

dvec3 matvec_t(mat3 const& m, vec3 v) {
    dvec3 r;
    r.x = v.x * m[0] + v.y * m[3] + v.z * m[6];
    r.y = v.x * m[1] + v.y * m[4] + v.z * m[7];
    r.z = v.x * m[2] + v.y * m[5] + v.z * m[8];
    return r;
}

dvec3 matvec(mat3 const& m, dvec3 v) {
    dvec3 r;
    r.x = mul_f7_s16(v.x, m[0]) + mul_f7_s16(v.y, m[1]) + mul_f7_s16(v.z, m[2]);
    r.y = mul_f7_s16(v.x, m[3]) + mul_f7_s16(v.y, m[4]) + mul_f7_s16(v.z, m[5]);
    r.z = mul_f7_s16(v.x, m[6]) + mul_f7_s16(v.y, m[7]) + mul_f7_s16(v.z, m[8]);
    return r;
}

dvec3 matvec_t(mat3 const& m, dvec3 v) {
    dvec3 r;
    r.x = mul_f7_s16(v.x, m[0]) + mul_f7_s16(v.y, m[3]) + mul_f7_s16(v.z, m[6]);
    r.y = mul_f7_s16(v.x, m[1]) + mul_f7_s16(v.y, m[4]) + mul_f7_s16(v.z, m[7]);
    r.z = mul_f7_s16(v.x, m[2]) + mul_f7_s16(v.y, m[5]) + mul_f7_s16(v.z, m[8]);
    return r;
}

dvec3 matvec(dmat3 const& m, dvec3 v) {
    dvec3 r;
    r.x = mul_f15_s16(v.x, m[0]) + mul_f15_s16(v.y, m[1]) + mul_f15_s16(v.z, m[2]);
    r.y = mul_f15_s16(v.x, m[3]) + mul_f15_s16(v.y, m[4]) + mul_f15_s16(v.z, m[5]);
    r.z = mul_f15_s16(v.x, m[6]) + mul_f15_s16(v.y, m[7]) + mul_f15_s16(v.z, m[8]);
    return r;
}

// find x such that (a*x*x) == (1<<24)
static uint16_t inv_sqrt(uint16_t a) {
    uint16_t x = 0x100; // 1

    for(uint8_t i = 0; i < 8; ++i) {
        uint16_t t;
        t = mul_f8_u16(x, x); // x*x
        t = mul_f8_u16(a, t); // a*x*x
        t >>= 1; // 0.5 * a*x*x
        myassert(t <= 0x180);
        x = mul_f8_u16(uint16_t(0x180 - t), x);
    }

    return x;
}

dvec3 normalized(dvec3 v) {
    // TODO: fit v.x into int8_t and propagate precision?
    //       would save a lot of ops
    while(tmax(tabs(v.x), tabs(v.y), tabs(v.z)) >= (1 << 8)) {
        v.x /= 2;
        v.y /= 2;
        v.z /= 2;
    }
    uint16_t d = 0;
    d += mul_f8_s16(v.x, v.x);
    d += mul_f8_s16(v.y, v.y);
    d += mul_f8_s16(v.z, v.z);
    d = inv_sqrt(d);
    v.x = mul_f8_s16(v.x, d);
    v.y = mul_f8_s16(v.y, d);
    v.z = mul_f8_s16(v.z, d);
    return v;
}

int16_t dot(dvec3 a, dvec3 b) {
    int16_t r = 0;
    r += mul_f8_s16(a.x, b.x);
    r += mul_f8_s16(a.y, b.y);
    r += mul_f8_s16(a.z, b.z);
    return r;
}

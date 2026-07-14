#include "game.hpp"

#define PERFDOOM 0

#define FDIST_CAMY_MOD 1

static constexpr int16_t ZNEAR = 256; // near

static constexpr uint8_t FLAG_HEIGHT = 4;
static constexpr uint8_t FLAG_SIZE = 2;
static constexpr uint8_t FLAG_POLE_PAT = 4; // 3? more visible against flat
static constexpr uint8_t FLAG_PAT = 3;

array<face, MAX_FACES> fs;
array<uint8_t, MAX_FACES> forder;
array<dvec2, MAX_VERTS> vs;

static dvec2 interpz(dvec2 a, int16_t az, dvec2 b, int16_t bz) {
    dvec2 r;
    r.x = interp(az, ZNEAR, bz, a.x, b.x);
    r.y = interp(az, ZNEAR, bz, a.y, b.y);
    return r;
}

void clear_buf() {
    uint8_t* pa = &buf[0];
    uint8_t* pb = pa + 1024;
    while(pa < pb)
        *pa++ = 0;
}

static uint8_t add_vertex(dmat3 const& m, dvec3 dv, uint8_t nv) {
    myassert(nv < MAX_VERTS); // TODO: runtime check?

    // translate
    dv.x -= cam.x;
    dv.y -= cam.y;
    dv.z -= cam.z;

    // vertex distance
    buf_vdist[nv] = uint16_t(
        uint32_t(int32_t(dv.x) * dv.x + int32_t(dv.y) * dv.y + int32_t(dv.z) * dv.z) >> 16);

    // rotate
    dv = matvec(m, dv);
    dv.z = -dv.z;

    // save vertex
    vs[nv] = {dv.x, dv.y};
    buf_tvz[nv] = dv.z;

    return nv + 1;
}

static uint16_t
    calc_fdist(uint8_t i0, uint8_t i1, uint8_t i2, int16_t y0, int16_t y1, int16_t y2) {
    uint16_t fdist = buf_vdist[i0] + buf_vdist[i1] + buf_vdist[i2];
#if FDIST_CAMY_MOD
    int16_t a0 = tabs(cam.y - y0);
    int16_t a1 = tabs(cam.y - y1);
    int16_t a2 = tabs(cam.y - y2);
    uint16_t amin = (uint16_t)tmin(a0, a1, a2);
    uint16_t amin2 = uint16_t((uint32_t(amin) * amin) >> 16);
    fdist += amin2 * 16;
#endif
    return fdist;
}

#ifndef ARDUINO
static int ortho_zoom;
#endif

/*

RAM ORGANIZATION

buffer:
    vy   : 1*n
    vxz  : 2*n      vdist: 2*n   (vxz offset by +16 for ball/flag vdist)
    faces: 3*n      fdist: 2*n   (faces offset by +8 for ball/flag fdist)
    boxes: 8*nb     tvz  : 2*n

nonbuffer:
    vs   : 4*n
    fs   : 4*n

FRAME STEPS

 1. init data in buffer (from prog or FX)
 2. physics step
 3. init vdist/vs/tvz/fdist/fs for ball vertices (2) and face
 4. init vy/vxz/faces for flag
 5. transform vertices (rot/trans) to vs+vz. compute vdist on top of vxz
 6. init faces (to fs/fdist).
 7. clip faces against near plane (can further output to vs/tvz/vdist/fs/fdist)
 8. project vertices (perspective divide, center in framebuffer)
 9. order faces using fs/fdist
10. draw faces from fs/vs

*/

template <bool ortho>
static uint8_t render_scene() {
    dmat3 m;
    rotation16(m, yaw, pitch);

    //inv16(256+128);

#if PERFDOOM
    for(int i = 0; i < PERFDOOM; ++i) {
#endif

        uint8_t nv = 0;
        uint8_t nf = 0;

        bool ball_valid = false;
        // ball vertices (center and right side)
        constexpr uint8_t balli0 = 0;
        constexpr uint8_t balli1 = 1;
        // ball "face" is index 0
#if 1
        if(!ball_in_hole()) {
            nv = add_vertex(m, ball, nv);
            if(buf_tvz[0] >= ZNEAR) {
                ball_valid = true;
                vs[balli1] = vs[balli0];
                vs[balli1].x += 128;
                buf_tvz[balli1] = buf_tvz[balli0];
                buf_vdist[balli1] = buf_vdist[balli0];
                int16_t by = ball.y;
                uint16_t fdist = calc_fdist(balli0, balli0, balli0, by, by, by);
                fdist += 400; // TODO: revisit
                buf_fdist[0] = fdist;
                fs[0].pt = 255; // ball identified by this pattern
                ++nf;
                ++nv;
            }
        }
#endif

        // flag vertices and faces
#if 1
        {
            bool flag_valid = true;
            uint8_t fi = nv;
            dvec3 dv = levelext.flag_pos;
            dv.y += 256 * 1;
            int16_t dvy0 = dv.y;
            nv = add_vertex(m, dv, nv);
            flag_valid &= buf_tvz[nv - 1] >= ZNEAR;
            {
                dvec2 tv = vs[nv - 1];
                tv.x += 48;
                vs[nv] = tv;
                buf_tvz[nv] = buf_tvz[nv - 1];
                ++nv;
            }
            dv.y += 256 * FLAG_HEIGHT;
            int16_t dvy1 = dv.y;
            nv = add_vertex(m, dv, nv);
            flag_valid &= buf_tvz[nv - 1] >= ZNEAR;
            {
                dvec2 tv = vs[nv - 1];
                tv.x += 48;
                vs[nv] = tv;
                buf_tvz[nv] = buf_tvz[nv - 1];
                ++nv;
            }
            dv.y += 256 * FLAG_SIZE;
            nv = add_vertex(m, dv, nv);
            flag_valid &= buf_tvz[nv - 1] >= ZNEAR;
            dv.y -= 256 * FLAG_SIZE / 2;
            dv.x += fsin(nframe << 3);
            dv.z -= 256 * FLAG_SIZE;
            nv = add_vertex(m, dv, nv);
            flag_valid &= buf_tvz[nv - 1] >= ZNEAR;

            if(flag_valid) {
                buf_fdist[nf] = calc_fdist(fi + 0, fi + 1, fi + 2, dvy0, dvy0, dvy1);
                fs[nf] = {uint8_t(fi + 0), uint8_t(fi + 1), uint8_t(fi + 2), FLAG_POLE_PAT};
                ++nf;

                buf_fdist[nf] = calc_fdist(fi + 1, fi + 2, fi + 3, dvy0, dvy1, dvy1);
                fs[nf] = {uint8_t(fi + 1), uint8_t(fi + 2), uint8_t(fi + 3), FLAG_POLE_PAT};
                ++nf;

                buf_fdist[nf] = calc_fdist(fi + 3, fi + 4, fi + 5, dvy1, dvy1, dvy1);
                fs[nf] = {uint8_t(fi + 2), uint8_t(fi + 4), uint8_t(fi + 5), FLAG_PAT};
                ++nf;
            }
        }
#endif

        uint8_t voff = nv;

        // translate and rotate vertices
        for(uint8_t j = 0; j < levelext.num_verts; ++j) {
            dvec3 dv;
            int8_t tvx, tvy, tvz;

            static_assert(MAX_VERTS <= 128, "revisit uint8_t casts below");
            tvx = buf_vxz[uint8_t(j * 2 + 0)];
            tvz = buf_vxz[uint8_t(j * 2 + 1)];
            tvy = buf_vy[j];

            dv.x = tvx * 64;
            dv.y = tvy * 64;
            dv.z = tvz * 64;

            nv = add_vertex(m, dv, nv);
        }

        // assemble faces
        uint8_t begin_nf = nf;
        for(uint8_t pat = 0, tnf = 0, i = 0; pat < 5; ++pat) {
            // TODO: special handling for ball/flag here?
            tnf += levelext.pat_faces[pat];
            for(; i < tnf; ++i) {
                uint8_t i0 = buf_faces[i * 3 + 0] + voff;
                uint8_t i1 = buf_faces[i * 3 + 1] + voff;
                uint8_t i2 = buf_faces[i * 3 + 2] + voff;

                if(buf_tvz[i0] < ZNEAR && buf_tvz[i1] < ZNEAR && buf_tvz[i2] < ZNEAR) continue;

                fs[nf] = {i0, i1, i2, pat};
                ++nf;
            }
        }

        // compute fdist and clip faces to near plane
        uint8_t end_nf = nf;
        for(uint8_t i = begin_nf; i < end_nf; ++i) {
            face f = fs[i];

            int16_t z0 = buf_tvz[f.i0];
            int16_t z1 = buf_tvz[f.i1];
            int16_t z2 = buf_tvz[f.i2];

            uint8_t behind = 0;
            if(z0 < ZNEAR) behind |= 1;
            if(z1 < ZNEAR) behind |= 2;
            if(z2 < ZNEAR) behind |= 4;

                // the following case should not happen because such faces
                // were clipped during face assembly above
#if 0
        // discard if fully behind near plane
        if(behind == 7)
            continue;
#endif

            // face distance
            uint16_t fdist;
            {
                int16_t y0 = buf_vy[f.i0 - voff] * 64;
                int16_t y1 = buf_vy[f.i1 - voff] * 64;
                int16_t y2 = buf_vy[f.i2 - voff] * 64;
                fdist = calc_fdist(f.i0, f.i1, f.i2, y0, y1, y2);
            }
            buf_fdist[i] = fdist;

            // clip: exactly two vertices behind near plane
            if((behind & (behind - 1)) != 0) {
                if(nv + 2 > MAX_VERTS) continue;

                dvec2 newv0, newv1;
                uint8_t ibase;

                // adjust the two near vertices.
                if(!(behind & 1)) {
                    ibase = f.i0;
                    newv0 = interpz(vs[f.i1], z1, vs[f.i0], z0);
                    newv1 = interpz(vs[f.i2], z2, vs[f.i0], z0);
                } else if(!(behind & 2)) {
                    ibase = f.i1;
                    newv0 = interpz(vs[f.i0], z0, vs[f.i1], z1);
                    newv1 = interpz(vs[f.i2], z2, vs[f.i1], z1);
                } else {
                    ibase = f.i2;
                    newv0 = interpz(vs[f.i0], z0, vs[f.i2], z2);
                    newv1 = interpz(vs[f.i1], z1, vs[f.i2], z2);
                }

                // add vertices
                vs[nv] = newv0;
                buf_tvz[nv] = ZNEAR;
                ++nv;
                vs[nv] = newv1;
                buf_tvz[nv] = ZNEAR;
                ++nv;

                // add clip face
                fs[i] = {ibase, uint8_t(nv - 1), uint8_t(nv - 2), f.pt};

                continue;
            }

            // clip: exactly one vertex behind near plane
            else if(behind != 0) {
                if(nv + 2 > MAX_VERTS) continue;
                if(nf + 1 > MAX_FACES) continue;

                uint8_t ib, ic; // winding order indices
                dvec2 newv0, newv1;
                if(behind & 1) {
                    ib = f.i1, ic = f.i2;
                    newv0 = interpz(vs[f.i0], z0, vs[f.i1], z1);
                    newv1 = interpz(vs[f.i0], z0, vs[f.i2], z2);
                } else if(behind & 2) {
                    ib = f.i2, ic = f.i0;
                    newv0 = interpz(vs[f.i1], z1, vs[f.i2], z2);
                    newv1 = interpz(vs[f.i1], z1, vs[f.i0], z0);
                } else {
                    ib = f.i0, ic = f.i1;
                    newv0 = interpz(vs[f.i2], z2, vs[f.i0], z0);
                    newv1 = interpz(vs[f.i2], z2, vs[f.i1], z1);
                }

                // add new vertices
                vs[nv] = newv0;
                buf_tvz[nv] = ZNEAR;
                ++nv;
                vs[nv] = newv1;
                buf_tvz[nv] = ZNEAR;
                ++nv;

                // add first clip face
                fs[i] = {uint8_t(nv - 2), ib, ic, f.pt};

                // add second clip face (new face)
                buf_fdist[nf] = fdist;
                fs[nf] = {uint8_t(nv - 2), ic, uint8_t(nv - 1), f.pt};
                ++nf;

                continue;
            }
        }

        // project vertices
        for(uint8_t i = 0; i < nv; ++i) {
            dvec3 dv = {vs[i].x, vs[i].y, buf_tvz[i]};

#ifndef ARDUINO
            if(ortho) {
                // divide x and y by ortho_zoom
                dv.x = int32_t(dv.x) * ortho_zoom / 256 / FB_FRAC_COEF;
                dv.y = int32_t(dv.y) * ortho_zoom / 256 / FB_FRAC_COEF;
            } else
#endif
                // multiply x and y by FB_FRAC_COEF/4 / z
                if(dv.z >= ZNEAR) {
                    uint16_t invz = inv16(dv.z);
                    int32_t nx = int32_t(invz) * dv.x;
                    int32_t ny = int32_t(invz) * dv.y;
#if FB_FRAC_BITS > 2
                    static constexpr int32_t CLAMP_VAL =
                        (int32_t)INT16_MAX * 240 / FB_FRAC_COEF * 1024;
                    nx = tclamp<int32_t>(nx, -CLAMP_VAL, CLAMP_VAL);
                    ny = tclamp<int32_t>(ny, -CLAMP_VAL, CLAMP_VAL);
#endif
                    dv.x = int16_t(uint32_t(nx) >> (18 - FB_FRAC_BITS));
                    dv.y = int16_t(uint32_t(ny) >> (18 - FB_FRAC_BITS));
                }

            // center in framebuffer
            dv.x += (FBW / 2 * FB_FRAC_COEF);
            dv.y = (FBH / 2 * FB_FRAC_COEF) - dv.y;

            // save vertex
            vs[i] = {dv.x, dv.y};
        }

        // order faces
#if 1
        reset_forder();
        {
            uint8_t i = 1;
            while(i < nf) {
                uint8_t x = forder[i];
                uint16_t d = buf_fdist[x];
                uint8_t j = i;
                while(j > 0 && buf_fdist[forder[j - 1]] < d) {
                    forder[j] = forder[j - 1];
                    j -= 1;
                }
                forder[j] = x;
                i += 1;
            }
        }
#else
    {
        uint8_t i = 1;
        while(i < nf) {
            uint16_t d = buf_fdist[i];
            face f = fs[i];
            uint8_t j = i;
            while(j > 0 && buf_fdist[j - 1] < d) {
                buf_fdist[j] = buf_fdist[j - 1];
                fs[j] = fs[j - 1];
                j -= 1;
            }
            buf_fdist[j] = d;
            fs[j] = f;
            i += 1;
        }
    }
#endif

        // finally, render
        clear_buf(); // buf mem was used during setup
        uint16_t ballr = 0;
        for(uint8_t i = 0; i < nf; ++i) {
            face f = fs[forder[i]];
            //face f = fs[i];
            if(f.pt == 255) {
                // ball
                if(ball_valid) {
                    // extract ball radius from vertices
                    dvec2 c = vs[balli0];
                    ballr = uint16_t(vs[balli1].x - c.x);
                    draw_ball_filled(vs[balli0], ballr, 0xffff);
#if !BALL_XRAY
                    draw_ball_outline(vs[balli0], ballr + (FB_FRAC_COEF / 2));
#endif
                }
                continue;
            } else {
                draw_tri(vs[f.i0], vs[f.i1], vs[f.i2], f.pt);
            }
        }
#if BALL_XRAY
        if(ball_valid) draw_ball_outline(vs[balli0], ballr + (FB_FRAC_COEF / 2));
#endif

#if PERFDOOM
    }
#endif

    return nf;
}

uint8_t render_scene() {
    return render_scene<false>();
}

#ifndef ARDUINO
uint8_t render_scene_persp() {
    return render_scene<false>();
}
uint8_t render_scene_ortho(int zoom) {
    ortho_zoom = zoom;
    return render_scene<true>();
}
dvec3 transform_point(dvec3 dv, bool ortho, int ortho_zoom) {
    dmat3 m;
    rotation16(m, yaw, pitch);

    dv.x -= cam.x;
    dv.y -= cam.y;
    dv.z -= cam.z;

    dv = matvec(m, dv);
    dv.z = -dv.z;

    if(ortho) {
        dv.x = int32_t(dv.x) * ortho_zoom / 256 / FB_FRAC_COEF;
        dv.y = int32_t(dv.y) * ortho_zoom / 256 / FB_FRAC_COEF;
    } else if(dv.z >= ZNEAR) {
        uint16_t invz = inv16(dv.z);
        int32_t nx = int32_t(invz) * dv.x;
        int32_t ny = int32_t(invz) * dv.y;
#if FB_FRAC_BITS > 2
        static constexpr int32_t CLAMP_VAL = (int32_t)INT16_MAX * 240 / FB_FRAC_COEF * 1024;
        nx = tclamp<int32_t>(nx, -CLAMP_VAL, CLAMP_VAL);
        ny = tclamp<int32_t>(ny, -CLAMP_VAL, CLAMP_VAL);
#endif
        dv.x = int16_t(uint32_t(nx) >> (18 - FB_FRAC_BITS));
        dv.y = int16_t(uint32_t(ny) >> (18 - FB_FRAC_BITS));
    }

    dv.x += (FBW / 2 * FB_FRAC_COEF);
    dv.y = (FBH / 2 * FB_FRAC_COEF) - dv.y;

    return dv;
}
#endif

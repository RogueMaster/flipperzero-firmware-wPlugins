#include <limits.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <string.h>

#define POLYFILL_FUN_1(name, ret, arg1)                   ret name(arg1)
#define POLYFILL_FUN_2(name, ret, arg1, arg2)             ret name(arg1, arg2)
#define POLYFILL_FUN_3(name, ret, arg1, arg2, arg3)       ret name(arg1, arg2, arg3)
#define POLYFILL_FUN_4(name, ret, arg1, arg2, arg3, arg4) ret name(arg1, arg2, arg3, arg4)

#ifndef __aeabi_l2f
POLYFILL_FUN_1(__aeabi_l2f, float, long long x) {
    return x;
}
#endif

#ifndef __aeabi_f2lz
POLYFILL_FUN_1(__aeabi_f2lz, long long, float x) {
    return x;
}
#endif

#ifndef __aeabi_dcmple
POLYFILL_FUN_2(__aeabi_dcmple, double, double, double) {
}
#endif

#ifndef __aeabi_dcmplt
POLYFILL_FUN_2(__aeabi_dcmplt, double, double, double) {
}
#endif

#ifndef __aeabi_dcmpun
POLYFILL_FUN_2(__aeabi_dcmpun, double, double, double) {
}
#endif

#ifndef __aeabi_dcmpeq
POLYFILL_FUN_2(__aeabi_dcmpeq, double, double, double) {
}
#endif

#ifndef __aeabi_dmul
double __aeabi_dmul(double x, double y) {
    return x * y;
}
#endif

#ifndef __aeabi_dadd
POLYFILL_FUN_2(__aeabi_dadd, double, double x, double y) {
    return x + y;
}
#endif

#ifndef __aeabi_ddiv
POLYFILL_FUN_2(__aeabi_ddiv, double, double x, double y) {
    return x / y;
}
#endif

#ifndef __aeabi_l2d
POLYFILL_FUN_1(__aeabi_l2d, double, long x) {
    return x;
}
#endif

#ifndef __aeabi_f2d
POLYFILL_FUN_1(__aeabi_f2d, double, float x) {
    return x;
}
#endif

#ifndef __aeabi_dsub
POLYFILL_FUN_2(__aeabi_dsub, double, double x, double y) {
    return x - y;
}
#endif

#ifndef __aeabi_dcmpge
POLYFILL_FUN_2(__aeabi_dcmpge, double, double, double) {
}
#endif

#ifndef __aeabi_i2d
POLYFILL_FUN_1(__aeabi_i2d, double, int x) {
    return x;
}
#endif

#ifndef __aeabi_dcmpgt
POLYFILL_FUN_1(__aeabi_dcmpgt, double, double) {
}
#endif

#ifndef __aeabi_d2iz
POLYFILL_FUN_1(__aeabi_d2iz, long int, double x) {
    return x;
}
#endif

#ifndef __aeabi_d2lz
POLYFILL_FUN_1(__aeabi_d2lz, long, double x) {
    return x;
}
#endif

#ifndef __aeabi_d2uiz
POLYFILL_FUN_1(__aeabi_d2uiz, unsigned long int, double x) {
    return x;
}
#endif

#ifndef __aeabi_d2f
POLYFILL_FUN_1(__aeabi_d2f, float, double x) {
    return x;
}
#endif

#ifndef __aeabi_ldivmod
POLYFILL_FUN_2(__aeabi_ldivmod, long, long, long) {
}
#endif

#ifndef strtox
POLYFILL_FUN_4(strtox, long long unsigned int, const char*, char**, int, long long unsigned int) {
}
#endif

#if FLT_EVAL_METHOD == 0 || FLT_EVAL_METHOD == 1
#define EPS DBL_EPSILON
#elif FLT_EVAL_METHOD == 2
#define EPS LDBL_EPSILON
#endif
static const double_t toint = 1 / EPS;

#ifndef FORCE_EVAL
#define FORCE_EVAL(x)                            \
    do {                                         \
        if(sizeof(x) == sizeof(float)) {         \
            volatile float __x;                  \
            __x = (x);                           \
            (void)__x;                           \
        } else if(sizeof(x) == sizeof(double)) { \
            volatile double __x;                 \
            __x = (x);                           \
            (void)__x;                           \
        } else {                                 \
            volatile long double __x;            \
            __x = (x);                           \
            (void)__x;                           \
        }                                        \
    } while(0)
#endif

#ifndef floorf
float floorf(float x) {
    union {
        float f;
        uint32_t i;
    } u = {x};
    int e = (int)(u.i >> 23 & 0xff) - 0x7f;
    uint32_t m;

    if(e >= 23) return x;
    if(e >= 0) {
        m = 0x007fffff >> e;
        if((u.i & m) == 0) return x;
        FORCE_EVAL(x + 0x1p120f);
        if(u.i >> 31) u.i += m;
        u.i &= ~m;
    } else {
        FORCE_EVAL(x + 0x1p120f);
        if(u.i >> 31 == 0)
            u.i = 0;
        else if(u.i << 1)
            u.f = -1.0;
    }
    return u.f;
}
#endif

#ifndef floor
double floor(double x) {
    union {
        double f;
        uint64_t i;
    } u = {x};
    int e = u.i >> 52 & 0x7ff;
    double_t y;

    if(e >= 0x3ff + 52 || x == 0) return x;
    /* y = int(x) - x, where int(x) is an integer neighbor of x */
    if(u.i >> 63)
        y = x - toint + toint - x;
    else
        y = x + toint - toint - x;
    /* special case because of non-nearest rounding modes */
    if(e <= 0x3ff - 1) {
        FORCE_EVAL(y);
        return u.i >> 63 ? -1 : 0;
    }
    if(y > 0) return x + y - 1;
    return x + y;
}
#endif

#ifndef fmodf
float fmodf(float x, float y) {
    union {
        float f;
        uint32_t i;
    } ux = {x}, uy = {y};
    int ex = ux.i >> 23 & 0xff;
    int ey = uy.i >> 23 & 0xff;
    uint32_t sx = ux.i & 0x80000000;
    uint32_t i;
    uint32_t uxi = ux.i;

    if(uy.i << 1 == 0 || isnan(y) || ex == 0xff) return (x * y) / (x * y);
    if(uxi << 1 <= uy.i << 1) {
        if(uxi << 1 == uy.i << 1) return 0 * x;
        return x;
    }

    /* normalize x and y */
    if(!ex) {
        for(i = uxi << 9; i >> 31 == 0; ex--, i <<= 1)
            ;
        uxi <<= -ex + 1;
    } else {
        uxi &= -1U >> 9;
        uxi |= 1U << 23;
    }
    if(!ey) {
        for(i = uy.i << 9; i >> 31 == 0; ey--, i <<= 1)
            ;
        uy.i <<= -ey + 1;
    } else {
        uy.i &= -1U >> 9;
        uy.i |= 1U << 23;
    }

    /* x mod y */
    for(; ex > ey; ex--) {
        i = uxi - uy.i;
        if(i >> 31 == 0) {
            if(i == 0) return 0 * x;
            uxi = i;
        }
        uxi <<= 1;
    }
    i = uxi - uy.i;
    if(i >> 31 == 0) {
        if(i == 0) return 0 * x;
        uxi = i;
    }
    for(; uxi >> 23 == 0; uxi <<= 1, ex--)
        ;

    /* scale result up */
    if(ex > 0) {
        uxi -= 1U << 23;
        uxi |= (uint32_t)ex << 23;
    } else {
        uxi >>= -ex + 1;
    }
    uxi |= sx;
    ux.i = uxi;
    return ux.f;
}
#endif

#ifndef fmod
double fmod(double x, double y) {
    union {
        double f;
        uint64_t i;
    } ux = {x}, uy = {y};
    int ex = ux.i >> 52 & 0x7ff;
    int ey = uy.i >> 52 & 0x7ff;
    int sx = ux.i >> 63;
    uint64_t i;

    /* in the followings uxi should be ux.i, but then gcc wrongly adds */
    /* float load/store to inner loops ruining performance and code size */
    uint64_t uxi = ux.i;

    if(uy.i << 1 == 0 || isnan(y) || ex == 0x7ff) return (x * y) / (x * y);
    if(uxi << 1 <= uy.i << 1) {
        if(uxi << 1 == uy.i << 1) return 0 * x;
        return x;
    }

    /* normalize x and y */
    if(!ex) {
        for(i = uxi << 12; i >> 63 == 0; ex--, i <<= 1)
            ;
        uxi <<= -ex + 1;
    } else {
        uxi &= -1ULL >> 12;
        uxi |= 1ULL << 52;
    }
    if(!ey) {
        for(i = uy.i << 12; i >> 63 == 0; ey--, i <<= 1)
            ;
        uy.i <<= -ey + 1;
    } else {
        uy.i &= -1ULL >> 12;
        uy.i |= 1ULL << 52;
    }

    /* x mod y */
    for(; ex > ey; ex--) {
        i = uxi - uy.i;
        if(i >> 63 == 0) {
            if(i == 0) return 0 * x;
            uxi = i;
        }
        uxi <<= 1;
    }
    i = uxi - uy.i;
    if(i >> 63 == 0) {
        if(i == 0) return 0 * x;
        uxi = i;
    }
    for(; uxi >> 52 == 0; uxi <<= 1, ex--)
        ;

    /* scale result */
    if(ex > 0) {
        uxi -= 1ULL << 52;
        uxi |= (uint64_t)ex << 52;
    } else {
        uxi >>= -ex + 1;
    }
    uxi |= (uint64_t)sx << 63;
    ux.i = uxi;
    return ux.f;
}
#endif

#ifndef nearbyintf
float nearbyintf(float x) {
    union {
        float f;
        uint32_t i;
    } u = {x};
    int e = u.i >> 23 & 0xff;
    int s = u.i >> 31;
    float_t y;

    if(e >= 0x7f + 23) return x;
    if(s)
        y = x - 0x1p23f + 0x1p23f;
    else
        y = x + 0x1p23f - 0x1p23f;
    if(y == 0) return s ? -0.0f : 0.0f;
    return y;
}
#endif

#ifndef stroll
long long strtoll(const char* restrict s, char** restrict p, int base) {
    return strtox(s, p, base, LLONG_MIN);
}
#endif

#ifndef pow
double pow(double x, double y) {
    return powf(x, y);
}
#endif

#ifndef rint

double rint(double x) {
    union {
        double f;
        uint64_t i;
    } u = {x};
    int e = u.i >> 52 & 0x7ff;
    int s = u.i >> 63;
    double_t y;

    if(e >= 0x3ff + 52) return x;
    if(s)
        y = x - toint + toint;
    else
        y = x + toint - toint;
    if(y == 0) return s ? -0.0 : 0;
    return y;
}
#endif

#ifndef nearbyint
double nearbyint(double x) {
#ifdef FE_INEXACT
#pragma STDC FENV_ACCESS ON
    int e;

    e = fetestexcept(FE_INEXACT);
#endif
    x = rint(x);
#ifdef FE_INEXACT
    if(!e) feclearexcept(FE_INEXACT);
#endif
    return x;
}
#endif

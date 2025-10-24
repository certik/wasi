#pragma once

#include <base/base_types.h>

#ifndef NAN
#define NAN (__builtin_nanf(""))
#endif

#ifndef INFINITY
#define INFINITY (__builtin_inff())
#endif

#ifndef HUGE_VAL
#define HUGE_VAL (__builtin_huge_val())
#endif

static inline double fabs(double x) {
    return x < 0 ? -x : x;
}

static inline float fabsf(float x) {
    return x < 0 ? -x : x;
}

// Trigonometric functions using compiler builtins
static inline float cosf(float x) {
    return __builtin_cosf(x);
}

static inline float sinf(float x) {
    return __builtin_sinf(x);
}

static inline double cos(double x) {
    return __builtin_cos(x);
}

static inline double sin(double x) {
    return __builtin_sin(x);
}


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

static inline double base_fabs(double x) {
    return x < 0 ? -x : x;
}

static inline float base_fabsf(float x) {
    return x < 0 ? -x : x;
}

// Simple round implementation. Note: Overflows for values outside [INT64_MIN, INT64_MAX].
static inline double base_round(double x) {
    return (x >= 0.0) ? (double)(int64_t)(x + 0.5) : (double)(int64_t)(x - 0.5);
}

// Fast single-precision trigonometric functions
float fast_sin(float x);
float fast_cos(float x);
float fast_tan(float x);
float fast_sqrtf(float x);

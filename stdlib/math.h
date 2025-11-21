/*
 * Minimal math.h wrapper that routes to the engine's fast math routines.
 *
 * Provides the handful of functions needed by the build without pulling in a
 * full libc. Functions are thin inline wrappers over base/base_math.h and
 * platform/platform.h implementations.
 */

#ifndef MATH_H
#define MATH_H

#include <base/base_math.h>
#include <platform/platform.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double sin(double x) { return (double)fast_sin((float)x); }
static inline double cos(double x) { return (double)fast_cos((float)x); }
static inline double tan(double x) { return (double)fast_tan((float)x); }
static inline float sinf(float x) { return fast_sin(x); }
static inline float cosf(float x) { return fast_cos(x); }
static inline float tanf(float x) { return fast_tan(x); }

static inline double sqrt(double x) { return fast_sqrt(x); }
static inline float sqrtf(float x) { return fast_sqrtf(x); }

static inline double fabs(double x) { return base_fabs(x); }
static inline float fabsf(float x) { return base_fabsf(x); }

static inline double floor(double x) { return __builtin_floor(x); }
static inline float floorf(float x) { return __builtin_floorf(x); }
static inline double pow(double x, double y) { return __builtin_pow(x, y); }
static inline float powf(float x, float y) { return __builtin_powf(x, y); }

#endif // MATH_H

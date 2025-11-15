#pragma once

#include <cmath>

// Basic math utilities
inline float clamp(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

inline float lerp(float t, float a, float b) {
    return (1 - t) * a + t * b;
}

// 2D Point/Vector
struct Vec2 {
    float x, y;

    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
};

// 3D Vector
struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vec3(float v) : x(v), y(v), z(v) {}

    Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator*(const Vec3& v) const { return Vec3(x * v.x, y * v.y, z * v.z); }
    Vec3 operator/(float s) const { float inv = 1.0f / s; return Vec3(x * inv, y * inv, z * inv); }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }

    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float length_squared() const { return x * x + y * y + z * z; }
    float length() const { return sqrtf(length_squared()); }

    Vec3 normalized() const {
        float len = length();
        if (len > 0) return *this / len;
        return Vec3(0, 0, 0);
    }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

inline Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - 2 * dot(v, n) * n;
}

// RGB color (alias for Vec3)
using Color = Vec3;

// 3x3 Matrix
struct Mat3 {
    float m[3][3];

    Mat3() {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                m[i][j] = (i == j) ? 1.0f : 0.0f;
    }

    Vec3 operator*(const Vec3& v) const {
        return Vec3(
            m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
            m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
            m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z
        );
    }
};

// Ray
struct Ray {
    Vec3 origin;
    Vec3 direction;

    Ray() {}
    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(d) {}

    Vec3 at(float t) const {
        return origin + direction * t;
    }
};

// Bounding box
struct Bounds3 {
    Vec3 min, max;

    Bounds3() : min(Vec3(1e30f)), max(Vec3(-1e30f)) {}
    Bounds3(const Vec3& p) : min(p), max(p) {}
    Bounds3(const Vec3& min, const Vec3& max) : min(min), max(max) {}

    void expand(const Vec3& p) {
        min.x = fminf(min.x, p.x);
        min.y = fminf(min.y, p.y);
        min.z = fminf(min.z, p.z);
        max.x = fmaxf(max.x, p.x);
        max.y = fmaxf(max.y, p.y);
        max.z = fmaxf(max.z, p.z);
    }

    void expand(const Bounds3& b) {
        expand(b.min);
        expand(b.max);
    }

    bool intersect(const Ray& ray, float* t_min, float* t_max) const {
        float t0 = 0.0f;
        float t1 = 1e30f;

        for (int i = 0; i < 3; i++) {
            float inv_dir = 1.0f / (&ray.direction.x)[i];
            float t_near = ((&min.x)[i] - (&ray.origin.x)[i]) * inv_dir;
            float t_far = ((&max.x)[i] - (&ray.origin.x)[i]) * inv_dir;

            if (t_near > t_far) {
                float tmp = t_near;
                t_near = t_far;
                t_far = tmp;
            }

            t0 = t_near > t0 ? t_near : t0;
            t1 = t_far < t1 ? t_far : t1;

            if (t0 > t1) return false;
        }

        if (t_min) *t_min = t0;
        if (t_max) *t_max = t1;
        return true;
    }
};

// Camera utilities
inline Mat3 look_at(const Vec3& from, const Vec3& to, const Vec3& up) {
    Vec3 forward = (to - from).normalized();
    Vec3 right = cross(forward, up).normalized();
    Vec3 new_up = cross(right, forward);

    Mat3 m;
    m.m[0][0] = right.x; m.m[0][1] = right.y; m.m[0][2] = right.z;
    m.m[1][0] = new_up.x; m.m[1][1] = new_up.y; m.m[1][2] = new_up.z;
    m.m[2][0] = forward.x; m.m[2][1] = forward.y; m.m[2][2] = forward.z;

    return m;
}

#pragma once

#include "math.h"

// Camera interface
class Camera {
public:
    virtual ~Camera() {}
    virtual Ray generate_ray(const Vec2& pixel, int width, int height) const = 0;
};

// Perspective camera
class PerspectiveCamera : public Camera {
public:
    Vec3 position;
    Vec3 forward;
    Vec3 right;
    Vec3 up;
    float tan_fov_half;

    PerspectiveCamera(const Vec3& pos, const Vec3& look_at, const Vec3& up_vec, float fov_degrees) {
        position = pos;
        forward = (look_at - pos).normalized();
        right = cross(forward, up_vec).normalized();
        up = cross(right, forward).normalized();
        tan_fov_half = tanf(fov_degrees * 0.5f * 3.14159265f / 180.0f);
    }

    Ray generate_ray(const Vec2& pixel, int width, int height) const override {
        // Convert pixel to NDC [-1, 1]
        float aspect = (float)width / (float)height;
        float x = (2.0f * pixel.x / width - 1.0f) * aspect * tan_fov_half;
        float y = (1.0f - 2.0f * pixel.y / height) * tan_fov_half;

        // Ray direction in camera space
        Vec3 dir = (forward + right * x + up * y).normalized();

        return Ray(position, dir);
    }
};

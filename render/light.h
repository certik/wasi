#pragma once

#include "math.h"
#include "geometry.h"

// Light interface
class Light {
public:
    virtual ~Light() {}

    // Sample the light from a surface point
    // Returns the incident radiance Li
    // wi is set to the direction toward the light (normalized)
    // pdf is the probability density (for point lights, this is 1 for delta distribution)
    virtual Color sample_Li(const SurfaceInteraction& isect, Vec3* wi, float* pdf) const = 0;

    // Compute PDF for sampling direction wi from surface point
    // Returns 0 for delta lights (point, directional), non-zero for area lights
    virtual float pdf_Li(const SurfaceInteraction& isect, const Vec3& wi) const = 0;

    // Check if this is a delta distribution (point/directional light)
    virtual bool is_delta() const = 0;
};

// Point light source
class PointLight : public Light {
public:
    Vec3 position;
    Color color;
    float intensity;

    PointLight(const Vec3& pos, const Color& col, float intens)
        : position(pos), color(col), intensity(intens) {}

    Color sample_Li(const SurfaceInteraction& isect, Vec3* wi, float* pdf) const override {
        // Direction from surface to light
        Vec3 to_light = position - isect.point;
        float distance_squared = to_light.length_squared();
        float distance = sqrtf(distance_squared);

        *wi = to_light / distance;
        *pdf = 1.0f; // Delta distribution for point light

        // Intensity falls off with inverse square law
        float attenuation = 1.0f / distance_squared;

        return color * (intensity * attenuation);
    }

    float pdf_Li(const SurfaceInteraction& isect, const Vec3& wi) const override {
        // Delta distribution - cannot be sampled by BSDF
        return 0.0f;
    }

    bool is_delta() const override {
        return true;
    }
};

// Directional light (sun-like)
class DirectionalLight : public Light {
public:
    Vec3 direction; // Direction from which light comes
    Color color;
    float intensity;

    DirectionalLight(const Vec3& dir, const Color& col, float intens)
        : direction(dir.normalized()), color(col), intensity(intens) {}

    Color sample_Li(const SurfaceInteraction& isect, Vec3* wi, float* pdf) const override {
        *wi = -direction; // Direction toward light
        *pdf = 1.0f;
        return color * intensity;
    }

    float pdf_Li(const SurfaceInteraction& isect, const Vec3& wi) const override {
        // Delta distribution - cannot be sampled by BSDF
        return 0.0f;
    }

    bool is_delta() const override {
        return true;
    }
};

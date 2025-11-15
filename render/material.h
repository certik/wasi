#pragma once

#include "math.h"
#include "texture.h"
#include "geometry.h"

// BSDF interface - represents how light scatters at a surface
class BSDF {
public:
    virtual ~BSDF() {}

    // Evaluate the BSDF for given incoming (wi) and outgoing (wo) directions
    // wi points toward the light, wo points toward the camera
    virtual Color f(const Vec3& wo, const Vec3& wi) const = 0;
};

// Lambertian (diffuse) BSDF
class LambertianBSDF : public BSDF {
public:
    Color albedo;

    LambertianBSDF(const Color& albedo) : albedo(albedo) {}

    Color f(const Vec3& wo, const Vec3& wi) const override {
        // Lambertian reflection: albedo / pi
        return albedo * (1.0f / 3.14159265f);
    }
};

// Material interface
class Material {
public:
    virtual ~Material() {}
    virtual BSDF* get_bsdf(const SurfaceInteraction& isect) const = 0;
};

// Diffuse material with texture
class DiffuseMaterial : public Material {
public:
    Texture* albedo_texture;
    bool owns_texture;

    DiffuseMaterial(Texture* tex, bool owns = true)
        : albedo_texture(tex), owns_texture(owns) {}

    DiffuseMaterial(const Color& color)
        : albedo_texture(new ConstantTexture(color)), owns_texture(true) {}

    ~DiffuseMaterial() {
        if (owns_texture)
            delete albedo_texture;
    }

    BSDF* get_bsdf(const SurfaceInteraction& isect) const override {
        Color albedo = albedo_texture->evaluate(isect.uv);
        return new LambertianBSDF(albedo);
    }
};

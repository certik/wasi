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

    // Sample a direction wi given wo, return f and pdf
    virtual Color sample_f(const Vec3& wo, Vec3* wi, float u1, float u2, float* pdf) const = 0;

    // Compute PDF for sampling direction wi given wo
    virtual float pdf(const Vec3& wo, const Vec3& wi) const = 0;
};

// Lambertian (diffuse) BSDF
class LambertianBSDF : public BSDF {
public:
    Color albedo;
    Vec3 normal;

    LambertianBSDF(const Color& albedo, const Vec3& n) : albedo(albedo), normal(n) {}

    Color f(const Vec3& wo, const Vec3& wi) const override {
        // Lambertian reflection: albedo / pi
        return albedo * (1.0f / 3.14159265f);
    }

    Color sample_f(const Vec3& wo, Vec3* wi, float u1, float u2, float* pdf) const override {
        // Cosine-weighted hemisphere sampling
        Vec3 local_wi = sample_cosine_hemisphere(u1, u2);
        *wi = local_to_world(local_wi, normal);

        *pdf = this->pdf(wo, *wi);
        return f(wo, *wi);
    }

    float pdf(const Vec3& wo, const Vec3& wi) const override {
        // Cosine-weighted PDF: cos(theta) / pi
        float cos_theta = dot(wi, normal);
        return cos_theta > 0 ? cos_theta / 3.14159265f : 0.0f;
    }
};

// Material interface
class Material {
public:
    virtual ~Material() {}
    virtual BSDF* get_bsdf(const SurfaceInteraction& isect) const = 0;
    virtual Color Le(const SurfaceInteraction& isect) const { return Color(0, 0, 0); }
    virtual bool is_emissive() const { return false; }
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
        return new LambertianBSDF(albedo, isect.normal);
    }
};

// Emissive material (area light)
class EmissiveMaterial : public Material {
public:
    Color emission;

    EmissiveMaterial(const Color& emit) : emission(emit) {}

    BSDF* get_bsdf(const SurfaceInteraction& isect) const override {
        // Emissive surfaces don't scatter light
        return nullptr;
    }

    Color Le(const SurfaceInteraction& isect) const override {
        return emission;
    }

    bool is_emissive() const override {
        return true;
    }
};

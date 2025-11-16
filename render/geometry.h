#pragma once

#include "math.h"
#include <vector>

// Forward declarations
struct Material;

// Surface interaction data
struct SurfaceInteraction {
    Vec3 point;
    Vec3 normal;
    Vec2 uv;
    float t;
    const Material* material;

    SurfaceInteraction() : t(1e30f), material(nullptr) {}
};

// Abstract primitive interface
class Primitive {
public:
    virtual ~Primitive() {}
    virtual bool intersect(const Ray& ray, SurfaceInteraction* isect) const = 0;
    virtual Bounds3 world_bound() const = 0;
};

// Triangle primitive using Möller-Trumbore intersection algorithm
class Triangle : public Primitive {
public:
    Vec3 v0, v1, v2;        // Vertices
    Vec3 n0, n1, n2;        // Normals
    Vec2 uv0, uv1, uv2;     // Texture coordinates
    const Material* material;

    Triangle(const Vec3& v0, const Vec3& v1, const Vec3& v2,
             const Vec3& n0, const Vec3& n1, const Vec3& n2,
             const Vec2& uv0, const Vec2& uv1, const Vec2& uv2,
             const Material* mat = nullptr)
        : v0(v0), v1(v1), v2(v2)
        , n0(n0), n1(n1), n2(n2)
        , uv0(uv0), uv1(uv1), uv2(uv2)
        , material(mat) {}

    bool intersect(const Ray& ray, SurfaceInteraction* isect) const override {
        const float EPSILON = 0.0000001f;

        Vec3 edge1 = v1 - v0;
        Vec3 edge2 = v2 - v0;
        Vec3 h = cross(ray.direction, edge2);
        float a = dot(edge1, h);

        if (a > -EPSILON && a < EPSILON)
            return false; // Ray parallel to triangle

        float f = 1.0f / a;
        Vec3 s = ray.origin - v0;
        float u = f * dot(s, h);

        if (u < 0.0f || u > 1.0f)
            return false;

        Vec3 q = cross(s, edge1);
        float v = f * dot(ray.direction, q);

        if (v < 0.0f || u + v > 1.0f)
            return false;

        float t = f * dot(edge2, q);

        if (t > EPSILON && t < isect->t) {
            isect->t = t;
            isect->point = ray.at(t);

            // Interpolate normal
            float w = 1.0f - u - v;
            isect->normal = (n0 * w + n1 * u + n2 * v).normalized();

            // Interpolate UV
            isect->uv = Vec2(
                uv0.x * w + uv1.x * u + uv2.x * v,
                uv0.y * w + uv1.y * u + uv2.y * v
            );

            isect->material = material;
            return true;
        }

        return false;
    }

    Bounds3 world_bound() const override {
        Bounds3 bounds(v0);
        bounds.expand(v1);
        bounds.expand(v2);
        return bounds;
    }
};

// Plane primitive (infinite or large)
class Plane : public Primitive {
public:
    Vec3 point;
    Vec3 normal;
    const Material* material;

    Plane(const Vec3& p, const Vec3& n, const Material* mat = nullptr)
        : point(p), normal(n.normalized()), material(mat) {}

    bool intersect(const Ray& ray, SurfaceInteraction* isect) const override {
        const float EPSILON = 0.0000001f;

        float denom = dot(normal, ray.direction);

        // Ray parallel to plane
        if (fabsf(denom) < EPSILON) {
            return false;
        }

        float t = dot(normal, point - ray.origin) / denom;

        // Behind ray origin or further than current intersection
        if (t < EPSILON || t >= isect->t) {
            return false;
        }

        isect->t = t;
        isect->point = ray.at(t);
        isect->normal = normal;

        // Simple planar UV mapping (project onto tangent plane)
        Vec3 tangent, bitangent;
        coordinate_frame(normal, &tangent, &bitangent);
        isect->uv = Vec2(dot(isect->point, tangent), dot(isect->point, bitangent));

        isect->material = material;
        return true;
    }

    Bounds3 world_bound() const override {
        // Infinite plane - return very large bounds
        const float inf = 1e10f;
        return Bounds3(Vec3(-inf, -inf, -inf), Vec3(inf, inf, inf));
    }
};

// Aggregate primitive - simple list (no acceleration structure)
class PrimitiveList : public Primitive {
public:
    std::vector<Primitive*> primitives;

    ~PrimitiveList() {
        for (auto p : primitives)
            delete p;
    }

    void add(Primitive* prim) {
        primitives.push_back(prim);
    }

    bool intersect(const Ray& ray, SurfaceInteraction* isect) const override {
        bool hit = false;
        for (const auto* prim : primitives) {
            if (prim->intersect(ray, isect))
                hit = true;
        }
        return hit;
    }

    Bounds3 world_bound() const override {
        Bounds3 bounds;
        for (const auto* prim : primitives)
            bounds.expand(prim->world_bound());
        return bounds;
    }
};

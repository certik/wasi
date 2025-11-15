#pragma once

#include "math.h"
#include "camera.h"
#include "scene.h"
#include "texture.h"
#include <cstdio>

// Film - stores rendered image
class Film {
public:
    Image image;

    Film(int width, int height) : image(width, height, 3) {}

    void add_sample(int x, int y, const Color& color) {
        image.set_pixel(x, y, color);
    }

    bool write_image(const char* filename) {
        return image.write_ppm(filename);
    }
};

// Integrator interface
class Integrator {
public:
    virtual ~Integrator() {}
    virtual void render(const Scene& scene, const Camera& camera, Film& film) = 0;
};

// Simple ray tracing integrator with direct lighting only
class SimpleIntegrator : public Integrator {
public:
    void render(const Scene& scene, const Camera& camera, Film& film) override {
        int width = film.image.width;
        int height = film.image.height;

        printf("Rendering %dx%d image...\n", width, height);

        for (int y = 0; y < height; y++) {
            if (y % 50 == 0) {
                printf("Progress: %.1f%%\r", 100.0f * y / height);
                fflush(stdout);
            }

            for (int x = 0; x < width; x++) {
                // Generate ray for pixel center
                Vec2 pixel_coord(x + 0.5f, y + 0.5f);
                Ray ray = camera.generate_ray(pixel_coord, width, height);

                // Trace ray
                Color color = trace(ray, scene);

                film.add_sample(x, y, color);
            }
        }

        printf("Rendering complete!          \n");
    }

private:
    Color trace(const Ray& ray, const Scene& scene) {
        SurfaceInteraction isect;

        // Find closest intersection
        if (!scene.intersect(ray, &isect)) {
            return scene.background;
        }

        // Get material BSDF
        if (!isect.material) {
            return Color(1, 0, 1); // Magenta for missing material
        }

        BSDF* bsdf = isect.material->get_bsdf(isect);
        if (!bsdf) {
            return Color(1, 0, 1);
        }

        // Accumulate lighting from all lights
        Color total_color(0, 0, 0);

        Vec3 wo = -ray.direction; // Direction toward camera

        for (const Light* light : scene.lights) {
            Vec3 wi;
            float pdf;

            // Sample the light
            Color Li = light->sample_Li(isect, &wi, &pdf);

            // Check if light is above surface
            float cos_theta = dot(isect.normal, wi);
            if (cos_theta > 0) {
                // Evaluate BSDF
                Color f = bsdf->f(wo, wi);

                // Add contribution: BSDF * Li * cos(theta)
                total_color += f * Li * cos_theta;
            }
        }

        delete bsdf;

        return total_color;
    }
};

// Path tracing integrator (recursive, with Russian roulette)
class PathIntegrator : public Integrator {
public:
    int max_depth;

    PathIntegrator(int depth = 5) : max_depth(depth) {}

    void render(const Scene& scene, const Camera& camera, Film& film) override {
        int width = film.image.width;
        int height = film.image.height;

        printf("Path tracing %dx%d image (max depth: %d)...\n", width, height, max_depth);

        for (int y = 0; y < height; y++) {
            if (y % 50 == 0) {
                printf("Progress: %.1f%%\r", 100.0f * y / height);
                fflush(stdout);
            }

            for (int x = 0; x < width; x++) {
                Vec2 pixel_coord(x + 0.5f, y + 0.5f);
                Ray ray = camera.generate_ray(pixel_coord, width, height);

                Color color = trace(ray, scene, 0);
                film.add_sample(x, y, color);
            }
        }

        printf("Rendering complete!          \n");
    }

private:
    Color trace(const Ray& ray, const Scene& scene, int depth) {
        if (depth >= max_depth) {
            return Color(0, 0, 0);
        }

        SurfaceInteraction isect;

        if (!scene.intersect(ray, &isect)) {
            return scene.background;
        }

        if (!isect.material) {
            return Color(1, 0, 1);
        }

        BSDF* bsdf = isect.material->get_bsdf(isect);
        if (!bsdf) {
            return Color(1, 0, 1);
        }

        Color total_color(0, 0, 0);
        Vec3 wo = -ray.direction;

        // Direct lighting
        for (const Light* light : scene.lights) {
            Vec3 wi;
            float pdf;
            Color Li = light->sample_Li(isect, &wi, &pdf);

            float cos_theta = dot(isect.normal, wi);
            if (cos_theta > 0) {
                Color f = bsdf->f(wo, wi);
                total_color += f * Li * cos_theta;
            }
        }

        delete bsdf;

        return total_color;
    }
};

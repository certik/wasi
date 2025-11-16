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
    float exposure;

    Film(int width, int height, float exp = 1.0f)
        : image(width, height, 3), exposure(exp) {}

    void add_sample(int x, int y, const Color& color) {
        // Apply tone mapping and exposure
        Color processed = process_pixel(color);
        image.set_pixel(x, y, processed);
    }

    bool write_image(const char* filename) {
        return image.write_ppm(filename);
    }

private:
    Color process_pixel(const Color& raw) const {
        // Apply exposure
        Color exposed = raw * exposure;

        // Reinhard tone mapping: maps [0, inf] to [0, 1]
        Color tone_mapped(
            exposed.x / (exposed.x + 1.0f),
            exposed.y / (exposed.y + 1.0f),
            exposed.z / (exposed.z + 1.0f)
        );

        // Optional: Apply gamma correction for sRGB display (gamma = 2.2)
        Color gamma_corrected(
            powf(tone_mapped.x, 1.0f / 2.2f),
            powf(tone_mapped.y, 1.0f / 2.2f),
            powf(tone_mapped.z, 1.0f / 2.2f)
        );

        return gamma_corrected;
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
    float roulette_prob;
    int spp;  // samples per pixel

    PathIntegrator(int depth = 5, int samples = 16, float rr_prob = 0.7f)
        : max_depth(depth), spp(samples), roulette_prob(rr_prob) {}

    void render(const Scene& scene, const Camera& camera, Film& film) override {
        int width = film.image.width;
        int height = film.image.height;

        printf("Path tracing %dx%d image (max depth: %d, spp: %d)...\n",
               width, height, max_depth, spp);

        for (int y = 0; y < height; y++) {
            if (y % 50 == 0) {
                printf("Progress: %.1f%%\r", 100.0f * y / height);
                fflush(stdout);
            }

            for (int x = 0; x < width; x++) {
                Color pixel_color(0, 0, 0);
                unsigned int seed = (y * width + x) * 1103515245u;

                // Multiple samples per pixel
                for (int s = 0; s < spp; s++) {
                    // Jitter pixel sample for anti-aliasing
                    float jitter_x = rng_float(&seed);
                    float jitter_y = rng_float(&seed);
                    Vec2 pixel_coord(x + jitter_x, y + jitter_y);

                    Ray ray = camera.generate_ray(pixel_coord, width, height);
                    pixel_color += Li(ray, scene, 0, &seed);
                }

                // Average samples
                pixel_color = pixel_color * (1.0f / spp);
                film.add_sample(x, y, pixel_color);
            }
        }

        printf("Rendering complete!          \n");
    }

private:
    Color Li(const Ray& ray, const Scene& scene, int depth, unsigned int* seed) {
        // Base case: max depth reached
        if (depth >= max_depth) {
            return Color(0, 0, 0);
        }

        SurfaceInteraction isect;

        // No intersection - return background
        if (!scene.intersect(ray, &isect)) {
            return scene.background;
        }

        if (!isect.material) {
            return Color(1, 0, 1); // Magenta for missing material
        }

        // If hit an emissive surface, return emission
        if (isect.material->is_emissive()) {
            return isect.material->Le(isect);
        }

        // Get BSDF
        BSDF* bsdf = isect.material->get_bsdf(isect);
        if (!bsdf) {
            return Color(0, 0, 0);
        }

        // Russian roulette termination (after some bounces)
        if (depth > 2) {
            float continue_prob = roulette_prob;
            if (rng_float(seed) > continue_prob) {
                delete bsdf;
                return Color(0, 0, 0);
            }
        }

        Vec3 wo = -ray.direction; // Direction toward camera

        // Sample BSDF for next bounce direction
        Vec3 wi;
        float pdf;
        float u1 = rng_float(seed);
        float u2 = rng_float(seed);
        Color f = bsdf->sample_f(wo, &wi, u1, u2, &pdf);

        delete bsdf;

        // Check if valid sample
        if (pdf == 0.0f) {
            return Color(0, 0, 0);
        }

        // Geometry term
        float cos_theta = abs_dot(wi, isect.normal);
        if (cos_theta == 0.0f) {
            return Color(0, 0, 0);
        }

        // Trace next bounce
        const float epsilon = 0.001f;
        Ray next_ray(isect.point + wi * epsilon, wi);
        Color incoming = Li(next_ray, scene, depth + 1, seed);

        // Monte Carlo estimator: f * Li * cos(theta) / pdf
        Color result = f * incoming * cos_theta / pdf;

        // Account for Russian roulette
        if (depth > 2) {
            result = result * (1.0f / roulette_prob);
        }

        return result;
    }
};

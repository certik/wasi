#pragma once

#include "math.h"
#include <cstdint>
#include <cstring>

// Simple image class for textures
class Image {
public:
    int width, height, channels;
    uint8_t* data;

    Image() : width(0), height(0), channels(0), data(nullptr) {}

    Image(int w, int h, int ch) : width(w), height(h), channels(ch) {
        data = new uint8_t[w * h * ch];
        memset(data, 0, w * h * ch);
    }

    ~Image() {
        delete[] data;
    }

    // Simple PPM loader (very basic, P6 format only)
    static Image* load_ppm(const char* filename) {
        FILE* f = fopen(filename, "rb");
        if (!f) return nullptr;

        char magic[3];
        int w, h, maxval;
        if (fscanf(f, "%2s\n%d %d\n%d\n", magic, &w, &h, &maxval) != 4 ||
            strcmp(magic, "P6") != 0) {
            fclose(f);
            return nullptr;
        }

        Image* img = new Image(w, h, 3);
        fread(img->data, 1, w * h * 3, f);
        fclose(f);
        return img;
    }

    Color get_pixel(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height)
            return Color(0, 0, 0);

        int idx = (y * width + x) * channels;
        return Color(
            data[idx + 0] / 255.0f,
            data[idx + 1] / 255.0f,
            data[idx + 2] / 255.0f
        );
    }

    void set_pixel(int x, int y, const Color& c) {
        if (x < 0 || x >= width || y < 0 || y >= height)
            return;

        int idx = (y * width + x) * channels;
        data[idx + 0] = (uint8_t)(clamp(c.x, 0.0f, 1.0f) * 255.0f);
        data[idx + 1] = (uint8_t)(clamp(c.y, 0.0f, 1.0f) * 255.0f);
        data[idx + 2] = (uint8_t)(clamp(c.z, 0.0f, 1.0f) * 255.0f);
    }

    // Bilinear sampling
    Color sample(float u, float v) const {
        if (!data) return Color(1, 1, 1);

        u = u - floorf(u); // Wrap
        v = v - floorf(v);

        float x = u * (width - 1);
        float y = v * (height - 1);

        int x0 = (int)x;
        int y0 = (int)y;
        int x1 = (x0 + 1) % width;
        int y1 = (y0 + 1) % height;

        float fx = x - x0;
        float fy = y - y0;

        Color c00 = get_pixel(x0, y0);
        Color c10 = get_pixel(x1, y0);
        Color c01 = get_pixel(x0, y1);
        Color c11 = get_pixel(x1, y1);

        Color c0 = c00 * (1 - fx) + c10 * fx;
        Color c1 = c01 * (1 - fx) + c11 * fx;

        return c0 * (1 - fy) + c1 * fy;
    }

    // Write PPM file (P6 format)
    bool write_ppm(const char* filename) const {
        FILE* f = fopen(filename, "wb");
        if (!f) return false;

        fprintf(f, "P6\n%d %d\n255\n", width, height);
        fwrite(data, 1, width * height * channels, f);
        fclose(f);
        return true;
    }
};

// Texture interface
class Texture {
public:
    virtual ~Texture() {}
    virtual Color evaluate(const Vec2& uv) const = 0;
};

// Constant color texture
class ConstantTexture : public Texture {
public:
    Color color;

    ConstantTexture(const Color& c) : color(c) {}

    Color evaluate(const Vec2& uv) const override {
        return color;
    }
};

// Image texture
class ImageTexture : public Texture {
public:
    Image* image;
    bool owns_image;

    ImageTexture(Image* img, bool owns = true) : image(img), owns_image(owns) {}

    ~ImageTexture() {
        if (owns_image)
            delete image;
    }

    Color evaluate(const Vec2& uv) const override {
        if (!image) return Color(1, 1, 1);
        return image->sample(uv.x, uv.y);
    }
};

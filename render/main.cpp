#include "math.h"
#include "geometry.h"
#include "material.h"
#include "light.h"
#include "camera.h"
#include "scene.h"
#include "integrator.h"
#include <cstdio>
#include <cstring>

// Create a simple test scene (Cornell box-like)
Scene* create_test_scene() {
    Scene* scene = new Scene();

    // Create materials
    Material* red_mat = new DiffuseMaterial(Color(0.8f, 0.2f, 0.2f));
    Material* green_mat = new DiffuseMaterial(Color(0.2f, 0.8f, 0.2f));
    Material* white_mat = new DiffuseMaterial(Color(0.8f, 0.8f, 0.8f));

    // Floor
    scene->geometry.add(new Triangle(
        Vec3(-2, -2, -2), Vec3(2, -2, -2), Vec3(2, -2, 2),
        Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 1, 0),
        Vec2(0, 0), Vec2(1, 0), Vec2(1, 1),
        white_mat
    ));
    scene->geometry.add(new Triangle(
        Vec3(-2, -2, -2), Vec3(2, -2, 2), Vec3(-2, -2, 2),
        Vec3(0, 1, 0), Vec3(0, 1, 0), Vec3(0, 1, 0),
        Vec2(0, 0), Vec2(1, 1), Vec2(0, 1),
        white_mat
    ));

    // Ceiling
    scene->geometry.add(new Triangle(
        Vec3(-2, 2, -2), Vec3(2, 2, 2), Vec3(2, 2, -2),
        Vec3(0, -1, 0), Vec3(0, -1, 0), Vec3(0, -1, 0),
        Vec2(0, 0), Vec2(1, 1), Vec2(1, 0),
        white_mat
    ));
    scene->geometry.add(new Triangle(
        Vec3(-2, 2, -2), Vec3(-2, 2, 2), Vec3(2, 2, 2),
        Vec3(0, -1, 0), Vec3(0, -1, 0), Vec3(0, -1, 0),
        Vec2(0, 0), Vec2(0, 1), Vec2(1, 1),
        white_mat
    ));

    // Back wall
    scene->geometry.add(new Triangle(
        Vec3(-2, -2, -2), Vec3(2, 2, -2), Vec3(2, -2, -2),
        Vec3(0, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 1),
        Vec2(0, 0), Vec2(1, 1), Vec2(1, 0),
        white_mat
    ));
    scene->geometry.add(new Triangle(
        Vec3(-2, -2, -2), Vec3(-2, 2, -2), Vec3(2, 2, -2),
        Vec3(0, 0, 1), Vec3(0, 0, 1), Vec3(0, 0, 1),
        Vec2(0, 0), Vec2(0, 1), Vec2(1, 1),
        white_mat
    ));

    // Left wall (red)
    scene->geometry.add(new Triangle(
        Vec3(-2, -2, -2), Vec3(-2, -2, 2), Vec3(-2, 2, 2),
        Vec3(1, 0, 0), Vec3(1, 0, 0), Vec3(1, 0, 0),
        Vec2(0, 0), Vec2(1, 0), Vec2(1, 1),
        red_mat
    ));
    scene->geometry.add(new Triangle(
        Vec3(-2, -2, -2), Vec3(-2, 2, 2), Vec3(-2, 2, -2),
        Vec3(1, 0, 0), Vec3(1, 0, 0), Vec3(1, 0, 0),
        Vec2(0, 0), Vec2(1, 1), Vec2(0, 1),
        red_mat
    ));

    // Right wall (green)
    scene->geometry.add(new Triangle(
        Vec3(2, -2, -2), Vec3(2, 2, 2), Vec3(2, -2, 2),
        Vec3(-1, 0, 0), Vec3(-1, 0, 0), Vec3(-1, 0, 0),
        Vec2(0, 0), Vec2(1, 1), Vec2(1, 0),
        green_mat
    ));
    scene->geometry.add(new Triangle(
        Vec3(2, -2, -2), Vec3(2, 2, -2), Vec3(2, 2, 2),
        Vec3(-1, 0, 0), Vec3(-1, 0, 0), Vec3(-1, 0, 0),
        Vec2(0, 0), Vec2(0, 1), Vec2(1, 1),
        green_mat
    ));

    // Add lights
    scene->add_light(new PointLight(Vec3(0, 1.8f, 0), Color(1, 1, 1), 10.0f));

    return scene;
}

int main(int argc, char** argv) {
    printf("=== Basic Physically Based Renderer ===\n\n");

    // Parse command line arguments
    const char* obj_file = nullptr;
    const char* output_file = "output.ppm";
    int width = 800;
    int height = 600;
    bool use_obj = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            obj_file = argv[++i];
            use_obj = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            height = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -i <file>    Input OBJ file (optional, uses test scene if not provided)\n");
            printf("  -o <file>    Output PPM file (default: output.ppm)\n");
            printf("  -w <width>   Image width (default: 800)\n");
            printf("  -h <height>  Image height (default: 600)\n");
            printf("  --help       Show this help\n");
            return 0;
        }
    }

    // Create or load scene
    Scene* scene;
    if (use_obj && obj_file) {
        printf("Loading OBJ file: %s\n", obj_file);
        Material* default_mat = new DiffuseMaterial(Color(0.7f, 0.7f, 0.7f));
        scene = OBJLoader::load(obj_file, default_mat);

        if (!scene) {
            printf("Failed to load OBJ file, using test scene instead\n");
            scene = create_test_scene();
        } else {
            // Add lights to loaded scene
            scene->add_light(new PointLight(Vec3(5, 5, 5), Color(1, 1, 1), 100.0f));
            scene->add_light(new PointLight(Vec3(-5, 5, 5), Color(1, 1, 1), 50.0f));
        }
    } else {
        printf("Using test scene (Cornell box)\n");
        scene = create_test_scene();
    }

    // Create camera
    Vec3 camera_pos(0, 0, 5);
    Vec3 look_at(0, 0, 0);
    Vec3 up(0, 1, 0);
    float fov = 60.0f;

    PerspectiveCamera camera(camera_pos, look_at, up, fov);

    // Create film
    Film film(width, height);

    // Create integrator
    SimpleIntegrator integrator;

    // Render
    integrator.render(*scene, camera, film);

    // Save image
    printf("Writing image to: %s\n", output_file);
    if (film.write_image(output_file)) {
        printf("Success!\n");
    } else {
        printf("Failed to write image\n");
        return 1;
    }

    // Cleanup
    delete scene;

    return 0;
}

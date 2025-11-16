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
    Material* light_mat = new EmissiveMaterial(Color(15, 15, 15));  // Emissive ceiling light

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

    // Ceiling light (emissive area)
    scene->geometry.add(new Triangle(
        Vec3(-0.5f, 1.99f, -0.5f), Vec3(0.5f, 1.99f, 0.5f), Vec3(0.5f, 1.99f, -0.5f),
        Vec3(0, -1, 0), Vec3(0, -1, 0), Vec3(0, -1, 0),
        Vec2(0, 0), Vec2(1, 1), Vec2(1, 0),
        light_mat
    ));
    scene->geometry.add(new Triangle(
        Vec3(-0.5f, 1.99f, -0.5f), Vec3(-0.5f, 1.99f, 0.5f), Vec3(0.5f, 1.99f, 0.5f),
        Vec3(0, -1, 0), Vec3(0, -1, 0), Vec3(0, -1, 0),
        Vec2(0, 0), Vec2(0, 1), Vec2(1, 1),
        light_mat
    ));

    // Rest of ceiling (white)
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
            printf("  -i <file>    Input file: .obj, .glb, or .gltf (optional, uses test scene if not provided)\n");
            printf("  -o <file>    Output PPM file (default: output.ppm)\n");
            printf("  -w <width>   Image width (default: 800, or from glTF camera)\n");
            printf("  -h <height>  Image height (default: 600, or from glTF camera)\n");
            printf("  --help       Show this help\n");
            printf("\nNotes:\n");
            printf("  - glTF/GLB files load camera, lights, and materials from the scene\n");
            printf("  - OBJ files use auto-camera positioning and add default lights\n");
            return 0;
        }
    }

    // Create or load scene
    Scene* scene;
    Camera* loaded_camera = nullptr;
    Bounds3 obj_bounds;  // For camera positioning
    bool has_obj_bounds = false;

    if (use_obj && obj_file) {
        // Detect file type by extension
        const char* ext = strrchr(obj_file, '.');
        if (ext && (strcmp(ext, ".glb") == 0 || strcmp(ext, ".gltf") == 0)) {
            // Load glTF/GLB file
            GLTFLoader::LoadResult gltf_result = GLTFLoader::load(obj_file);

            if (!gltf_result.scene) {
                printf("Failed to load glTF file, using test scene instead\n");
                scene = create_test_scene();
            } else {
                scene = gltf_result.scene;
                loaded_camera = gltf_result.camera;

                // Use camera dimensions if specified
                if (loaded_camera) {
                    width = gltf_result.width;
                    height = gltf_result.height;
                }
            }
        } else {
            // Load OBJ file
            printf("Loading OBJ file: %s\n", obj_file);
            Material* default_mat = new DiffuseMaterial(Color(0.7f, 0.7f, 0.7f));
            scene = OBJLoader::load(obj_file, default_mat);

            if (!scene) {
                printf("Failed to load OBJ file, using test scene instead\n");
                scene = create_test_scene();
            } else {
                // Get scene bounds to position floor (BEFORE adding plane)
                obj_bounds = scene->geometry.world_bound();
                has_obj_bounds = true;
                float floor_y = obj_bounds.min.y;

                printf("Object bounds: min=(%.2f, %.2f, %.2f), max=(%.2f, %.2f, %.2f)\n",
                       obj_bounds.min.x, obj_bounds.min.y, obj_bounds.min.z,
                       obj_bounds.max.x, obj_bounds.max.y, obj_bounds.max.z);

                // Add floor plane at bottom of scene
                Material* floor_mat = new DiffuseMaterial(
                        Color(0.8078f, 0.6235f, 0.4353f));
                scene->add_material(floor_mat);
                scene->geometry.add(new Plane(Vec3(0, floor_y, 0), Vec3(0, 1, 0), floor_mat));
                printf("Added floor plane at Y=%.2f\n", floor_y);

                // Add lights to loaded scene (these now work with NEE)
                scene->add_light(new PointLight(Vec3(5, 5, 5), Color(1, 1, 1), 100.0f));
                scene->add_light(new PointLight(Vec3(-5, 5, 5), Color(1, 1, 1), 50.0f));
            }
        }
    } else {
        printf("Using test scene (Cornell box)\n");
        scene = create_test_scene();
    }

    // Create camera with automatic positioning or use loaded camera
    Camera* camera = nullptr;

    if (loaded_camera) {
        // Use camera from glTF file
        camera = loaded_camera;
    } else {
        // Auto-position camera or use defaults
        Vec3 camera_pos, look_at, up;
        float fov = 45.0f;

        if (has_obj_bounds) {
        // Center of the bounding box
        Vec3 center = (obj_bounds.min + obj_bounds.max) * 0.5f;

        // Size of the object
        Vec3 size = obj_bounds.max - obj_bounds.min;
        float max_size = fmaxf(fmaxf(size.x, size.y), size.z);

        // Calculate camera distance to fit object in view
        // Using FOV and object size: distance = (size/2) / tan(fov/2)
        float tan_half_fov = tanf(fov * 0.5f * 3.14159265f / 180.0f);
        float distance = (max_size * 1.2f) / (2.0f * tan_half_fov);  // 1.2x for margin

        // Position camera at 30-degree elevation angle, looking at center
        float elevation_angle = 25.0f * 3.14159265f / 180.0f;
        float azimuth_angle = 45.0f * 3.14159265f / 180.0f;

        camera_pos = Vec3(
            center.x + distance * cosf(elevation_angle) * cosf(azimuth_angle),
            center.y + distance * sinf(elevation_angle),
            center.z + distance * cosf(elevation_angle) * sinf(azimuth_angle)
        );

        look_at = center;
        up = Vec3(0, 1, 0);

            printf("Camera: pos=(%.2f, %.2f, %.2f), look_at=(%.2f, %.2f, %.2f), distance=%.2f\n",
                   camera_pos.x, camera_pos.y, camera_pos.z,
                   look_at.x, look_at.y, look_at.z, distance);
        } else {
            // Cornell box - use fixed camera
            camera_pos = Vec3(1.5, 0, 3);
            look_at = Vec3(0, 0, 0);
            up = Vec3(0, 1, 0);
        }

        camera = new PerspectiveCamera(camera_pos, look_at, up, fov);
    }

    // Create film
    Film film(width, height);

    // Create integrator - use PathIntegrator for global illumination
    PathIntegrator integrator(5, 64);  // max_depth=5, spp=64

    // Render
    integrator.render(*scene, *camera, film);

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
    if (camera && camera != loaded_camera) {
        delete camera;
    }
    if (loaded_camera) {
        delete loaded_camera;
    }

    return 0;
}

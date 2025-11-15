#pragma once

#include "math.h"
#include "geometry.h"
#include "material.h"
#include "texture.h"
#include "light.h"
#include <vector>
#include <string>
#include <cstdio>
#include <map>
#include <cstring>

// Scene container
class Scene {
public:
    PrimitiveList geometry;
    std::vector<Light*> lights;
    std::vector<Material*> materials;
    Color background;

    Scene() : background(0.1f, 0.1f, 0.2f) {}

    ~Scene() {
        for (auto* light : lights)
            delete light;
        for (auto* mat : materials)
            delete mat;
    }

    void add_light(Light* light) {
        lights.push_back(light);
    }

    void add_material(Material* mat) {
        materials.push_back(mat);
    }

    bool intersect(const Ray& ray, SurfaceInteraction* isect) const {
        return geometry.intersect(ray, isect);
    }
};

// Simple OBJ loader with MTL support
class OBJLoader {
public:
    struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 uv;
    };

    // Load MTL file and create materials
    static std::map<std::string, Material*> load_mtl(const char* filename, const char* base_path, Scene* scene) {
        std::map<std::string, Material*> materials;

        FILE* f = fopen(filename, "r");
        if (!f) {
            printf("Failed to open MTL file: %s\n", filename);
            return materials;
        }

        printf("Loading MTL file: %s\n", filename);

        char line[512];
        std::string current_mat;
        std::string diffuse_map;

        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "newmtl ", 7) == 0) {
                // Save previous material
                if (!current_mat.empty() && !diffuse_map.empty()) {
                    char texture_path[512];
                    snprintf(texture_path, sizeof(texture_path), "%s/%s", base_path, diffuse_map.c_str());

                    Image* img = Image::load(texture_path);
                    if (img) {
                        Texture* tex = new ImageTexture(img, true);
                        Material* mat = new DiffuseMaterial(tex, true);
                        materials[current_mat] = mat;
                        scene->add_material(mat);
                        printf("  Material '%s' -> texture '%s'\n", current_mat.c_str(), diffuse_map.c_str());
                    }
                }

                // Start new material
                char mat_name[256];
                sscanf(line, "newmtl %s", mat_name);
                current_mat = mat_name;
                diffuse_map.clear();
            }
            else if (strncmp(line, "map_Kd ", 7) == 0) {
                // Diffuse texture map
                char tex_name[256];
                sscanf(line, "map_Kd %s", tex_name);
                diffuse_map = tex_name;
            }
        }

        // Save last material
        if (!current_mat.empty() && !diffuse_map.empty()) {
            char texture_path[512];
            snprintf(texture_path, sizeof(texture_path), "%s/%s", base_path, diffuse_map.c_str());

            Image* img = Image::load(texture_path);
            if (img) {
                Texture* tex = new ImageTexture(img, true);
                Material* mat = new DiffuseMaterial(tex, true);
                materials[current_mat] = mat;
                scene->add_material(mat);
                printf("  Material '%s' -> texture '%s'\n", current_mat.c_str(), diffuse_map.c_str());
            }
        }

        fclose(f);
        return materials;
    }

    static Scene* load(const char* filename, const Material* default_material) {
        FILE* f = fopen(filename, "r");
        if (!f) {
            printf("Failed to open OBJ file: %s\n", filename);
            return nullptr;
        }

        // Extract base path from filename
        char base_path[512];
        strncpy(base_path, filename, sizeof(base_path));
        char* last_slash = strrchr(base_path, '/');
        if (last_slash) {
            *last_slash = '\0';
        } else {
            strcpy(base_path, ".");
        }

        std::vector<Vec3> positions;
        std::vector<Vec3> normals;
        std::vector<Vec2> uvs;
        std::vector<int> v_indices, vn_indices, vt_indices;
        std::vector<std::string> mat_indices;

        Scene* scene = new Scene();
        std::map<std::string, Material*> materials;
        std::string current_material;

        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "mtllib ", 7) == 0) {
                // Load MTL file
                char mtl_name[256];
                sscanf(line, "mtllib %s", mtl_name);
                char mtl_path[512];
                snprintf(mtl_path, sizeof(mtl_path), "%s/%s", base_path, mtl_name);
                materials = load_mtl(mtl_path, base_path, scene);
            }
            else if (strncmp(line, "usemtl ", 7) == 0) {
                // Switch to different material
                char mat_name[256];
                sscanf(line, "usemtl %s", mat_name);
                current_material = mat_name;
            }
            else if (line[0] == 'v' && line[1] == ' ') {
                // Vertex position
                Vec3 v;
                sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);
                positions.push_back(v);
            }
            else if (line[0] == 'v' && line[1] == 'n') {
                // Vertex normal
                Vec3 n;
                sscanf(line, "vn %f %f %f", &n.x, &n.y, &n.z);
                normals.push_back(n);
            }
            else if (line[0] == 'v' && line[1] == 't') {
                // Texture coordinate
                Vec2 uv;
                sscanf(line, "vt %f %f", &uv.x, &uv.y);
                uvs.push_back(uv);
            }
            else if (line[0] == 'f' && line[1] == ' ') {
                // Face
                int v[3], vt[3], vn[3];
                int matched = sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                    &v[0], &vt[0], &vn[0],
                    &v[1], &vt[1], &vn[1],
                    &v[2], &vt[2], &vn[2]);

                if (matched == 9) {
                    for (int i = 0; i < 3; i++) {
                        v_indices.push_back(v[i] - 1);
                        vt_indices.push_back(vt[i] - 1);
                        vn_indices.push_back(vn[i] - 1);
                    }
                    mat_indices.push_back(current_material);
                } else {
                    // Try v//vn format
                    matched = sscanf(line, "f %d//%d %d//%d %d//%d",
                        &v[0], &vn[0],
                        &v[1], &vn[1],
                        &v[2], &vn[2]);
                    if (matched == 6) {
                        for (int i = 0; i < 3; i++) {
                            v_indices.push_back(v[i] - 1);
                            vt_indices.push_back(0);
                            vn_indices.push_back(vn[i] - 1);
                        }
                        mat_indices.push_back(current_material);
                    } else {
                        // Try v only
                        matched = sscanf(line, "f %d %d %d", &v[0], &v[1], &v[2]);
                        if (matched == 3) {
                            for (int i = 0; i < 3; i++) {
                                v_indices.push_back(v[i] - 1);
                                vt_indices.push_back(0);
                                vn_indices.push_back(0);
                            }
                            mat_indices.push_back(current_material);
                        }
                    }
                }
            }
        }
        fclose(f);

        // Default UV and normal if not provided
        if (uvs.empty()) uvs.push_back(Vec2(0, 0));
        if (normals.empty()) normals.push_back(Vec3(0, 1, 0));

        // Create triangles
        size_t tri_idx = 0;
        for (size_t i = 0; i < v_indices.size(); i += 3, tri_idx++) {
            Vec3 v0 = positions[v_indices[i + 0]];
            Vec3 v1 = positions[v_indices[i + 1]];
            Vec3 v2 = positions[v_indices[i + 2]];

            Vec3 n0 = (vn_indices[i + 0] < (int)normals.size()) ? normals[vn_indices[i + 0]] : Vec3(0, 1, 0);
            Vec3 n1 = (vn_indices[i + 1] < (int)normals.size()) ? normals[vn_indices[i + 1]] : Vec3(0, 1, 0);
            Vec3 n2 = (vn_indices[i + 2] < (int)normals.size()) ? normals[vn_indices[i + 2]] : Vec3(0, 1, 0);

            Vec2 uv0 = (vt_indices[i + 0] < (int)uvs.size()) ? uvs[vt_indices[i + 0]] : Vec2(0, 0);
            Vec2 uv1 = (vt_indices[i + 1] < (int)uvs.size()) ? uvs[vt_indices[i + 1]] : Vec2(0, 0);
            Vec2 uv2 = (vt_indices[i + 2] < (int)uvs.size()) ? uvs[vt_indices[i + 2]] : Vec2(0, 0);

            // If normals are not provided, compute face normal
            if (vn_indices[i + 0] == 0) {
                Vec3 face_normal = cross(v1 - v0, v2 - v0).normalized();
                n0 = n1 = n2 = face_normal;
            }

            // Find material for this triangle
            const Material* mat = default_material;
            if (tri_idx < mat_indices.size() && !mat_indices[tri_idx].empty()) {
                auto it = materials.find(mat_indices[tri_idx]);
                if (it != materials.end()) {
                    mat = it->second;
                }
            }

            Triangle* tri = new Triangle(v0, v1, v2, n0, n1, n2, uv0, uv1, uv2, mat);
            scene->geometry.add(tri);
        }

        printf("Loaded OBJ: %zu vertices, %zu triangles, %zu materials\n",
               positions.size(), v_indices.size() / 3, materials.size());

        return scene;
    }
};

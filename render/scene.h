#pragma once

#include "math.h"
#include "geometry.h"
#include "material.h"
#include "light.h"
#include <vector>
#include <string>
#include <cstdio>

// Scene container
class Scene {
public:
    PrimitiveList geometry;
    std::vector<Light*> lights;
    Color background;

    Scene() : background(0.1f, 0.1f, 0.2f) {}

    ~Scene() {
        for (auto* light : lights)
            delete light;
    }

    void add_light(Light* light) {
        lights.push_back(light);
    }

    bool intersect(const Ray& ray, SurfaceInteraction* isect) const {
        return geometry.intersect(ray, isect);
    }
};

// Simple OBJ loader (no external dependencies)
class OBJLoader {
public:
    struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 uv;
    };

    static Scene* load(const char* filename, const Material* default_material) {
        FILE* f = fopen(filename, "r");
        if (!f) {
            printf("Failed to open OBJ file: %s\n", filename);
            return nullptr;
        }

        std::vector<Vec3> positions;
        std::vector<Vec3> normals;
        std::vector<Vec2> uvs;
        std::vector<int> v_indices, vn_indices, vt_indices;

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (line[0] == 'v' && line[1] == ' ') {
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
                    } else {
                        // Try v only
                        matched = sscanf(line, "f %d %d %d", &v[0], &v[1], &v[2]);
                        if (matched == 3) {
                            for (int i = 0; i < 3; i++) {
                                v_indices.push_back(v[i] - 1);
                                vt_indices.push_back(0);
                                vn_indices.push_back(0);
                            }
                        }
                    }
                }
            }
        }
        fclose(f);

        // Create scene
        Scene* scene = new Scene();

        // Default UV and normal if not provided
        if (uvs.empty()) uvs.push_back(Vec2(0, 0));
        if (normals.empty()) normals.push_back(Vec3(0, 1, 0));

        // Create triangles
        for (size_t i = 0; i < v_indices.size(); i += 3) {
            Vec3 v0 = positions[v_indices[i + 0]];
            Vec3 v1 = positions[v_indices[i + 1]];
            Vec3 v2 = positions[v_indices[i + 2]];

            Vec3 n0 = (vn_indices[i + 0] < normals.size()) ? normals[vn_indices[i + 0]] : Vec3(0, 1, 0);
            Vec3 n1 = (vn_indices[i + 1] < normals.size()) ? normals[vn_indices[i + 1]] : Vec3(0, 1, 0);
            Vec3 n2 = (vn_indices[i + 2] < normals.size()) ? normals[vn_indices[i + 2]] : Vec3(0, 1, 0);

            Vec2 uv0 = (vt_indices[i + 0] < uvs.size()) ? uvs[vt_indices[i + 0]] : Vec2(0, 0);
            Vec2 uv1 = (vt_indices[i + 1] < uvs.size()) ? uvs[vt_indices[i + 1]] : Vec2(0, 0);
            Vec2 uv2 = (vt_indices[i + 2] < uvs.size()) ? uvs[vt_indices[i + 2]] : Vec2(0, 0);

            // If normals are not provided, compute face normal
            if (vn_indices[i + 0] == 0) {
                Vec3 face_normal = cross(v1 - v0, v2 - v0).normalized();
                n0 = n1 = n2 = face_normal;
            }

            Triangle* tri = new Triangle(v0, v1, v2, n0, n1, n2, uv0, uv1, uv2, default_material);
            scene->geometry.add(tri);
        }

        printf("Loaded OBJ: %zu vertices, %zu triangles\n",
               positions.size(), v_indices.size() / 3);

        return scene;
    }
};

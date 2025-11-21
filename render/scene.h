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

    Scene() : background(0.0f, 0.0f, 0.0f) {}  // Neutral gray for even ambient lighting

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

    // Test visibility between two points (for shadow rays)
    bool visible(const Vec3& p1, const Vec3& p2) const {
        Vec3 direction = p2 - p1;
        float distance = direction.length();
        direction = direction / distance;

        // Create shadow ray with slightly shorter distance to avoid self-intersection at target
        Ray shadow_ray(p1, direction);
        SurfaceInteraction shadow_isect;
        shadow_isect.t = distance - 0.0001f;  // Stop just before target

        // If we hit something before reaching p2, it's occluded
        return !geometry.intersect(shadow_ray, &shadow_isect);
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

// glTF/GLB loader using cgltf
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

class GLTFLoader {
public:
    struct LoadResult {
        Scene* scene;
        Camera* camera;
        int width;
        int height;

        LoadResult() : scene(nullptr), camera(nullptr), width(800), height(600) {}
    };

    static LoadResult load(const char* filename) {
        LoadResult result;

        cgltf_options options = {};
        cgltf_data* data = nullptr;
        cgltf_result res = cgltf_parse_file(&options, filename, &data);

        if (res != cgltf_result_success) {
            printf("Failed to parse glTF file: %s\n", filename);
            return result;
        }

        res = cgltf_load_buffers(&options, data, filename);
        if (res != cgltf_result_success) {
            printf("Failed to load glTF buffers\n");
            cgltf_free(data);
            return result;
        }

        printf("Loading glTF file: %s\n", filename);
        printf("  Meshes: %zu, Nodes: %zu, Cameras: %zu, Lights: %zu, Materials: %zu\n",
               data->meshes_count, data->nodes_count, data->cameras_count, data->lights_count,
               data->materials_count);

        result.scene = new Scene();

        // Load camera from first camera node
        result.camera = load_camera(data, result.width, result.height);

        // Load lights
        load_lights(data, result.scene);

        // Load meshes
        load_meshes(data, result.scene);

        cgltf_free(data);

        printf("glTF load complete\n");
        return result;
    }

private:
    static Camera* load_camera(cgltf_data* data, int& width, int& height) {
        // Find first camera node in scene
        for (cgltf_size i = 0; i < data->nodes_count; i++) {
            cgltf_node* node = &data->nodes[i];
            if (node->camera) {
                // Get world transform
                float matrix[16];
                cgltf_node_transform_world(node, matrix);
                Mat4 transform(matrix);

                Vec3 position = transform.get_translation();
                Vec3 forward = transform.get_forward();
                Vec3 up = transform.get_up();
                Vec3 look_at = position + forward;

                // Extract FOV
                float fov_degrees = 45.0f;
                if (node->camera->type == cgltf_camera_type_perspective) {
                    cgltf_camera_perspective* persp = &node->camera->data.perspective;
                    fov_degrees = persp->yfov * 180.0f / 3.14159265f;  // radians to degrees

                    // Use aspect ratio if specified
                    if (persp->has_aspect_ratio) {
                        width = 800;
                        height = (int)(width / persp->aspect_ratio);
                    }
                }

                printf("Camera: pos=(%.2f, %.2f, %.2f), look_at=(%.2f, %.2f, %.2f), fov=%.1f°\n",
                       position.x, position.y, position.z,
                       look_at.x, look_at.y, look_at.z, fov_degrees);

                return new PerspectiveCamera(position, look_at, up, fov_degrees);
            }
        }

        return nullptr;  // No camera found
    }

    static void load_lights(cgltf_data* data, Scene* scene) {
        for (cgltf_size i = 0; i < data->nodes_count; i++) {
            cgltf_node* node = &data->nodes[i];
            if (node->light) {
                cgltf_light* light = node->light;

                // Get world transform
                float matrix[16];
                cgltf_node_transform_world(node, matrix);
                Mat4 transform(matrix);

                Vec3 position = transform.get_translation();
                Vec3 direction = transform.get_forward();

                Color color(light->color[0], light->color[1], light->color[2]);

                // glTF uses physical units: candela (cd) for point/spot, lux for directional
                // Our renderer expects much smaller values, so scale down
                // Typical glTF lights: 100-10000 cd, our renderer: 10-1000
                float intensity = light->intensity * 0.002f;  // Scale factor tuned for our renderer

                if (light->type == cgltf_light_type_point) {
                    scene->add_light(new PointLight(position, color, intensity));
                    printf("  Point light: pos=(%.2f, %.2f, %.2f), intensity=%.1f (glTF: %.1f cd)\n",
                           position.x, position.y, position.z, intensity, light->intensity);
                } else if (light->type == cgltf_light_type_directional) {
                    scene->add_light(new DirectionalLight(direction, color, intensity));
                    printf("  Directional light: dir=(%.2f, %.2f, %.2f), intensity=%.1f (glTF: %.1f lux)\n",
                           direction.x, direction.y, direction.z, intensity, light->intensity);
                } else if (light->type == cgltf_light_type_spot) {
                    // Treat spot as point for now
                    scene->add_light(new PointLight(position, color, intensity));
                    printf("  Spot light (as point): pos=(%.2f, %.2f, %.2f), intensity=%.1f (glTF: %.1f cd)\n",
                           position.x, position.y, position.z, intensity, light->intensity);
                }
            }
        }
    }

    static void load_meshes(cgltf_data* data, Scene* scene) {
        Material* default_mat = new DiffuseMaterial(Color(0.7f, 0.7f, 0.7f));
        scene->add_material(default_mat);

        int triangle_count = 0;

        for (cgltf_size node_idx = 0; node_idx < data->nodes_count; node_idx++) {
            cgltf_node* node = &data->nodes[node_idx];
            if (!node->mesh) continue;

            // Get world transform for this node
            float matrix[16];
            cgltf_node_transform_world(node, matrix);
            Mat4 transform(matrix);

            cgltf_mesh* mesh = node->mesh;

            for (cgltf_size prim_idx = 0; prim_idx < mesh->primitives_count; prim_idx++) {
                cgltf_primitive* primitive = &mesh->primitives[prim_idx];

                if (primitive->type != cgltf_primitive_type_triangles) {
                    continue;  // Only handle triangles
                }

                // Find position, normal, texcoord attributes
                cgltf_accessor* pos_accessor = nullptr;
                cgltf_accessor* norm_accessor = nullptr;
                cgltf_accessor* uv_accessor = nullptr;

                for (cgltf_size attr_idx = 0; attr_idx < primitive->attributes_count; attr_idx++) {
                    cgltf_attribute* attr = &primitive->attributes[attr_idx];
                    if (attr->type == cgltf_attribute_type_position) {
                        pos_accessor = attr->data;
                    } else if (attr->type == cgltf_attribute_type_normal) {
                        norm_accessor = attr->data;
                    } else if (attr->type == cgltf_attribute_type_texcoord) {
                        uv_accessor = attr->data;
                    }
                }

                if (!pos_accessor) continue;

                // Get material
                Material* mat = default_mat;
                if (primitive->material) {
                    mat = load_material(primitive->material, scene);
                    printf("      Primitive %zu: using material '%s' (%s)\n",
                           prim_idx,
                           primitive->material->name ? primitive->material->name : "unnamed",
                           mat->is_emissive() ? "EMISSIVE" : "diffuse");
                }

                // Load indices or generate them
                cgltf_accessor* ind_accessor = primitive->indices;

                if (ind_accessor) {
                    // Indexed geometry
                    for (cgltf_size i = 0; i < ind_accessor->count; i += 3) {
                        int idx0 = (int)cgltf_accessor_read_index(ind_accessor, i + 0);
                        int idx1 = (int)cgltf_accessor_read_index(ind_accessor, i + 1);
                        int idx2 = (int)cgltf_accessor_read_index(ind_accessor, i + 2);

                        Vec3 v0, v1, v2, n0, n1, n2;
                        Vec2 uv0(0, 0), uv1(0, 0), uv2(0, 0);

                        cgltf_accessor_read_float(pos_accessor, idx0, &v0.x, 3);
                        cgltf_accessor_read_float(pos_accessor, idx1, &v1.x, 3);
                        cgltf_accessor_read_float(pos_accessor, idx2, &v2.x, 3);

                        v0 = transform.transform_point(v0);
                        v1 = transform.transform_point(v1);
                        v2 = transform.transform_point(v2);

                        if (norm_accessor) {
                            cgltf_accessor_read_float(norm_accessor, idx0, &n0.x, 3);
                            cgltf_accessor_read_float(norm_accessor, idx1, &n1.x, 3);
                            cgltf_accessor_read_float(norm_accessor, idx2, &n2.x, 3);
                            n0 = transform.transform_vector(n0).normalized();
                            n1 = transform.transform_vector(n1).normalized();
                            n2 = transform.transform_vector(n2).normalized();
                        } else {
                            Vec3 face_normal = cross(v1 - v0, v2 - v0).normalized();
                            n0 = n1 = n2 = face_normal;
                        }

                        if (uv_accessor) {
                            cgltf_accessor_read_float(uv_accessor, idx0, &uv0.x, 2);
                            cgltf_accessor_read_float(uv_accessor, idx1, &uv1.x, 2);
                            cgltf_accessor_read_float(uv_accessor, idx2, &uv2.x, 2);
                        }

                        scene->geometry.add(new Triangle(v0, v1, v2, n0, n1, n2, uv0, uv1, uv2, mat));
                        triangle_count++;
                    }
                }
            }
        }

        printf("  Loaded %d triangles\n", triangle_count);
    }

    static Material* load_material(cgltf_material* gltf_mat, Scene* scene) {
        const char* mat_name = gltf_mat->name ? gltf_mat->name : "unnamed";

        // Check for emissive material (area light)
        Color emissive(gltf_mat->emissive_factor[0],
                       gltf_mat->emissive_factor[1],
                       gltf_mat->emissive_factor[2]);

        if (emissive.x > 0.0f || emissive.y > 0.0f || emissive.z > 0.0f) {
            printf("    Material '%s': emissive factor (%.2f, %.2f, %.2f)\n",
                   mat_name, emissive.x, emissive.y, emissive.z);
            Material* mat = new EmissiveMaterial(emissive);
            scene->add_material(mat);
            return mat;
        }

        // Try to load baseColorTexture from PBR metallic-roughness
        if (gltf_mat->has_pbr_metallic_roughness) {
            cgltf_pbr_metallic_roughness* pbr = &gltf_mat->pbr_metallic_roughness;

            if (pbr->base_color_texture.texture && pbr->base_color_texture.texture->image) {
                cgltf_image* img = pbr->base_color_texture.texture->image;

                Image* image = nullptr;

                if (img->uri) {
                    // External image file
                    printf("    Material '%s': loading external texture '%s'\n", mat_name, img->uri);
                    image = Image::load(img->uri);
                } else if (img->buffer_view) {
                    // Embedded image in GLB
                    printf("    Material '%s': loading embedded texture (%s, %zu bytes)\n",
                           mat_name, img->mime_type ? img->mime_type : "unknown",
                           img->buffer_view->size);

                    // Get buffer view data
                    const uint8_t* data = (const uint8_t*)img->buffer_view->buffer->data;
                    data += img->buffer_view->offset;
                    cgltf_size size = img->buffer_view->size;

                    // Use stb_image to load from memory
                    Image* embedded_img = new Image();
                    embedded_img->data = stbi_load_from_memory(data, (int)size,
                                                                &embedded_img->width,
                                                                &embedded_img->height,
                                                                &embedded_img->channels, 0);
                    if (embedded_img->data) {
                        printf("    Material '%s': embedded texture loaded (%dx%d, %d channels)\n",
                               mat_name, embedded_img->width, embedded_img->height, embedded_img->channels);
                        image = embedded_img;
                    } else {
                        printf("    Material '%s': failed to decode embedded texture\n", mat_name);
                        delete embedded_img;
                    }
                }

                if (image) {
                    Texture* tex = new ImageTexture(image, true);
                    Material* mat = new DiffuseMaterial(tex, true);
                    scene->add_material(mat);
                    return mat;
                }
            }

            // Use base color factor if no texture
            Color base_color(
                pbr->base_color_factor[0],
                pbr->base_color_factor[1],
                pbr->base_color_factor[2]
            );
            printf("    Material '%s': using base color factor (%.2f, %.2f, %.2f)\n",
                   mat_name, base_color.x, base_color.y, base_color.z);
            Material* mat = new DiffuseMaterial(base_color);
            scene->add_material(mat);
            return mat;
        }

        // Default gray material
        printf("    Material '%s': using default gray\n", mat_name);
        Material* mat = new DiffuseMaterial(Color(0.7f, 0.7f, 0.7f));
        scene->add_material(mat);
        return mat;
    }
};

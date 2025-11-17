#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cgltf.h"  // Assume cgltf.h is included in the project (single-header glTF parser)

typedef struct {
    float min[3];
    float max[3];
} Bounds;

void init_bounds(Bounds* bounds) {
    bounds->min[0] = bounds->min[1] = bounds->min[2] = FLT_MAX;
    bounds->max[0] = bounds->max[1] = bounds->max[2] = -FLT_MAX;
}

void update_bounds(Bounds* bounds, float x, float y, float z) {
    if (x < bounds->min[0]) bounds->min[0] = x;
    if (y < bounds->min[1]) bounds->min[1] = y;
    if (z < bounds->min[2]) bounds->min[2] = z;
    if (x > bounds->max[0]) bounds->max[0] = x;
    if (y > bounds->max[1]) bounds->max[1] = y;
    if (z > bounds->max[2]) bounds->max[2] = z;
}

void transform_point(float mat[16], float* x, float* y, float* z) {
    float tx = mat[0] * (*x) + mat[4] * (*y) + mat[8] * (*z) + mat[12];
    float ty = mat[1] * (*x) + mat[5] * (*y) + mat[9] * (*z) + mat[13];
    float tz = mat[2] * (*x) + mat[6] * (*y) + mat[10] * (*z) + mat[14];
    *x = tx; *y = ty; *z = tz;
}

void compute_node_bounds(cgltf_node* node, Bounds* scene_bounds) {
    if (node->mesh) {
        float world_mat[16];
        cgltf_node_transform_world(node, world_mat);
        
        for (cgltf_size m = 0; m < node->mesh->primitives_count; ++m) {
            cgltf_primitive* prim = &node->mesh->primitives[m];
            for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
                if (prim->attributes[a].type == cgltf_attribute_type_position) {
                    cgltf_accessor* acc = prim->attributes[a].data;
                    // Transform the 8 corners of the local AABB
                    float corners[8][3] = {
                        {acc->min[0], acc->min[1], acc->min[2]},
                        {acc->min[0], acc->min[1], acc->max[2]},
                        {acc->min[0], acc->max[1], acc->min[2]},
                        {acc->min[0], acc->max[1], acc->max[2]},
                        {acc->max[0], acc->min[1], acc->min[2]},
                        {acc->max[0], acc->min[1], acc->max[2]},
                        {acc->max[0], acc->max[1], acc->min[2]},
                        {acc->max[0], acc->max[1], acc->max[2]}
                    };
                    for (int c = 0; c < 8; ++c) {
                        float px = corners[c][0], py = corners[c][1], pz = corners[c][2];
                        transform_point(world_mat, &px, &py, &pz);
                        update_bounds(scene_bounds, px, py, pz);
                    }
                    break;
                }
            }
        }
    }
    for (cgltf_size i = 0; i < node->children_count; ++i) {
        compute_node_bounds(node->children[i], scene_bounds);
    }
}

void compute_scene_bounds(cgltf_data* data, Bounds* bounds) {
    init_bounds(bounds);
    cgltf_scene* scene = data->scene ? data->scene : &data->scenes[0];
    for (cgltf_size i = 0; i < scene->nodes_count; ++i) {
        compute_node_bounds(scene->nodes[i], bounds);
    }
}

void write_indent(FILE* fp, int level) {
    for (int i = 0; i < level; ++i) fprintf(fp, "    ");
}

void write_transform(FILE* fp, cgltf_node* node, int level) {
    if (node->has_matrix) {
        write_indent(fp, level);
        fprintf(fp, "matrix4d xformOp:transform = ( ");
        for (int r = 0; r < 4; ++r) {
            fprintf(fp, "(%.6f, %.6f, %.6f, %.6f)", node->matrix[4*r+0], node->matrix[4*r+1], node->matrix[4*r+2], node->matrix[4*r+3]);
            if (r < 3) fprintf(fp, ", ");
        }
        fprintf(fp, " )\n");
        write_indent(fp, level);
        fprintf(fp, "uniform token[] xformOpOrder = [\"xformOp:transform\"]\n");
    } else {
        if (node->has_translation) {
            write_indent(fp, level);
            fprintf(fp, "double3 xformOp:translate = (%.6f, %.6f, %.6f)\n", node->translation[0], node->translation[1], node->translation[2]);
        }
        if (node->has_rotation) {
            // Convert quaternion to Euler (simple approx, assume XYZ)
            float rx = atan2(2*(node->rotation[3]*node->rotation[0] + node->rotation[1]*node->rotation[2]), 1 - 2*(node->rotation[0]*node->rotation[0] + node->rotation[1]*node->rotation[1])) * 180/M_PI;
            float ry = asin(2*(node->rotation[3]*node->rotation[1] - node->rotation[2]*node->rotation[0])) * 180/M_PI;
            float rz = atan2(2*(node->rotation[3]*node->rotation[2] + node->rotation[0]*node->rotation[1]), 1 - 2*(node->rotation[1]*node->rotation[1] + node->rotation[2]*node->rotation[2])) * 180/M_PI;
            write_indent(fp, level);
            fprintf(fp, "float3 xformOp:rotateXYZ = (%.6f, %.6f, %.6f)\n", rx, ry, rz);
        }
        if (node->has_scale) {
            write_indent(fp, level);
            fprintf(fp, "double3 xformOp:scale = (%.6f, %.6f, %.6f)\n", node->scale[0], node->scale[1], node->scale[2]);
        }
        write_indent(fp, level);
        fprintf(fp, "uniform token[] xformOpOrder = [");
        if (node->has_translation) fprintf(fp, "\"xformOp:translate\", ");
        if (node->has_rotation) fprintf(fp, "\"xformOp:rotateXYZ\", ");
        if (node->has_scale) fprintf(fp, "\"xformOp:scale\"");
        fprintf(fp, "]\n");
    }
}

void write_accessor_data(FILE* fp, cgltf_accessor* acc, const char* type, int level) {
    if (!acc || !acc->buffer_view || !acc->buffer_view->buffer->data) return;
    
    void* data = (char*)acc->buffer_view->buffer->data + acc->buffer_view->offset + acc->offset;
    size_t stride = acc->stride ? acc->stride : acc->buffer_view->stride;
    
    write_indent(fp, level);
    fprintf(fp, "%s[] %s = [", type, acc->normalized ? "norm " : "");
    for (cgltf_size i = 0; i < acc->count; ++i) {
        if (i > 0) fprintf(fp, ", ");
        if (acc->component_type == cgltf_component_type_r_32f) {
            float* val = (float*)((char*)data + i * stride);
            if (acc->type == cgltf_type_vec3) fprintf(fp, "(%.6f, %.6f, %.6f)", val[0], val[1], val[2]);
            else if (acc->type == cgltf_type_vec2) fprintf(fp, "(%.6f, %.6f)", val[0], val[1]);
        } else if (acc->component_type == cgltf_component_type_r_16u) {
            // Add more types as needed
        } // Assume float for simplicity
    }
    fprintf(fp, "]\n");
}

void write_material(FILE* fp, cgltf_material* mat, int mat_index, int level) {
    if (!mat) return;
    write_indent(fp, level);
    fprintf(fp, "def Material \"Material_%d\"\n", mat_index);
    write_indent(fp, level);
    fprintf(fp, "{\n");
    write_indent(fp, level + 1);
    fprintf(fp, "token outputs:surface.connect = </root/Materials/Material_%d/PreviewSurface.outputs:surface>\n", mat_index);
    write_indent(fp, level + 1);
    fprintf(fp, "def Shader \"PreviewSurface\"\n");
    write_indent(fp, level + 1);
    fprintf(fp, "{\n");
    write_indent(fp, level + 2);
    fprintf(fp, "uniform token info:id = \"UsdPreviewSurface\"\n");
    if (mat->has_pbr_metallic_roughness) {
        write_indent(fp, level + 2);
        fprintf(fp, "color3f inputs:diffuseColor = (%.6f, %.6f, %.6f)\n", mat->pbr_metallic_roughness.base_color_factor[0],
                mat->pbr_metallic_roughness.base_color_factor[1], mat->pbr_metallic_roughness.base_color_factor[2]);
        write_indent(fp, level + 2);
        fprintf(fp, "float inputs:metallic = %.6f\n", mat->pbr_metallic_roughness.metallic_factor);
        write_indent(fp, level + 2);
        fprintf(fp, "float inputs:roughness = %.6f\n", mat->pbr_metallic_roughness.roughness_factor);
    }
    write_indent(fp, level + 2);
    fprintf(fp, "token outputs:surface\n");
    write_indent(fp, level + 1);
    fprintf(fp, "}\n");
    write_indent(fp, level);
    fprintf(fp, "}\n");
}

void write_mesh(FILE* fp, cgltf_mesh* mesh, cgltf_material* mat, int mat_index, int level) {
    for (cgltf_size p = 0; p < mesh->primitives_count; ++p) {  // For each submesh
        cgltf_primitive* prim = &mesh->primitives[p];
        write_indent(fp, level);
        fprintf(fp, "def Mesh \"Submesh_%zu\"\n", p);
        write_indent(fp, level);
        fprintf(fp, "{\n");
        
        // Points
        cgltf_accessor* pos_acc = NULL;
        cgltf_accessor* norm_acc = NULL;
        cgltf_accessor* uv_acc = NULL;
        for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
            if (prim->attributes[a].type == cgltf_attribute_type_position) pos_acc = prim->attributes[a].data;
            if (prim->attributes[a].type == cgltf_attribute_type_normal) norm_acc = prim->attributes[a].data;
            if (prim->attributes[a].type == cgltf_attribute_type_texcoord) uv_acc = prim->attributes[a].data;
        }
        if (pos_acc) {
            write_indent(fp, level + 1);
            fprintf(fp, "point3f[] points = [");
            // Note: For large meshes, this prints all vertices - in practice, use loop to print in batches
            void* data = (char*)pos_acc->buffer_view->buffer->data + pos_acc->buffer_view->offset + pos_acc->offset;
            size_t stride = pos_acc->stride ? pos_acc->stride : pos_acc->buffer_view->stride;
            for (cgltf_size i = 0; i < pos_acc->count; ++i) {
                if (i > 0) fprintf(fp, ", ");
                float* val = (float*)((char*)data + i * stride);
                fprintf(fp, "(%.6f, %.6f, %.6f)", val[0], val[1], val[2]);
            }
            fprintf(fp, "]\n");
        }
        
        // Normals
        if (norm_acc) {
            write_indent(fp, level + 1);
            fprintf(fp, "normal3f[] normals = [");
            void* data = (char*)norm_acc->buffer_view->buffer->data + norm_acc->buffer_view->offset + norm_acc->offset;
            size_t stride = norm_acc->stride ? norm_acc->stride : norm_acc->buffer_view->stride;
            for (cgltf_size i = 0; i < norm_acc->count; ++i) {
                if (i > 0) fprintf(fp, ", ");
                float* val = (float*)((char*)data + i * stride);
                fprintf(fp, "(%.6f, %.6f, %.6f)", val[0], val[1], val[2]);
            }
            fprintf(fp, "] (interpolation = \"vertex\")\n");
        }
        
        // UVs
        if (uv_acc) {
            write_indent(fp, level + 1);
            fprintf(fp, "texCoord2f[] primvars:st = [");
            void* data = (char*)uv_acc->buffer_view->buffer->data + uv_acc->buffer_view->offset + uv_acc->offset;
            size_t stride = uv_acc->stride ? uv_acc->stride : uv_acc->buffer_view->stride;
            for (cgltf_size i = 0; i < uv_acc->count; ++i) {
                if (i > 0) fprintf(fp, ", ");
                float* val = (float*)((char*)data + i * stride);
                fprintf(fp, "(%.6f, %.6f)", val[0], 1.0f - val[1]);  // Flip Y for USD
            }
            fprintf(fp, "] (interpolation = \"vertex\")\n");
        }
        
        // Indices and face counts
        if (prim->type == cgltf_primitive_type_triangles) {
            write_indent(fp, level + 1);
            fprintf(fp, "int[] faceVertexCounts = [");
            for (cgltf_size i = 0; i < prim->indices->count / 3; ++i) {
                if (i > 0) fprintf(fp, ", ");
                fprintf(fp, "3");
            }
            fprintf(fp, "]\n");
            
            write_indent(fp, level + 1);
            fprintf(fp, "int[] faceVertexIndices = [");
            void* idx_data = (char*)prim->indices->buffer_view->buffer->data + prim->indices->buffer_view->offset + prim->indices->offset;
            size_t idx_stride = prim->indices->stride;
            for (cgltf_size i = 0; i < prim->indices->count; ++i) {
                if (i > 0) fprintf(fp, ", ");
                if (prim->indices->component_type == cgltf_component_type_r_32u) {
                    uint32_t val = *((uint32_t*)((char*)idx_data + i * idx_stride));
                    fprintf(fp, "%u", val);
                } else if (prim->indices->component_type == cgltf_component_type_r_16u) {
                    uint16_t val = *((uint16_t*)((char*)idx_data + i * idx_stride));
                    fprintf(fp, "%u", val);
                } // Assume unsigned
            }
            fprintf(fp, "]\n");
        }
        
        write_indent(fp, level + 1);
        fprintf(fp, "uniform token subdivisionScheme = \"none\"\n");
        
        if (mat) {
            write_indent(fp, level + 1);
            fprintf(fp, "rel material:binding = </root/Materials/Material_%d>\n", mat_index);
        }
        
        write_indent(fp, level);
        fprintf(fp, "}\n");
    }
}

void write_node_recursive(FILE* fp, cgltf_node* node, int level, cgltf_data* data) {
    write_indent(fp, level);
    if (node->name) {
        fprintf(fp, "def Xform \"%s\"\n", node->name);
    } else {
        fprintf(fp, "def Xform \"Node_%p\"\n", (void*)node);  // Unique name
    }
    write_indent(fp, level);
    fprintf(fp, "{\n");
    
    write_transform(fp, node, level + 1);
    
    if (node->mesh) {
        int mat_index = -1;
        if (node->mesh->primitives_count > 0 && node->mesh->primitives[0].material) {
            for (cgltf_size m = 0; m < data->materials_count; ++m) {
                if (&data->materials[m] == node->mesh->primitives[0].material) {  // Assume same mat for all prims
                    mat_index = m;
                    break;
                }
            }
        }
        write_mesh(fp, node->mesh, node->mesh->primitives[0].material, mat_index, level + 1);
    }
    
    for (cgltf_size i = 0; i < node->children_count; ++i) {
        write_node_recursive(fp, node->children[i], level + 1, data);
    }
    
    write_indent(fp, level);
    fprintf(fp, "}\n");
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s input.glb output.usda\n", argv[0]);
        return 1;
    }
    
    const char* glb_path = argv[1];
    const char* usda_path = argv[2];
    
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, glb_path, &data);
    if (result != cgltf_result_success) {
        printf("Failed to parse glTF\n");
        return 1;
    }
    result = cgltf_load_buffers(&options, data, glb_path);  // dirname from glb_path
    if (result != cgltf_result_success) {
        printf("Failed to load buffers\n");
        cgltf_free(data);
        return 1;
    }
    
    FILE* fp = fopen(usda_path, "w");
    if (!fp) {
        printf("Failed to open output file\n");
        cgltf_free(data);
        return 1;
    }
    
    fprintf(fp, "#usda 1.0\n");
    fprintf(fp, "(\n");
    fprintf(fp, "    upAxis = \"Y\"\n");
    fprintf(fp, ")\n\n");
    
    fprintf(fp, "def Xform \"root\"\n");
    fprintf(fp, "{\n");
    
    // Compute bounds
    Bounds bounds;
    compute_scene_bounds(data, &bounds);
    float center[3] = {(bounds.min[0] + bounds.max[0])/2, (bounds.min[1] + bounds.max[1])/2, (bounds.min[2] + bounds.max[2])/2};
    float size_x = (bounds.max[0] - bounds.min[0]) / 2 + 1.0f;  // Padding
    float size_z = (bounds.max[2] - bounds.min[2]) / 2 + 1.0f;
    float height = bounds.max[1] - bounds.min[1];
    float model_y_offset = -bounds.min[1];  // Shift model so bottom is at y=0
    
    // Write plane
    fprintf(fp, "    def Mesh \"plane\"\n");
    fprintf(fp, "    {\n");
    fprintf(fp, "        int[] faceVertexCounts = [4]\n");
    fprintf(fp, "        int[] faceVertexIndices = [0, 1, 2, 3]\n");
    fprintf(fp, "        normal3f[] normals = [(0,1,0), (0,1,0), (0,1,0), (0,1,0)] (interpolation = \"vertex\")\n");
    fprintf(fp, "        point3f[] points = [(%.2f, 0, %.2f), (%.2f, 0, -%.2f), (-%.2f, 0, -%.2f), (-%.2f, 0, %.2f)]\n",
            size_x, size_z, size_x, size_z, size_x, size_z, size_x, size_z);
    fprintf(fp, "        uniform token subdivisionScheme = \"none\"\n");
    fprintf(fp, "    }\n\n");
    
    // Materials scope
    if (data->materials_count > 0) {
        fprintf(fp, "    def Scope \"Materials\"\n");
        fprintf(fp, "    {\n");
        for (cgltf_size m = 0; m < data->materials_count; ++m) {
            write_material(fp, &data->materials[m], m, 2);
        }
        fprintf(fp, "    }\n\n");
    }
    
    // Model with offset
    fprintf(fp, "    def Xform \"model\"\n");
    fprintf(fp, "    {\n");
    fprintf(fp, "        double3 xformOp:translate = (0, %.6f, 0)\n", model_y_offset);
    fprintf(fp, "        uniform token[] xformOpOrder = [\"xformOp:translate\"]\n\n");
    
    // Write glTF scene nodes
    cgltf_scene* scene = data->scene ? data->scene : &data->scenes[0];
    for (cgltf_size i = 0; i < scene->nodes_count; ++i) {
        write_node_recursive(fp, scene->nodes[i], 3, data);
    }
    
    fprintf(fp, "    }\n\n");
    
    // Area light
    fprintf(fp, "    def RectLight \"areaLight\"\n");
    fprintf(fp, "    {\n");
    fprintf(fp, "        color3f inputs:color = (1, 1, 1)\n");
    fprintf(fp, "        float inputs:intensity = 10.0\n");
    fprintf(fp, "        float inputs:height = %.2f\n", size_z * 2);
    fprintf(fp, "        float inputs:width = %.2f\n", size_x * 2);
    fprintf(fp, "        float3 xformOp:rotateXYZ = (-90, 0, 0)\n");  // Face down
    fprintf(fp, "        double3 xformOp:translate = (0, %.2f, 0)\n", height + 2.0f);
    fprintf(fp, "        uniform token[] xformOpOrder = [\"xformOp:translate\", \"xformOp:rotateXYZ\"]\n");
    fprintf(fp, "    }\n\n");
    
    // Camera
    float cam_dist = fmax(size_x, size_z) * 2 + height;
    fprintf(fp, "    def Camera \"camera\"\n");
    fprintf(fp, "    {\n");
    fprintf(fp, "        float2 clippingRange = (0.1, 10000)\n");
    fprintf(fp, "        float focalLength = 35\n");
    fprintf(fp, "        float horizontalAperture = 20.955\n");
    fprintf(fp, "        float verticalAperture = 15.2908\n");
    fprintf(fp, "        token projection = \"perspective\"\n");
    fprintf(fp, "        float3 xformOp:rotateXYZ = (-15, 0, 0)\n");  // Slight tilt
    fprintf(fp, "        double3 xformOp:translate = (%.2f, %.2f, %.2f)\n", center[0], center[1] + height / 2, center[2] + cam_dist);
    fprintf(fp, "        uniform token[] xformOpOrder = [\"xformOp:translate\", \"xformOp:rotateXYZ\"]\n");
    fprintf(fp, "    }\n");
    
    fprintf(fp, "}\n");
    
    fclose(fp);
    cgltf_free(data);
    printf("USD file generated: %s\n", usda_path);
    return 0;
}

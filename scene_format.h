/*
 * Scene Serialization Format
 *
 * Defines the binary format for serialized 3D scenes using offset-based
 * arena approach for zero-copy deserialization via mmap.
 */

#ifndef SCENE_FORMAT_H
#define SCENE_FORMAT_H

#include <stdint.h>

#define SCENE_MAGIC 0x53434E45  // "SCNE"
#define SCENE_VERSION 2

typedef struct {
    float min[3];
    float max[3];
} SceneAABB;

typedef struct {
    float origin[3];      // World-space origin of the SDF grid (voxel center at index 0)
    float voxel_size;     // World-space size of one voxel edge
    uint32_t dim[3];      // Grid resolution (x, y, z)
    float max_distance;   // Clamp distance for empty space (helps bound ray steps)
    uint32_t pad;         // Alignment
} SceneSDFInfo;

// GPU-ready vertex format (matches game.c MapVertex)
typedef struct {
    float position[3];     // x, y, z
    float surface_type;    // Material ID (0-7)
    float uv[2];           // Texture coordinates
    float normal[3];       // Normal vector
} SceneVertex;

// Light data (GPU-compatible layout with padding)
typedef struct {
    float position[3];
    float pad0;
    float color[3];
    float pad1;
} SceneLight;

// Texture reference (path_offset becomes pointer after fixup)
typedef struct {
    uint64_t path_offset;      // Offset to string in string arena
    uint32_t surface_type_id;  // 0-7 material ID
    uint32_t pad;              // Alignment
} SceneTexture;

// Scene file header
typedef struct {
    uint32_t magic;            // SCENE_MAGIC (0x53434E45)
    uint32_t version;          // SCENE_VERSION
    uint64_t total_size;       // Total size of serialized blob

    // Vertex data
    SceneVertex *vertices;     // Pointer to SceneVertex array (offset before fixup)
    uint64_t vertex_size;      // Size in bytes
    uint32_t vertex_count;     // Number of vertices
    uint32_t pad0;

    // Index data
    uint16_t *indices;         // Pointer to uint16_t indices (offset before fixup)
    uint64_t index_size;       // Size in bytes
    uint32_t index_count;      // Number of indices
    uint32_t pad1;

    // Light data
    SceneLight *lights;        // Pointer to SceneLight array (offset before fixup)
    uint64_t light_size;       // Size in bytes
    uint32_t light_count;      // Number of lights
    uint32_t pad2;

    // Texture data
    SceneTexture *textures;    // Pointer to SceneTexture array (offset before fixup)
    uint64_t texture_size;     // Size in bytes
    uint32_t texture_count;    // Number of textures
    uint32_t pad3;

    // String arena (for texture paths, etc.)
    char *strings;             // Pointer to string arena (offset before fixup)
    uint64_t string_size;      // Size of string arena

    // Scene bounds
    SceneAABB bounds;          // World-space AABB for cascade placement

    // Signed distance field (for probe ray marching)
    float *sdf;                // Pointer to SDF float data (offset before fixup)
    uint64_t sdf_size;         // Size of sdf buffer in bytes
    SceneSDFInfo sdf_info;     // Grid metadata
} SceneHeader;

// Blob layout:
// [SceneHeader]
// [SceneVertex array]     ← vertices (offset until pointer fixup)
// [uint16_t indices]      ← indices (offset until pointer fixup)
// [SceneLight array]      ← lights (offset until pointer fixup)
// [SceneTexture array]    ← textures (offset until pointer fixup)
// [String arena]          ← strings (offset until pointer fixup, null-terminated strings)
// [SDF float array]       ← sdf (offset until pointer fixup, dim.x * dim.y * dim.z floats)

#endif // SCENE_FORMAT_H

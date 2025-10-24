#include <base/io.h>
#include <gm.h>

// ============================================================================
// CRC32-based Hash Functions for Mesh Validation
// ============================================================================

// Simple CRC32 implementation (public domain)
static uint32_t crc32(uint32_t crc, const uint8_t *buf, uint64_t len) {
    static const uint32_t table[16] = {
        0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
        0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
        0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
        0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    };
    crc = ~crc;
    while (len--) {
        crc ^= *buf++;
        crc = (crc >> 4) ^ table[crc & 0x0f];
        crc = (crc >> 4) ^ table[crc & 0x0f];
    }
    return ~crc;
}

// Quantize float array to uint32_t for consistent hashing
static void reduced_precision_floats(const float *src, uint32_t count, uint32_t *dst, float scale) {
    for (uint32_t i = 0; i < count; i++) {
        float val = src[i] * scale;
        // Round to nearest integer
        if (val >= 0.0f) {
            val += 0.5f;
        } else {
            val -= 0.5f;
        }
        int64_t ival = (int64_t)val;
        // Clamp to uint32 range (treat negatives as large positives for hash)
        if (ival < 0) {
            dst[i] = (uint32_t)(0x80000000 + (ival & 0x7FFFFFFF));
        } else if (ival > 0xFFFFFFFF) {
            dst[i] = 0xFFFFFFFF;
        } else {
            dst[i] = (uint32_t)ival;
        }
    }
}

// Compute CRC32 hash of float array (reduced precision)
static uint32_t hash_float_array(const float *data, uint32_t float_count, float scale) {
    if (float_count == 0) return 0;

    // Allocate reduced precision buffer (using stack for simplicity)
    uint32_t reduced_precision[float_count];
    reduced_precision_floats(data, float_count, reduced_precision, scale);

    // Hash the reduced precision integers
    uint32_t hash = crc32(0, (uint8_t*)reduced_precision, float_count * sizeof(uint32_t));

    // Include count in hash for robustness
    uint32_t count_val = float_count;
    hash = crc32(hash, (uint8_t*)&count_val, sizeof(count_val));

    return hash;
}

// Compute CRC32 hash of uint16 array
static uint32_t hash_uint16_array(const uint16_t *data, uint32_t count) {
    if (count == 0) return 0;

    uint32_t hash = crc32(0, (uint8_t*)data, count * sizeof(uint16_t));

    // Include count in hash
    hash = crc32(hash, (uint8_t*)&count, sizeof(count));

    return hash;
}

// ============================================================================
// Mesh Data Integrity Test
// ============================================================================

static int test_mesh_data_integrity(const MeshData *mesh) {
    println(str_lit(""));
    println(str_lit("=== Test 3: Mesh data integrity ==="));

    // Expected values for the test map
    const uint32_t EXPECTED_VERTEX_COUNT = 640;
    const uint32_t EXPECTED_POSITION_COUNT = 1920;  // 640 * 3
    const uint32_t EXPECTED_UV_COUNT = 1280;        // 640 * 2
    const uint32_t EXPECTED_NORMAL_COUNT = 1920;    // 640 * 3
    const uint32_t EXPECTED_INDEX_COUNT = 960;

    // Validate counts
    int counts_ok = 1;
    if (mesh->vertex_count != EXPECTED_VERTEX_COUNT) {
        println(str_lit("ERROR: vertex_count = {}, expected {}"),
                (int64_t)mesh->vertex_count, (int64_t)EXPECTED_VERTEX_COUNT);
        counts_ok = 0;
    }
    if (mesh->position_count != EXPECTED_POSITION_COUNT) {
        println(str_lit("ERROR: position_count = {}, expected {}"),
                (int64_t)mesh->position_count, (int64_t)EXPECTED_POSITION_COUNT);
        counts_ok = 0;
    }
    if (mesh->uv_count != EXPECTED_UV_COUNT) {
        println(str_lit("ERROR: uv_count = {}, expected {}"),
                (int64_t)mesh->uv_count, (int64_t)EXPECTED_UV_COUNT);
        counts_ok = 0;
    }
    if (mesh->normal_count != EXPECTED_NORMAL_COUNT) {
        println(str_lit("ERROR: normal_count = {}, expected {}"),
                (int64_t)mesh->normal_count, (int64_t)EXPECTED_NORMAL_COUNT);
        counts_ok = 0;
    }
    if (mesh->index_count != EXPECTED_INDEX_COUNT) {
        println(str_lit("ERROR: index_count = {}, expected {}"),
                (int64_t)mesh->index_count, (int64_t)EXPECTED_INDEX_COUNT);
        counts_ok = 0;
    }

    if (counts_ok) {
        println(str_lit("Counts validation: PASS"));
    } else {
        return 1;
    }

    // Compute hashes
    println(str_lit("Computing hashes..."));

    uint32_t indices_hash = hash_uint16_array(mesh->indices, mesh->index_count);
    uint32_t positions_hash = hash_float_array(mesh->positions, mesh->position_count, 1e5f);
    uint32_t uvs_hash = hash_float_array(mesh->uvs, mesh->uv_count, 1e4f);
    uint32_t normals_hash = hash_float_array(mesh->normals, mesh->normal_count, 1e3f);

    // Expected hash values (captured from correct implementation)
    const uint32_t EXPECTED_INDICES_HASH = 23908648u;
    const uint32_t EXPECTED_POSITIONS_HASH = 2855516617u;
    const uint32_t EXPECTED_UVS_HASH = 3712481616u;
    const uint32_t EXPECTED_NORMALS_HASH = 279143128u;

    // Print hashes
    println(str_lit("  Indices hash:   {}"), (int64_t)indices_hash);
    println(str_lit("  Positions hash: {}"), (int64_t)positions_hash);
    println(str_lit("  UVs hash:       {}"), (int64_t)uvs_hash);
    println(str_lit("  Normals hash:   {}"), (int64_t)normals_hash);

    // Validate hashes
    int hashes_ok = 1;
    if (indices_hash != EXPECTED_INDICES_HASH) {
        println(str_lit("ERROR: Indices hash mismatch! Expected {}"),
                (int64_t)EXPECTED_INDICES_HASH);
        hashes_ok = 0;
    }
    if (positions_hash != EXPECTED_POSITIONS_HASH) {
        println(str_lit("ERROR: Positions hash mismatch! Expected {}"),
                (int64_t)EXPECTED_POSITIONS_HASH);
        hashes_ok = 0;
    }
    if (uvs_hash != EXPECTED_UVS_HASH) {
        println(str_lit("ERROR: UVs hash mismatch! Expected {}"),
                (int64_t)EXPECTED_UVS_HASH);
        hashes_ok = 0;
    }
    if (normals_hash != EXPECTED_NORMALS_HASH) {
        println(str_lit("ERROR: Normals hash mismatch! Expected {}"),
                (int64_t)EXPECTED_NORMALS_HASH);
        hashes_ok = 0;
    }

    if (!hashes_ok) {
        println(str_lit("Hash validation: FAIL"));
        return 1;
    }

    println(str_lit("Hash validation: PASS"));
    return 0;
}

int main(void) {
    // Test map (same as in gm.html)
    int map[MAP_HEIGHT][MAP_WIDTH] = {
        {1,1,1,1,1,1,1,1,1,1},
        {1,7,0,0,0,0,0,0,0,1},
        {1,0,1,2,1,0,2,0,0,1},
        {1,0,1,0,0,0,0,1,0,1},
        {1,0,1,0,1,0,0,0,0,1},
        {1,0,1,0,3,0,1,0,0,1},
        {1,0,3,0,1,1,0,0,1,1},
        {1,0,1,0,1,0,0,0,0,1},
        {1,0,0,0,1,0,0,1,0,1},
        {1,1,1,1,1,1,1,1,1,1}
    };

    float startX, startZ, startYaw;

    println(str_lit("=== Test 1: Finding starting position ==="));

    int found = find_start_position((int*)map, MAP_WIDTH, MAP_HEIGHT,
                                   &startX, &startZ, &startYaw);

    if (found) {
        println(str_lit("Found starting position:"));
        println(str_lit("  X: {}"), (int64_t)(startX * 100.0f));  // Print as fixed-point
        println(str_lit("  Z: {}"), (int64_t)(startZ * 100.0f));
        println(str_lit("  Yaw: {}"), (int64_t)(startYaw * 100.0f));

        // Determine direction name
        const char *direction;
        if (startYaw < -1.0f) {
            direction = "North";
        } else if (startYaw < 1.0f) {
            direction = "East";
        } else if (startYaw < 2.0f) {
            direction = "South";
        } else {
            direction = "West";
        }
        println(str_lit("  Direction: {}"), str_from_cstr_view((char*)direction));
    } else {
        println(str_lit("ERROR: Starting position not found!"));
        return 1;
    }

    println(str_lit(""));
    println(str_lit("=== Test 2: Generating mesh ==="));

    MeshData *mesh = generate_mesh((int*)map, MAP_WIDTH, MAP_HEIGHT);

    if (mesh) {
        println(str_lit("Mesh generated successfully:"));
        println(str_lit("  Vertices: {}"), (int64_t)mesh->vertex_count);
        println(str_lit("  Positions: {} floats"), (int64_t)mesh->position_count);
        println(str_lit("  UVs: {} floats"), (int64_t)mesh->uv_count);
        println(str_lit("  Normals: {} floats"), (int64_t)mesh->normal_count);
        println(str_lit("  Indices: {}"), (int64_t)mesh->index_count);
        println(str_lit("  Triangles: {}"), (int64_t)(mesh->index_count / 3));

        // Sanity checks
        if (mesh->vertex_count == 0) {
            println(str_lit("ERROR: No vertices generated!"));
            return 1;
        }
        if (mesh->index_count == 0) {
            println(str_lit("ERROR: No indices generated!"));
            return 1;
        }
        if (mesh->position_count != mesh->vertex_count * 3) {
            println(str_lit("ERROR: Position count mismatch!"));
            return 1;
        }
        if (mesh->uv_count != mesh->vertex_count * 2) {
            println(str_lit("ERROR: UV count mismatch!"));
            return 1;
        }
        if (mesh->normal_count != mesh->vertex_count * 3) {
            println(str_lit("ERROR: Normal count mismatch!"));
            return 1;
        }

        // Run integrity test
        if (test_mesh_data_integrity(mesh) != 0) {
            return 1;
        }

        println(str_lit(""));
        println(str_lit("=== All tests passed ==="));
        return 0;
    } else {
        println(str_lit("ERROR: Mesh generation failed!"));
        return 1;
    }
}

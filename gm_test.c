#include <base/io.h>
#include <gm.h>

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

        println(str_lit(""));
        println(str_lit("=== All tests passed ==="));
        return 0;
    } else {
        println(str_lit("ERROR: Mesh generation failed!"));
        return 1;
    }
}

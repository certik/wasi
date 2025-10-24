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

    println(str_lit("Finding starting position..."));

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

        return 0;
    } else {
        println(str_lit("ERROR: Starting position not found!"));
        return 1;
    }
}

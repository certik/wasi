#include <base/io.h>
#include <base/wasi.h>
#include <base/exit.h>
#include <base/format.h>

// Map dimensions
#define MAP_WIDTH 10
#define MAP_HEIGHT 10

// Map cell types
// 0 = floor, 1 = wall, 2 = wall with north/south window, 3 = wall with east/west window
// 5-8 = starting position with direction: 5=North, 6=East, 7=South, 8=West

// Math constants
#define PI 3.14159265358979323846

// Find starting position and direction in the map
// Returns: 1 if found, 0 if not found
// Outputs: startX, startZ (position), startYaw (direction in radians), and modifies map
int find_start_position(int *map, int width, int height,
                       float *startX, float *startZ, float *startYaw) {
    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            int cell = map[z * width + x];
            if (cell >= 5 && cell <= 8) {
                *startX = (float)x + 0.5f;
                *startZ = (float)z + 0.5f;

                // Set yaw based on direction
                if (cell == 5) {
                    *startYaw = -PI / 2.0f;  // North
                } else if (cell == 6) {
                    *startYaw = 0.0f;        // East
                } else if (cell == 7) {
                    *startYaw = PI / 2.0f;   // South
                } else { // cell == 8
                    *startYaw = PI;          // West
                }

                // Clear the marker
                map[z * width + x] = 0;

                return 1;
            }
        }
    }
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

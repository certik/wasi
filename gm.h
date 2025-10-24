#pragma once

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
                       float *startX, float *startZ, float *startYaw);

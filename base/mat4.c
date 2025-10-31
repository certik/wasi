#include "mat4.h"
#include <base/base_math.h>
#include <base/mem.h>

mat4 mat4_identity(void) {
    mat4 result;
    base_memset(&result, 0, sizeof(mat4));
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

mat4 mat4_multiply(mat4 a, mat4 b) {
    mat4 result;

    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            float sum = 0.0f;
            for (int k = 0; k < 4; k++) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            result.m[col * 4 + row] = sum;
        }
    }

    return result;
}

mat4 mat4_perspective(float fov_y, float aspect, float near, float far) {
    mat4 result;
    base_memset(&result, 0, sizeof(mat4));

    float tan_half_fov = 1.0f / __builtin_tanf(fov_y / 2.0f);

    result.m[0] = tan_half_fov / aspect;
    result.m[5] = tan_half_fov;
    result.m[10] = far / (near - far);
    result.m[11] = -1.0f;
    result.m[14] = -(far * near) / (far - near);

    return result;
}

mat4 mat4_translate(float x, float y, float z) {
    mat4 result = mat4_identity();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

mat4 mat4_rotate_x(float angle) {
    mat4 result = mat4_identity();
    float c = __builtin_cosf(angle);
    float s = __builtin_sinf(angle);

    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;

    return result;
}

mat4 mat4_rotate_y(float angle) {
    mat4 result = mat4_identity();
    float c = __builtin_cosf(angle);
    float s = __builtin_sinf(angle);

    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;

    return result;
}

mat4 mat4_rotate_z(float angle) {
    mat4 result = mat4_identity();
    float c = __builtin_cosf(angle);
    float s = __builtin_sinf(angle);

    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;

    return result;
}

mat4 mat4_scale(float x, float y, float z) {
    mat4 result = mat4_identity();
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    return result;
}

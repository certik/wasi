#include <base/io.h>
#include <base/arena.h>
#include <base/buddy.h>
#include <base/base_string.h>
#include <platform/platform.h>
#include <base/mem.h>
#include <base/numconv.h>
#include <base/format.h>
#include <base/base_io.h>
#include <base/exit.h>
#include <base/assert.h>

// Vertex with position and normal
typedef struct {
    float x, y, z;     // position
    float nx, ny, nz;  // normal
} Vertex;

// Mesh builder
typedef struct {
    Vertex *vertices;
    uint32_t *indices;
    uint32_t vertex_count;
    uint32_t vertex_capacity;
    uint32_t index_count;
    uint32_t index_capacity;
    Arena *arena;
} MeshBuilder;

static MeshBuilder mesh_builder_new(Arena *arena, uint32_t initial_vertex_capacity, uint32_t initial_index_capacity) {
    MeshBuilder mb;
    mb.arena = arena;
    mb.vertices = arena_alloc_array(arena, Vertex, initial_vertex_capacity);
    mb.indices = arena_alloc_array(arena, uint32_t, initial_index_capacity);
    mb.vertex_count = 0;
    mb.vertex_capacity = initial_vertex_capacity;
    mb.index_count = 0;
    mb.index_capacity = initial_index_capacity;
    return mb;
}

static void mesh_builder_add_vertex(MeshBuilder *mb, float x, float y, float z, float nx, float ny, float nz) {
    assert(mb->vertex_count < mb->vertex_capacity);
    Vertex *v = &mb->vertices[mb->vertex_count++];
    v->x = x; v->y = y; v->z = z;
    v->nx = nx; v->ny = ny; v->nz = nz;
}

static void mesh_builder_add_index(MeshBuilder *mb, uint32_t idx) {
    assert(mb->index_count < mb->index_capacity);
    mb->indices[mb->index_count++] = idx;
}

// Add a box with given bounds and height
static void add_box(MeshBuilder *mb, float x0, float z0, float x1, float z1, float y0, float y1) {
    uint32_t base = mb->vertex_count;

    // Bottom face (y = y0, normal = -Y)
    mesh_builder_add_vertex(mb, x0, y0, z0, 0, -1, 0);
    mesh_builder_add_vertex(mb, x1, y0, z0, 0, -1, 0);
    mesh_builder_add_vertex(mb, x1, y0, z1, 0, -1, 0);
    mesh_builder_add_vertex(mb, x0, y0, z1, 0, -1, 0);

    // Top face (y = y1, normal = +Y)
    mesh_builder_add_vertex(mb, x0, y1, z0, 0, 1, 0);
    mesh_builder_add_vertex(mb, x1, y1, z0, 0, 1, 0);
    mesh_builder_add_vertex(mb, x1, y1, z1, 0, 1, 0);
    mesh_builder_add_vertex(mb, x0, y1, z1, 0, 1, 0);

    // Front face (z = z0, normal = -Z)
    mesh_builder_add_vertex(mb, x0, y0, z0, 0, 0, -1);
    mesh_builder_add_vertex(mb, x1, y0, z0, 0, 0, -1);
    mesh_builder_add_vertex(mb, x1, y1, z0, 0, 0, -1);
    mesh_builder_add_vertex(mb, x0, y1, z0, 0, 0, -1);

    // Back face (z = z1, normal = +Z)
    mesh_builder_add_vertex(mb, x0, y0, z1, 0, 0, 1);
    mesh_builder_add_vertex(mb, x1, y0, z1, 0, 0, 1);
    mesh_builder_add_vertex(mb, x1, y1, z1, 0, 0, 1);
    mesh_builder_add_vertex(mb, x0, y1, z1, 0, 0, 1);

    // Left face (x = x0, normal = -X)
    mesh_builder_add_vertex(mb, x0, y0, z0, -1, 0, 0);
    mesh_builder_add_vertex(mb, x0, y0, z1, -1, 0, 0);
    mesh_builder_add_vertex(mb, x0, y1, z1, -1, 0, 0);
    mesh_builder_add_vertex(mb, x0, y1, z0, -1, 0, 0);

    // Right face (x = x1, normal = +X)
    mesh_builder_add_vertex(mb, x1, y0, z0, 1, 0, 0);
    mesh_builder_add_vertex(mb, x1, y0, z1, 1, 0, 0);
    mesh_builder_add_vertex(mb, x1, y1, z1, 1, 0, 0);
    mesh_builder_add_vertex(mb, x1, y1, z0, 1, 0, 0);

    // Indices for all 6 faces (2 triangles each)
    for (uint32_t face = 0; face < 6; face++) {
        uint32_t f = base + face * 4;
        mesh_builder_add_index(mb, f + 0);
        mesh_builder_add_index(mb, f + 1);
        mesh_builder_add_index(mb, f + 2);
        mesh_builder_add_index(mb, f + 0);
        mesh_builder_add_index(mb, f + 2);
        mesh_builder_add_index(mb, f + 3);
    }
}

// Get height for a character type
static float get_height(char c) {
    switch (c) {
        case '#': return 8.0f;  // wall
        case 'D': return 7.0f;  // door
        case 'W': return 4.0f;  // window (will start at y=3)
        case 'B': return 2.0f;  // bed
        case 'T': return 2.5f;  // table/toilet
        case 'V': return 3.0f;  // vanity
        case 'S': return 2.5f;  // shower/sink
        case 'G': return 7.0f;  // glass door
        case 'C': return 8.0f;  // closet
        case 'P': return 0.5f;  // pillow
        case '_': return 8.0f;  // overhang/ceiling
        default: return 0.0f;   // floor or unknown
    }
}

// Get base height for a character type
static float get_base_height(char c) {
    if (c == 'W') return 4.5f;  // windows start at 4.5 feet
    return 0.0f;
}

// Check if character should have geometry
static bool has_geometry(char c) {
    return c == '#' || c == 'D' || c == 'W' || c == 'B' || c == 'T' ||
           c == 'V' || c == 'S' || c == 'G' || c == 'C' || c == 'P' || c == '_';
}

// Write binary buffer to file
static bool write_buffer(const char *filename, const void *data, size_t size) {
    wasi_fd_t fd = wasi_path_open(filename, base_strlen(filename),
                                    WASI_RIGHTS_WRITE,
                                    WASI_O_CREAT | WASI_O_TRUNC);
    if (fd < 0) return false;

    ciovec_t iov = { .buf = data, .buf_len = size };
    size_t written;
    uint32_t result = wasi_fd_write(fd, &iov, 1, &written);
    wasi_fd_close(fd);
    return result == 0 && written == size;
}

// Write string to file
static bool write_string(const char *filename, const char *str) {
    return write_buffer(filename, str, base_strlen(str));
}

// Simple integer to string without using format
static void int_to_str_simple(char *buf, size_t bufsize, int value) {
    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[32];
    int i = 0;
    int is_neg = value < 0;
    if (is_neg) value = -value;
    while (value > 0 && i < 31) {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }
    int j = 0;
    if (is_neg) buf[j++] = '-';
    while (i > 0 && j < (int)bufsize - 1) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

// Float to string (simple version)
static void float_to_str_simple(char *buf, size_t bufsize, float value, int decimals) {
    int int_part = (int)value;
    float frac = value - int_part;
    if (frac < 0) frac = -frac;

    int_to_str_simple(buf, bufsize, int_part);
    int len = base_strlen(buf);
    if (len + 2 < (int)bufsize) {
        buf[len++] = '.';
        for (int i = 0; i < decimals && len < (int)bufsize - 1; i++) {
            frac *= 10;
            int digit = (int)frac;
            buf[len++] = '0' + digit;
            frac -= digit;
        }
        buf[len] = '\0';
    }
}

// Write glTF JSON with minimal allocations - write directly in place
static void write_gltf_json(const char *filename, uint32_t vertex_count, uint32_t index_count,
                            size_t pos_size, size_t norm_size, size_t idx_size, size_t total_size,
                            float min_x, float min_y, float min_z,
                            float max_x, float max_y, float max_z) {
    // Pre-calculate JSON size (generous estimate)
    const size_t json_size = 4096;
    char *json = (char*)buddy_alloc(json_size, NULL);
    if (!json) return;

    int pos = 0;

    #define APP(s) do{ const char*_s=s; while(*_s && pos<(int)json_size-1) json[pos++]=*_s++; }while(0)
    #define APP_INT(v) do{ char t[32]; int_to_str_simple(t,32,v); APP(t); }while(0)
    #define APP_FLOAT(v) do{ char t[32]; float_to_str_simple(t,32,v,2); APP(t); }while(0)

    APP("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],");
    APP("\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},\"indices\":2}]}],");
    APP("\"accessors\":[");
    APP("{\"bufferView\":0,\"componentType\":5126,\"count\":"); APP_INT(vertex_count);
    APP(",\"type\":\"VEC3\",\"min\":["); APP_FLOAT(min_x); APP(","); APP_FLOAT(min_y); APP(","); APP_FLOAT(min_z);
    APP("],\"max\":["); APP_FLOAT(max_x); APP(","); APP_FLOAT(max_y); APP(","); APP_FLOAT(max_z); APP("]},");
    APP("{\"bufferView\":1,\"componentType\":5126,\"count\":"); APP_INT(vertex_count); APP(",\"type\":\"VEC3\"},");
    APP("{\"bufferView\":2,\"componentType\":5125,\"count\":"); APP_INT(index_count); APP(",\"type\":\"SCALAR\"}],");
    APP("\"bufferViews\":[");
    APP("{\"buffer\":0,\"byteOffset\":0,\"byteLength\":"); APP_INT(pos_size); APP(",\"target\":34962},");
    APP("{\"buffer\":0,\"byteOffset\":"); APP_INT(pos_size); APP(",\"byteLength\":"); APP_INT(norm_size); APP(",\"target\":34962},");
    APP("{\"buffer\":0,\"byteOffset\":"); APP_INT(pos_size+norm_size); APP(",\"byteLength\":"); APP_INT(idx_size); APP(",\"target\":34963}],");
    APP("\"buffers\":[{\"byteLength\":"); APP_INT(total_size); APP(",\"uri\":\"hotel.bin\"}]}\n");

    json[pos] = '\0';
    write_string(filename, json);
    buddy_free(json);
}

// Generate glTF - simplified to minimize allocations
static void generate_gltf(MeshBuilder *mb, const char *bin_filename, const char *gltf_filename) {
    size_t pos_size = mb->vertex_count * 3 * sizeof(float);
    size_t norm_size = mb->vertex_count * 3 * sizeof(float);
    size_t idx_size = mb->index_count * sizeof(uint32_t);
    size_t total_size = pos_size + norm_size + idx_size;

    // Allocate buffer directly from buddy
    uint8_t *buffer = (uint8_t*)buddy_alloc(total_size, NULL);
    if (!buffer) return;

    // Write positions
    float *pos_ptr = (float*)buffer;
    for (uint32_t i = 0; i < mb->vertex_count; i++) {
        pos_ptr[i*3+0] = mb->vertices[i].x;
        pos_ptr[i*3+1] = mb->vertices[i].y;
        pos_ptr[i*3+2] = mb->vertices[i].z;
    }

    // Write normals
    float *norm_ptr = (float*)(buffer + pos_size);
    for (uint32_t i = 0; i < mb->vertex_count; i++) {
        norm_ptr[i*3+0] = mb->vertices[i].nx;
        norm_ptr[i*3+1] = mb->vertices[i].ny;
        norm_ptr[i*3+2] = mb->vertices[i].nz;
    }

    // Write indices
    base_memcpy(buffer + pos_size + norm_size, mb->indices, idx_size);

    // Calculate bounds
    float min_x=pos_ptr[0], max_x=pos_ptr[0];
    float min_y=pos_ptr[1], max_y=pos_ptr[1];
    float min_z=pos_ptr[2], max_z=pos_ptr[2];
    for (uint32_t i=1; i<mb->vertex_count; i++) {
        float x=pos_ptr[i*3], y=pos_ptr[i*3+1], z=pos_ptr[i*3+2];
        if(x<min_x)min_x=x; if(x>max_x)max_x=x;
        if(y<min_y)min_y=y; if(y>max_y)max_y=y;
        if(z<min_z)min_z=z; if(z>max_z)max_z=z;
    }

    write_buffer(bin_filename, buffer, total_size);
    write_gltf_json(gltf_filename, mb->vertex_count, mb->index_count,
                    pos_size, norm_size, idx_size, total_size,
                    min_x, min_y, min_z, max_x, max_y, max_z);

    buddy_free(buffer);
}

int main(void) {
    buddy_init();
    Arena *arena = arena_new(4 * 1024 * 1024);  // 4 MB initial size

    // println(str_lit("Hotel to glTF converter"));

    // Read hotel.txt
    string hotel_text = read_file_ok(arena, str_lit("hotel.txt"));
    // println(str_lit("Read hotel.txt: {} bytes"), (int64_t)hotel_text.size);

    // Skip the legend (first 17 lines) - start parsing from line 18
    const uint32_t skip_lines = 17;
    uint64_t start_idx = 0;
    uint32_t lines_seen = 0;

    for (uint64_t i = 0; i < hotel_text.size && lines_seen < skip_lines; i++) {
        if (hotel_text.str[i] == '\n') {
            lines_seen++;
            if (lines_seen == skip_lines) {
                start_idx = i + 1;
            }
        }
    }

    // Parse grid dimensions from actual floor plan
    uint32_t rows = 0;
    uint32_t cols = 0;
    uint32_t current_col = 0;

    for (uint64_t i = start_idx; i < hotel_text.size; i++) {
        char c = hotel_text.str[i];
        if (c == '\n') {
            rows++;
            if (current_col > cols) cols = current_col;
            current_col = 0;
        } else {
            current_col++;
        }
    }
    if (current_col > 0) {
        rows++;
        if (current_col > cols) cols = current_col;
    }

    // Create grid array
    char *grid = arena_alloc_array(arena, char, rows * cols);
    base_memset(grid, ' ', rows * cols);

    uint32_t row = 0, col = 0;
    for (uint64_t i = start_idx; i < hotel_text.size; i++) {
        char c = hotel_text.str[i];
        if (c == '\n') {
            row++;
            col = 0;
        } else {
            if (row < rows && col < cols) {
                grid[row * cols + col] = c;
            }
            col++;
        }
    }

    // Estimate capacity: 24 vertices and 36 indices per cell
    uint32_t max_cells = rows * cols;
    MeshBuilder mb = mesh_builder_new(arena, max_cells * 24, max_cells * 36);

    // Generate geometry
    const float unit = 0.5f;  // 6 inches per grid cell
    uint32_t geometry_count = 0;

    for (uint32_t r = 0; r < rows; r++) {
        for (uint32_t c = 0; c < cols; c++) {
            char cell = grid[r * cols + c];
            if (has_geometry(cell)) {
                float x0 = c * unit;
                float x1 = (c + 1) * unit;
                float z0 = r * unit;
                float z1 = (r + 1) * unit;
                float y0 = get_base_height(cell);
                float y1 = y0 + get_height(cell);
                add_box(&mb, x0, z0, x1, z1, y0, y1);
                geometry_count++;
            }
        }
    }

    // println(str_lit("Generated {} geometry cells"), (int64_t)geometry_count);
    // println(str_lit("Total vertices: {}"), (int64_t)mb.vertex_count);
    // println(str_lit("Total indices: {}"), (int64_t)mb.index_count);

    // Generate glTF output
    generate_gltf(&mb, "hotel.bin", "hotel.gltf");

    arena_free(arena);
    return 0;
}

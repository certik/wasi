# Basic Physically Based Renderer

A minimal C++ ray tracer implementation based on the design from the PBRT book, built without external dependencies.

## Features

- **Ray-Triangle Intersection**: Möller-Trumbore algorithm
- **Materials**: Lambertian (diffuse) BSDF with texture support
- **Lighting**: Point lights and directional lights with inverse-square falloff
- **Camera**: Perspective camera with configurable FOV
- **Scene Loading**: Simple OBJ file parser (no external dependencies)
- **Image Output**: PPM format (easy to convert to PNG with external tools)

## Building

```bash
make
```

## Usage

### Render the default Cornell box test scene:
```bash
./renderer
```

### Render an OBJ file:
```bash
./renderer -i model.obj -o output.ppm -w 1920 -h 1080
```

### Options:
- `-i <file>` - Input OBJ file (optional, uses test scene if not provided)
- `-o <file>` - Output PPM file (default: output.ppm)
- `-w <width>` - Image width (default: 800)
- `-h <height>` - Image height (default: 600)
- `--help` - Show help

## Converting PPM to PNG

The renderer outputs PPM files. Convert to PNG using ImageMagick:
```bash
convert output.ppm output.png
```

Or on macOS with `sips`:
```bash
sips -s format png output.ppm --out output.png
```

## Architecture

Following the PBRT design principles:

- `math.h` - Vector math, rays, bounding boxes
- `geometry.h` - Primitives (triangles) and intersection tests
- `material.h` - BSDF interface and diffuse material
- `light.h` - Light sources (point, directional)
- `camera.h` - Ray generation from camera
- `scene.h` - Scene container and OBJ loader
- `texture.h` - Image loading and texture sampling
- `integrator.h` - Rendering algorithms (simple ray tracing, path tracing)
- `main.cpp` - Main rendering pipeline

## Extending the Renderer

The modular design makes it easy to extend:

1. **Add new materials**: Subclass `BSDF` and `Material`
2. **Add new lights**: Subclass `Light`
3. **Add acceleration structures**: Implement BVH in `geometry.h`
4. **Add path tracing**: Use `PathIntegrator` instead of `SimpleIntegrator`
5. **Add shadows**: Modify integrator to cast shadow rays

## Test Scene

The default test scene is a simple Cornell box with:
- White floor, ceiling, and back wall
- Red left wall
- Green right wall
- One point light at the top

## Performance Notes

The current implementation uses naive ray-triangle intersection (O(n) per ray). For complex scenes, consider implementing:
- BVH (Bounding Volume Hierarchy)
- Multi-threading (OpenMP or std::thread)
- Vectorization (SIMD)

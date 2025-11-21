# Design Document for Implementing a Physically Based Renderer Based on the PBRT Book

## Introduction

This design document outlines the architecture and implementation plan for a physically based renderer inspired by the algorithms and system structure described in the *Physically Based Rendering* (PBR) book (4th edition, available at https://pbr-book.org/). The renderer will take as input an OBJ file for geometry, a camera position (and related parameters), material properties stored as image textures (e.g., albedo, normal maps, roughness), and light positions with their properties (e.g., point lights with intensity and color).

The goal is to create a modular, extensible system that someone with no prior knowledge of rendering can implement step by step. We start with a basic version supporting:
- Geometry from OBJ files (triangular meshes).
- Basic texture colors (albedo/diffuse maps).
- Simple point lights with basic diffuse shading (Lambertian reflection, no shadows initially).

This basic version uses a simple ray tracing approach for rendering. The design emphasizes modularity using abstract interfaces (e.g., in C++), allowing easy extension to more advanced features like full path tracing, physically based materials (e.g., microfacet models), shadows, area lights, global illumination, and volume rendering, as described in later chapters of the PBR book.

Assumptions:
- Implementation language: C++ (for performance and alignment with PBRT's code style), but concepts are language-agnostic.
- No prior rendering knowledge: Each module includes explanations of concepts, pseudocode, and references to PBR book chapters.
- Input format: OBJ file for meshes. Camera, materials, and lights specified via a simple JSON config file (e.g., "scene.json") or command-line arguments. Material textures are image files (e.g., PNG/JPG) referenced in the config.
- Output: A PNG image file.
- Extensibility: Use interfaces/abstract classes for key components to allow subclassing for advanced features.

References to PBR book chapters are provided for deeper reading. Start implementing the basic version before adding extensions.

## Requirements

### Functional Requirements
- Load geometry from OBJ file (vertices, normals, UVs, faces).
- Load material textures (start with albedo color map).
- Set camera position, orientation, and field of view (FOV).
- Define basic lights (point lights with position, color, intensity).
- Render an image by tracing rays from the camera, intersecting geometry, and computing basic shading.
- Output the rendered image as PNG.

### Non-Functional Requirements
- Performance: Basic version uses naive ray intersection (loop over all triangles); extend to acceleration structures (e.g., BVH) later.
- Extensibility: Modules should allow adding advanced materials, lights, and integrators without rewriting core code.
- Threading: Basic version single-threaded; design for easy parallelization (e.g., per-pixel rendering).
- Dependencies: Use libraries like tinyobjloader (for OBJ), stb_image (for textures), and GLM (for math vectors/matrices). No heavy dependencies initially.

### Basic Version Scope
- Shading: Diffuse (Lambertian) with point lights, no shadows.
- No anti-aliasing, no global illumination.
- Resolution: Configurable (e.g., 800x600).

### Future Extensions
- Add shadows (ray casting to lights).
- Physically based materials (e.g., metallic/roughness from textures, using microfacet models from book Chapter 9).
- Path tracing integrator (Monte Carlo integration, Chapters 14-15).
- Area lights, emissive materials (Chapter 13).
- Acceleration structures (BVH, Chapter 4).
- Sampling strategies (Chapter 7).
- Volume rendering (Chapter 12).

## High-Level Architecture

The system follows PBRT's modular design (see book Chapter 1: Introduction, and System Overview section). Key components are abstract interfaces that can be implemented and swapped. The rendering process flows as follows:

1. **Load Input**: Parse OBJ, config (camera/lights/materials), and textures into a `Scene` object.
2. **Setup Components**: Create `Camera`, `Sampler` (basic), `Integrator` (basic ray tracer), and `Film` (image buffer).
3. **Render**: The `Integrator` uses the `Camera` to generate rays for each pixel (via `Sampler`), intersects rays with the `Scene` (geometry via `Primitive`s), computes shading using `Material`s and `Light`s, and stores results in `Film`.
4. **Output**: `Film` writes the image to file.

Main modules (as classes/interfaces):
- **Scene**: Holds all geometry (`Primitive`s), lights, and infinite environment (if added later).
- **Primitive**: Represents geometry (e.g., triangles); aggregates multiple for acceleration.
- **Camera**: Generates rays from pixels.
- **Sampler**: Provides sample points (basic: one per pixel; extend to multi-sample).
- **Integrator**: Core rendering algorithm (basic: simple ray tracing; extend to path tracing).
- **Material**: Defines surface properties (basic: diffuse texture; extend to PBR).
- **Texture**: Loads and evaluates image maps.
- **Light**: Emits light (basic: point; extend to area/directional).
- **Film**: Stores and processes the output image.

This mirrors PBRT's abstractions for extensibility: subclass interfaces to add features.

## Detailed Modules

### 1. Scene Loading
**Purpose**: Load all input data into memory. No rendering knowledge needed—treat as file parsing.

**Components**:
- Use tinyobjloader to parse OBJ: Extracts vertices, normals, UVs, faces (triangles), and material names.
- Parse JSON config: Example structure:
  ```json
  {
    "camera": {
      "position": [0, 0, 5],
      "look_at": [0, 0, 0],
      "fov": 60
    },
    "materials": {
      "material1": {
        "albedo_texture": "texture_albedo.png"
      }
    },
    "lights": [
      {
        "type": "point",
        "position": [10, 10, 10],
        "color": [1, 1, 1],
        "intensity": 100
      }
    ]
  }
  ```
- Load textures using stb_image.

**Pseudocode**:
```cpp
class SceneLoader {
  Scene Load(const std::string& obj_file, const std::string& config_file) {
    // Parse OBJ -> list of Triangle primitives
    // Parse JSON -> camera, lights, material map
    // Assign materials to primitives based on OBJ material names
    // Build Scene with primitives, lights
  }
};
```

**References**: Book Chapter 2 (Geometry), Chapter 3 (Shapes for triangles).

**Extension**: Add support for PBRT scene files or more formats.

### 2. Geometry (Primitives)
**Purpose**: Represent scene objects for ray intersection. Start with triangles.

**Concepts Explained**: A "primitive" is a shape like a triangle. A ray is a line from the camera; intersection finds if/where it hits.

**Interface**:
```cpp
class Primitive {
public:
  virtual Bounds3f WorldBound() const = 0;  // Bounding box for acceleration
  virtual bool Intersect(const Ray& ray, SurfaceInteraction* interaction) const = 0;
};

class Triangle : public Primitive {
  // Stores 3 vertices, normals, UVs
  // Implement Moller-Trumbore intersection algorithm (book Chapter 3)
};
```

**Aggregate Primitive**: For now, a simple list looping over all triangles. Extend to BVH (book Chapter 4).

**Extension**: Add spheres, meshes, instancing.

### 3. Camera
**Purpose**: Generate rays for each pixel.

**Concepts**: Camera defines viewpoint. Rays are shot through a virtual film plane.

**Interface**:
```cpp
class Camera {
public:
  virtual Ray GenerateRay(const Point2f& film_sample) const = 0;
};

class PerspectiveCamera : public Camera {
  // Uses position, look_at, up, FOV to compute projection matrix
  // Ray direction from pixel coord (book Chapter 6)
};
```

**Extension**: Add orthographic, environment cameras.

### 4. Materials and Textures
**Purpose**: Define how light interacts with surfaces. Start with basic diffuse.

**Concepts**: Material computes scattered light. Texture provides varying colors.

**Interfaces**:
```cpp
class Texture {
public:
  virtual Spectrum Evaluate(const Point2f& uv) const = 0;
};

class ImageTexture : public Texture {
  // Loads image, bilinear interpolation at UV
};

class Material {
public:
  virtual BSDF* GetBSDF(const SurfaceInteraction& interaction) const = 0;
};

class DiffuseMaterial : public Material {
  // Returns Lambertian BSDF with texture color (book Chapter 9)
};
```

**Explanation**: BSDF (Bidirectional Scattering Distribution Function) is a function returning how much light scatters from incoming to outgoing direction. Basic: diffuse = albedo * (1/PI) * dot(normal, light_dir).

**Extension**: Add microfacet, specular, subsurface (Chapters 9-10). Use PBR textures (roughness, metallic).

### 5. Lights
**Purpose**: Sources of illumination.

**Concepts**: Lights emit radiance. Basic point light: intensity falls off with distance^2.

**Interface**:
```cpp
class Light {
public:
  virtual Spectrum Sample_Li(const SurfaceInteraction& interaction, const Point2f& u, Vector3f* wi, float* pdf) const = 0;
  // For basic: wi = direction to light, pdf=1, Li = intensity / dist^2
};

class PointLight : public Light {
  // Position, color, intensity
};
```

**References**: Book Chapter 13.

**Extension**: Area lights, infinite (environment maps), with better sampling.

### 6. Integrator
**Purpose**: The "brain" computing pixel colors.

**Concepts**: Integrator solves the rendering equation (how light transports). Basic: trace ray, find intersection, sum direct light from all lights using material's BSDF.

**Interface**:
```cpp
class Integrator {
public:
  virtual void Render(const Scene& scene) = 0;
};

class SimpleIntegrator : public Integrator {
  void Render(const Scene& scene) {
    for each pixel (x, y) {
      Point2f sample = sampler->Get2D();  // Basic: center
      Ray ray = camera->GenerateRay(sample);
      SurfaceInteraction interaction;
      if (scene.Intersect(ray, &interaction)) {
        Spectrum color(0);
        for each light in scene.lights {
          Vector3f wi;
          float pdf;
          Spectrum Li = light->Sample_Li(interaction, sampler->Get2D(), &wi, &pdf);
          // Visibility test? Skip for basic (no shadows)
          color += interaction.bsdf->f(wo= -ray.dir, wi) * Li * dot(normal, wi);
        }
        film->AddSample(x, y, color);
      } else {
        film->AddSample(x, y, background_color);
      }
    }
  }
};
```

**References**: Book Chapters 15-16 (Light Transport). Start with Whitted-style (simple), extend to path tracing.

**Extension**: Add recursion for reflections, Monte Carlo for indirect light.

### 7. Sampler
**Purpose**: Generate samples for pixels/integrals.

**Basic Implementation**: One sample per pixel at center.

**Interface**:
```cpp
class Sampler {
public:
  virtual Point2f Get2D() = 0;
  virtual void StartPixel() = 0;
};

class PixelSampler : public Sampler { /* Simple implementation */ };
```

**References**: Book Chapter 7.

**Extension**: Stratified, low-discrepancy for anti-aliasing.

### 8. Film
**Purpose**: Store and output the image.

**Concepts**: Accumulates samples, applies filter (basic: none).

**Interface**:
```cpp
class Film {
public:
  virtual void AddSample(int x, int y, const Spectrum& color) = 0;
  virtual void WriteImage(const std::string& filename) = 0;
};

class ImageFilm : public Film {
  // 2D array of colors, divide by samples if multi-sampling
  // Use stb_image_write for PNG
};
```

**References**: Book Chapter 8.

**Extension**: HDR output, tone mapping.

## Rendering Process

1. **Initialization**: Load scene, create camera, sampler, film, integrator.
2. **Render Loop**: Call `integrator->Render(scene)`.
   - Parallelize: Use OpenMP or std::thread for pixels (future).
3. **Post-Process**: Film writes image.

Pseudocode for main:
```cpp
int main() {
  Scene scene = SceneLoader::Load("model.obj", "scene.json");
  Camera* camera = new PerspectiveCamera(scene.camera_params);
  Sampler* sampler = new PixelSampler();
  Film* film = new ImageFilm(resolution);
  Integrator* integrator = new SimpleIntegrator(camera, sampler, film);
  integrator->Render(scene);
  film->WriteImage("output.png");
}
```

## Extensibility

- **Interfaces**: All key classes are abstract. To add a new material, subclass `Material` and implement `GetBSDF()`. Register in scene loading.
- **Plugins**: For advanced, compile new subclasses and select via config (e.g., "integrator_type": "path_tracer").
- **Phased Implementation**:
  1. Implement basic modules without shading (just intersection colors).
  2. Add diffuse materials and lights.
  3. Add shadows (visibility rays in integrator).
  4. Replace SimpleIntegrator with PathIntegrator (recursive ray bouncing).
  5. Add acceleration (BVHAggregate subclass of Primitive).
- **Testing**: Render simple scenes (e.g., Cornell box) and compare to book examples.

## Implementation Notes

- **Math**: Use a Spectrum class for colors (RGB initially; extend to spectral). Vectors with GLM.
- **Units**: Follow book (radiometry, Chapter 5).
- **Debugging**: Output intersection counts, visualize rays.
- **Performance**: Profile intersections; add BVH early if slow.
- **Resources**: Download PBRT source from GitHub for inspiration, but implement from scratch.
- **Timeline**: Basic version: 1-2 weeks. Extensions: iterative.

This design provides a clear path from basics to full PBR, aligning with the book's progressive teaching style. Refer to the book for math derivations—implement formulas directly.

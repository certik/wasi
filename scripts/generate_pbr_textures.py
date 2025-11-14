from PIL import Image, ImageDraw
import math

SIZE = 512  # Texture resolution

# Helper to create a grayscale gradient or pattern
def create_grayscale_pattern(image, width, height, is_checker=False):
    pixels = image.load()
    for x in range(width):
        for y in range(height):
            if is_checker:
                val = 255 if (x // 64 + y // 64) % 2 == 0 else 128
            else:
                val = int(255 * (math.sin(x / 20.0) * math.sin(y / 20.0) + 1) / 2)
            if image.mode in ('RGB', 'RGBA'):
                pixels[x, y] = (val, val, val) if image.mode == 'RGB' else (val, val, val, 255)
            else:
                pixels[x, y] = val

# Albedo: Reddish brick color with some variation
albedo = Image.new('RGB', (SIZE, SIZE), (0, 0, 0))
draw = ImageDraw.Draw(albedo)
for x in range(SIZE):
    for y in range(SIZE):
        r = 200 + int(55 * math.sin(x / 30.0))
        g = 100 + int(55 * math.sin(y / 30.0))
        b = 50
        draw.point((x, y), (r, g, b))
albedo.save('debug_albedo.png')

# Normal map: Simple bump pattern (tangent space: RGB = (0.5,0.5,1) is flat)
normal = Image.new('RGB', (SIZE, SIZE), (128, 128, 255))  # Flat base
draw = ImageDraw.Draw(normal)
create_grayscale_pattern(normal, SIZE, SIZE)
for x in range(SIZE):
    for y in range(SIZE):
        # Perturb X/Y based on height-like sine
        nx = int(128 + 127 * math.sin(x / 50.0))
        ny = int(128 + 127 * math.cos(y / 50.0))
        nz = 255  # Z always positive
        draw.point((x, y), (nx, ny, nz))
normal.save('debug_normal.png')

# Metallic-Roughness: Red=metallic (checker: 0 or 1), Green=roughness (gradient 0-1), Blue=1 (unused or AO=1)
metallic_roughness = Image.new('RGB', (SIZE, SIZE), (0, 0, 255))
draw = ImageDraw.Draw(metallic_roughness)
for x in range(SIZE):
    for y in range(SIZE):
        metallic = 255 if (x // 128 + y // 128) % 2 == 0 else 0
        roughness = int(255 * (y / SIZE))  # Gradient from smooth to rough
        ao = 255  # Full AO
        draw.point((x, y), (metallic, roughness, ao))
metallic_roughness.save('debug_metallic_roughness.png')

# Emissive: Glowing spots (black=off, white=on)
emissive = Image.new('RGB', (SIZE, SIZE), (0, 0, 0))
draw = ImageDraw.Draw(emissive)
create_grayscale_pattern(emissive, SIZE, SIZE, is_checker=True)
emissive.save('debug_emissive.png')

# Height: Grayscale depth for POM (white=raised, black=indented)
height = Image.new('L', (SIZE, SIZE), 0)
draw = ImageDraw.Draw(height)
create_grayscale_pattern(height, SIZE, SIZE)
height.save('debug_height.png')

print("Debug PBR textures generated: debug_albedo.png, debug_normal.png, etc.")

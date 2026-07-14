from PIL import Image

img = Image.open("kelp.png").convert("RGBA")

pixels = img.load()

black_pixels = []

width, height = img.size

for y in range(height):
    for x in range(width):
        r, g, b, a = pixels[x, y]

        if a > 0 and r == 0 and g == 0 and b == 0:
            black_pixels.append([x + 2, y + 2])

print(black_pixels)

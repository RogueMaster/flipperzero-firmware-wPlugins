# images to image sheet

import sys
import os
from PIL import Image, ImageOps
from math import sqrt

spacing = 0

if (len(sys.argv) < 2) or (len(sys.argv) > 4):
    sys.stderr.write(
        "Usage: {} imagefolder [outputfileBase] [flip=False]\nWhen no outputfile is given, the imagesheet will be created in the parent folder.".format(
            os.path.basename(sys.argv[0])
        )
    )
    sys.exit(-1)

imagepath = sys.argv[1]
flip = False

if len(sys.argv) == 4:
    if sys.argv[3].upper() == "TRUE":
        flip = True
if os.path.isfile(imagepath):
    imagepath = os.path.dirname(os.path.abspath(imagepath))
imagepath += os.sep

images = []
for f in sorted(os.listdir(imagepath)):
    if os.path.isfile(imagepath + f):
        if f != ".DS_Store":
            images.append(f)

imagecount = len(images)
cols = imagecount
for i in range(int(sqrt(imagecount)), 0, -1):
    if imagecount % i == 0:
        cols = imagecount // i
        break
rows = imagecount // cols
img = Image.open(imagepath + images[0]).convert("RGBA")
width = img.size[0]
height = img.size[1]
new = Image.new(
    "RGBA",
    (cols * (width + spacing) + spacing, rows * (height + spacing) + spacing),
    (0, 0, 0, 0),
)
c = 0
r = 0
for i in range(len(images)):
    img = Image.open(imagepath + images[i]).convert("RGBA")
    if flip:
        img = ImageOps.mirror(img)
    new.paste(img, (c * (width + spacing) + spacing, r * (height + spacing) + spacing))
    c += 1
    if c == cols:
        c = 0
        r += 1
if len(sys.argv) >= 3:
    filename = sys.argv[2]
    filename = "{}_{}x{}_{}.png".format(filename, width, height, spacing)
elif spacing > 0:
    filename = "{}{}{}_{}x{}_{}.png".format(
        os.path.dirname(imagepath[:-1]),
        os.sep,
        os.path.basename(imagepath[:-1]),
        width,
        height,
        spacing,
    )
else:
    filename = "{}{}{}_{}x{}.png".format(
        os.path.dirname(imagepath[:-1]),
        os.sep,
        os.path.basename(imagepath[:-1]),
        width,
        height,
    )
new.save(filename)

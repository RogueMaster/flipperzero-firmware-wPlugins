# images to sheet

import sys
import os
from PIL import Image
from PIL import ImageOps

if len(sys.argv) != 2:
    sys.stderr.write("No images to process.\n")
    sys.exit(-1)

imagepath = sys.argv[1]
if os.path.isfile(imagepath):
    imagepath = os.path.dirname(os.path.abspath(imagepath))
imagepath += os.sep

images = []
for f in os.listdir(imagepath):
    if os.path.isfile(imagepath + f):
        images.append(f)

for i in range(len(images)):
    img = Image.open(imagepath + images[i]).convert("RGBA")
    new = ImageOps.mirror(img)
    new.save(imagepath + images[i])

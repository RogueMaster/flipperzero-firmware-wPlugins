#!/usr/bin/env python3
# 生成 10x10 单色 PNG 图标: 3D 迷宫透视
from PIL import Image, ImageDraw

img = Image.new("1", (10, 10), 0)
d = ImageDraw.Draw(img)

# 外框(屏幕)
d.rectangle([0, 0, 9, 9], outline=1)
# 透视消失点(中心)
d.point([4, 4], 1)
d.point([5, 4], 1)
d.point([4, 5], 1)
d.point([5, 5], 1)
# 透视线四角到中心
d.line([0, 0, 4, 4], fill=1)
d.line([9, 0, 5, 4], fill=1)
d.line([0, 9, 4, 5], fill=1)
d.line([9, 9, 5, 5], fill=1)

img.save("/workspace/maze3d.png")
print("saved maze3d.png")

#!/usr/bin/env python3
# 生成 10x10 单色 PNG 应用图标(一个笑脸气泡)
from PIL import Image, ImageDraw

img = Image.new("1", (10, 10), 0)  # 0 = 透明/白
d = ImageDraw.Draw(img)

# 圆形外框
d.ellipse([0, 0, 9, 9], outline=1, width=1)
# 两只眼睛
d.point([3, 3], 1)
d.point([6, 3], 1)
# 微笑(下弧)
d.arc([2, 3, 7, 8], start=20, end=160, fill=1)

img.save("/workspace/hello.png")
print("saved hello.png")

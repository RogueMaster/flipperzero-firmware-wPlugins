# mitzi-karl-eido
<img alt="Karl Eido Main Screen"  src="screenshots/MainScreen.png" width="40%" />

A simple Flipper Zero kaleidoscope app.

## Detailed description
**Karl Eido** displays triangular grid of equilateral triangles. The up/down arrows control the size of the triangles in steps of 2px, starting at side-length a=5px up to a=63px. The base side of the leftmost triangle(s) is always the left side of the screen. This means that there are vertical lines at `(int) (floor(sqrt(3)/2 a))`. One base triangle is centered on the screen height, meaning that the top is pointing towards the right at 31px from the top of the screen. Each triangle should also display a center point, the display can be toggled by pressing shortly OK. Back-Button should exit the app. Left/right button decreases/increases the number of random pixels in the base triangle, the dots are mirror along the axis accordingly.

The top right of the screen shows debug info:
```
# [area = number of white pixels per triangle (ignore the center point here)] T: [number of visible center points]
```

## Version history
See [changelog.md](changelog.md)


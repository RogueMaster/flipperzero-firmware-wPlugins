# mitzi-scroller
A simple Flipper Zero to view annotated, tiled images.  

<img alt="Main Screen"  src="screenshots/MainScreen.png" width="40%" />

The user can scroll around a large images (consisting of many small 128x64px-tiles saved in the `assets/`-folder). There is a CSV file which specifies the arrangement of the tiles. Another CSV-file specifies tile-names, coordinates on the tiles, and text-annotations. The user sees a 8px circle in the middle of the screen as cursor. Whenever the cursor is on a coordinate with an annotation, the text is shown on top left of the screen.

## Usage
- **Arrow Keys**: Move cursor around
- **OK Button**: Appears when there is an annotation for the current image position
- **Back Button** (hold or press): Exit

## Example data
We start with a black-and-white PNG squaredd `640px' image showing most important stars (magnitude 1-6) of the Northern hemnisphere as black symbols. Magnitude 6 is a single dot. 

## Version history
See [changelog.md](changelog.md)

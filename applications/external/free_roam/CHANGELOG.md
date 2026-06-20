## v0.6
- Added Online mode
- Added option in the game menu to hide/show the player

## v0.5
- Added a mini map with player position and direction indicators
- Optimized Vector usage in the rendering code to improve performance and reduce memory usage
- Updated input handling to only parse short and long presses
- Updated wall rendering to use 3D sprites
- Updated the engine with various optimizations, bug fixes, and improvements to the rendering pipeline
- Updated maps with new elements and layouts

## v0.4
- Updated the settings with a connect to WiFi button and conditional compilation for Momentum support
- Updated the engine to use floats for triangle vertices

## v0.3.2
- Updated to use the flipper_http folder as intended

## v0.3.1
- Updated the save/load functions to use a specific folder ("flipper_http") for storing Wi-Fi and user credentials. This simplifies management and avoids potential conflicts with other applications.

## v0.2 
- Added teleport doors and an information screen after the user teleports
- Updated the player's sprite to face the direction of movement
- Fixed minor bugs with movement and interactions
- Switched to heap allocations which resolved the reported stack overflow issues

## v0.1
- Initial release

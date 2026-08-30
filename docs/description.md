# ZeroMesh

A Meshtastic client for the Flipper Zero. Connect a mesh node and read the network from the Flipper screen: incoming messages, a live node roster, signal stats, device telemetry, and an offline vector map with your nodes drawn on it.

## Connecting a node

Two transports are supported, selected in Settings.

**Serial** works with stock Meshtastic firmware. Wire the node to the Flipper GPIO header: node TX to Flipper RX, node RX to Flipper TX, and a common ground. This is the default.

**Bluetooth LE** is also available, but the Flipper radio can only act as a peripheral and never dials out. The node therefore has to connect to the Flipper, which requires a Meshtastic build carrying the ZeroMesh link module. With stock node firmware, use serial.

Do not power the node from Flipper 5V while it is also on USB.

## What it does

- Read and send messages, with delivery reported from routing acknowledgements
- Live node roster with names, signal quality, battery and telemetry
- Private chat with any node on the mesh, and unread indicators
- Offline vector map centred on a mesh node, with place and water labels, a scale bar, and range and bearing to the selected node
- Pan across tile boundaries, cycle zoom levels, and jump to nearby towns from an on-map toolbar
- Configure the connected radio: LoRa region, modem preset, device role, GPS, a fixed position for a node with no GPS fix, and the primary channel key
- Haptic and LED notification on incoming traffic, with selectable ringtones

## Map data

The map is optional. Without an archive the Map page still opens and says so, and everything else works normally.

Map archives are built from OpenStreetMap vector tiles and stored on the SD card. The repository includes tooling that takes a place name and a radius, downloads the tiles, packs them, and copies the archive to the Flipper. See the project page for the full setup.

Map data comes from OpenStreetMap contributors and is licensed under the Open Database License.

# Third-Party Notices

This project includes or depends on third-party open-source software. The following notices are provided for attribution and license compliance.

## Meshtastic firmware / protocol assets
Project: Meshtastic
Source: https://github.com/meshtastic/firmware
License: GNU General Public License v3.0 (GPL-3.0)
Copyright: See upstream repository.

## Flipper Zero firmware / SDK
Project: flipperzero-firmware
Source: https://github.com/flipperdevices/flipperzero-firmware
License: GNU General Public License v3.0 (GPL-3.0)
Copyright: See upstream repository.

## Nanopb
Project: nanopb
Source: https://github.com/nanopb/nanopb
License: zlib license
Copyright: See upstream repository.
Notice: The zlib license text must be retained with distributions.

## libcarto
Project: CartoTUI (libcarto)
Source: https://github.com/SAMS0N1TE/CartoTUI
License: GNU General Public License v3.0 (GPL-3.0)
Copyright: See upstream repository.
Notice: The vector tile renderer under lib/carto is vendored from CartoTUI and
modified for the Flipper Zero (single-precision maths, reduced fixed buffers).

## Map data
Project: OpenStreetMap
Source: https://www.openstreetmap.org
License: Open Database License (ODbL) 1.0
Copyright: (c) OpenStreetMap contributors.
Notice: ZeroMesh ships no map data. tools/fetch_tiles.py downloads vector tiles
from VersaTiles (https://versatiles.org), which are produced from OpenStreetMap
data. Anything published from those tiles must credit OpenStreetMap contributors
and honour the ODbL share-alike terms.

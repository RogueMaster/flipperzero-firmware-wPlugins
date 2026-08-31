#!/usr/bin/env python3
"""One command to go from a place name to a map on the Flipper.

  zeromesh_setup.py --place "Concord, New Hampshire" --radius 25 --install
  zeromesh_setup.py --bbox -72.56,42.69,-70.70,45.31 --max-zoom 12
  zeromesh_setup.py --reset-node COM9

Fetches tiles for the area, packs them, and copies the archive to the Flipper.
Node firmware is not flashed here: the Meshtastic web flasher at
flasher.meshtastic.org handles board variants better than anything worth
duplicating. --reset-node covers the one case it cannot, a board that will not
drop into its bootloader on its own.
"""

import argparse
import json
import math
import os
import subprocess
import sys
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
NOMINATIM = 'https://nominatim.openstreetmap.org/search'
USER_AGENT = 'ZeroMesh-setup/1.0 (Flipper Zero offline maps)'
FLIPPER_VID_PID = '0483:5740'
CHUNK = 4096
SERIAL_WARN_BYTES = 2 * 1024 * 1024


def geocode(place):
    url = NOMINATIM + '?' + urllib.parse.urlencode(
        {'q': place, 'format': 'json', 'limit': '1'})
    req = urllib.request.Request(url, headers={'User-Agent': USER_AGENT})
    with urllib.request.urlopen(req, timeout=30) as r:
        data = json.loads(r.read().decode('utf-8'))
    if not data:
        sys.exit('no match for %r' % place)
    hit = data[0]
    south, north, west, east = (float(v) for v in hit['boundingbox'])
    return hit['display_name'], float(hit['lat']), float(hit['lon']), (west, south, east, north)


def bbox_from_radius(lat, lon, km):
    dlat = km / 111.32
    dlon = km / (111.32 * max(0.01, math.cos(math.radians(lat))))
    return (lon - dlon, lat - dlat, lon + dlon, lat + dlat)


def run(args):
    print('  $ ' + ' '.join(os.path.basename(a) if a.endswith('.py') else a for a in args[1:]))
    r = subprocess.run(args)
    if r.returncode != 0:
        sys.exit('step failed')


def find_flipper(explicit):
    if explicit:
        return explicit
    try:
        import serial.tools.list_ports
    except ImportError:
        sys.exit('pyserial is needed to talk to the Flipper: pip install pyserial')
    for p in serial.tools.list_ports.comports():
        if FLIPPER_VID_PID in (p.hwid or '').upper().replace('VID:PID=', ''):
            return p.device
        if 'FLIP' in (p.serial_number or '').upper():
            return p.device
    return None


def cli(port):
    import serial
    s = serial.Serial(port, timeout=5)
    time.sleep(0.3)
    s.reset_input_buffer()
    s.write(b'\r\n')
    s.read_until(b'>: ')
    return s


def upload(port, local, remote, mkdir='/ext/apps_data/zeromesh'):
    import serial  # noqa: F401  (import checked in find_flipper)
    size = os.path.getsize(local)
    if size > SERIAL_WARN_BYTES:
        print('  note: %.1f MB over USB takes a while. A card reader is much faster.'
              % (size / 1048576.0))

    s = cli(port)
    try:
        s.write(('storage mkdir %s\r\n' % mkdir).encode())
        s.read_until(b'>: ')
        s.write(('storage remove "%s"\r\n' % remote).encode())
        s.read_until(b'>: ')

        sent = 0
        started = time.time()
        with open(local, 'rb') as fh:
            while True:
                block = fh.read(CHUNK)
                if not block:
                    break
                s.write(('storage write_chunk "%s" %d\r' % (remote, len(block))).encode())
                s.read_until(b'\n')
                s.write(block)
                s.read_until(b'>: ')
                sent += len(block)
                rate = sent / max(0.001, time.time() - started) / 1024.0
                sys.stdout.write('\r  %3d%%  %.0f KB/s' % (sent * 100 // size, rate))
                sys.stdout.flush()
        print()
    finally:
        s.close()


def reset_node(port):
    """A 1200 baud open is the standard request to enter the bootloader. Some
    boards will not reset on their own and re-enumerate on a different port."""
    import serial
    print('touching %s at 1200 baud' % port)
    try:
        s = serial.Serial(port, 1200)
        time.sleep(0.3)
        s.close()
    except Exception as ex:
        sys.exit('could not open %s: %s' % (port, ex))
    print('done. The board should now be in its bootloader, possibly on a new')
    print('port. Flash it from https://flasher.meshtastic.org')


def check_rtttl(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as fh:
        text = fh.read().strip()
    parts = text.split(':')
    if len(parts) < 3:
        sys.exit('%s does not look like RTTTL (needs name:settings:notes)' % path)
    if not parts[2].strip():
        sys.exit('%s has no notes' % path)
    return text


def send_ringtone(path, port):
    if not os.path.isfile(path):
        sys.exit('no such file: %s' % path)
    text = check_rtttl(path)
    name = os.path.basename(path)
    if not name.lower().endswith('.rtttl'):
        name += '.rtttl'

    dev = find_flipper(port)
    if not dev:
        sys.exit('no Flipper found. Plug it in, or pass --port')

    print('%s -> %s' % (text.split(':')[0], dev))
    upload(dev, path, '/ext/apps_data/zeromesh/ringtones/' + name,
           mkdir='/ext/apps_data/zeromesh/ringtones')
    print('done. Pick it in ZeroMesh under Settings.')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--place', help='place name, resolved to an area')
    ap.add_argument('--center', help='lat,lon instead of --place')
    ap.add_argument('--radius', type=float, help='km around the place or centre')
    ap.add_argument('--bbox', help='W,S,E,N to skip lookup entirely')
    ap.add_argument('--min-zoom', type=int, default=10)
    ap.add_argument('--max-zoom', type=int, default=12)
    ap.add_argument('--out', default='map.pmtiles')
    ap.add_argument('--tiles', default='map', help='where tiles are cached')
    ap.add_argument('--install', action='store_true', help='copy to the Flipper')
    ap.add_argument('--port', help='Flipper serial port, autodetected otherwise')
    ap.add_argument('--reset-node', metavar='PORT', help='1200 baud bootloader touch')
    ap.add_argument('--ringtone', metavar='FILE', help='send an .rtttl to the Flipper')
    args = ap.parse_args()

    if args.reset_node:
        reset_node(args.reset_node)
        return

    if args.ringtone:
        send_ringtone(args.ringtone, args.port)
        return

    if args.bbox:
        w, s, e, n = (float(v) for v in args.bbox.split(','))
        label = 'the given box'
    elif args.place:
        label, lat, lon, box = geocode(args.place)
        if args.radius:
            w, s, e, n = bbox_from_radius(lat, lon, args.radius)
        else:
            w, s, e, n = box
    elif args.center and args.radius:
        lat, lon = (float(v) for v in args.center.split(','))
        w, s, e, n = bbox_from_radius(lat, lon, args.radius)
        label = args.center
    else:
        sys.exit('need --place, --center with --radius, or --bbox')

    print('area   : %s' % label)
    print('bbox   : %.4f,%.4f,%.4f,%.4f' % (w, s, e, n))
    print('zooms  : %d-%d' % (args.min_zoom, args.max_zoom))
    print()

    run([sys.executable, os.path.join(HERE, 'fetch_tiles.py'), args.tiles,
         '--bbox', '%f,%f,%f,%f' % (w, s, e, n),
         '--min-zoom', str(args.min_zoom), '--max-zoom', str(args.max_zoom)])

    print()
    run([sys.executable, os.path.join(HERE, 'build_pmtiles.py'), args.tiles, args.out,
         '--min-zoom', str(args.min_zoom), '--max-zoom', str(args.max_zoom),
         '--simplify', '--max-tile-bytes', '20736', '--leaf-size', '256'])

    if not args.install:
        print()
        print('Copy %s to /ext/apps_data/zeromesh/map.pmtiles on the SD card,' % args.out)
        print('or re-run with --install to send it over USB.')
        return

    port = find_flipper(args.port)
    if not port:
        sys.exit('no Flipper found. Plug it in, or pass --port')

    print()
    print('installing to %s' % port)
    upload(port, args.out, '/ext/apps_data/zeromesh/map.pmtiles')
    print('done. Open ZeroMesh and go to the Map page.')


if __name__ == '__main__':
    main()

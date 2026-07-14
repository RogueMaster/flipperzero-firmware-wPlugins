VERSION = "1.20"
title = "Arduboy FX data uploader v" + VERSION + " by Mr.Blinky Feb.2022-Mar.2023"

import sys
import os
import time

try:
    toolspath = os.path.dirname(os.path.abspath(sys.argv[0]))
    sys.path.insert(0, toolspath)
    from serial.tools.list_ports import comports
    from serial import Serial
except:
    sys.stderr.write("pySerial python module not found or wrong version.\n")
    sys.stderr.write(
        "Make sure the correct module is installed or placed at {}\n".format(toolspath)
    )
    sys.exit(-1)

compatibledevices = [
    # Arduboy Leonardo
    "VID:PID=2341:0036",
    "VID:PID=2341:8036",
    "VID:PID=2A03:0036",
    "VID:PID=2A03:8036",
    # Arduboy Micro
    "VID:PID=2341:0037",
    "VID:PID=2341:8037",
    "VID:PID=2A03:0037",
    "VID:PID=2A03:8037",
    # Genuino Micro
    "VID:PID=2341:0237",
    "VID:PID=2341:8237",
    # Sparkfun Pro Micro 5V
    "VID:PID=1B4F:9205",
    "VID:PID=1B4F:9206",
    # Adafruit ItsyBitsy 5V
    "VID:PID=239A:000E",
    "VID:PID=239A:800E",
]

manufacturers = {
    0x01: "Spansion",
    0x14: "Cypress",
    0x1C: "EON",
    0x1F: "Adesto(Atmel)",
    0x20: "Micron",
    0x37: "AMIC",
    0x9D: "ISSI",
    0xC2: "General Plus",
    0xC8: "Giga Device",
    0xBF: "Microchip",
    0xEF: "Winbond",
}

PAGESIZE = 256
BLOCKSIZE = 65536
PAGES_PER_BLOCK = BLOCKSIZE // PAGESIZE
MAX_PAGES = 65536
bootloader_active = False


def print(s):
    sys.stdout.write(s + "\n")
    sys.stdout.flush()


def getComPort(verbose):
    global bootloader_active
    devicelist = list(comports())
    for device in devicelist:
        for vidpid in compatibledevices:
            if vidpid in device[2]:
                port = device[0]
                bootloader_active = (compatibledevices.index(vidpid) & 1) == 0
                if verbose:
                    sys.stdout.write("Found {} at port {} ".format(device[1], port))
                return port
    if verbose:
        print("Arduboy not found.")


def bootloaderStart():
    global bootloader
    ## find and connect to Arduboy in bootloader mode ##
    port = getComPort(True)
    if port is None:
        sys.exit(-1)
    if not bootloader_active:
        print("Selecting bootloader mode...")
        try:
            bootloader = Serial(port, 1200)
            time.sleep(0.1)
            bootloader.close()
            time.sleep(0.5)
        except:
            sys.stderr.write("COM port not available.\n")
            sys.exit(-1)
        # wait for disconnect and reconnect in bootloader mode
        while getComPort(False) == port:
            time.sleep(0.1)
            if bootloader_active:
                break
        while getComPort(False) is None:
            time.sleep(0.1)
        port = getComPort(True)

    sys.stdout.write("Opening port ...")
    sys.stdout.flush()
    for retries in range(20):
        try:
            time.sleep(0.1)
            bootloader = Serial(port, 57600)
            break
        except:
            if retries == 19:
                print(" Failed!")
                sys.exit(-1)
            sys.stdout.write(".")
            sys.stdout.flush()
            time.sleep(0.4)
    print("\r")


def getVersion():
    bootloader.write(b"V")
    return int(bootloader.read(2))


def getJedecID():
    bootloader.write(b"j")
    jedec_id = bootloader.read(3)
    time.sleep(0.5)
    bootloader.write(b"j")
    jedec_id2 = bootloader.read(3)
    if (
        jedec_id2 != jedec_id
        or jedec_id == b"\x00\x00\x00"
        or jedec_id == b"\xFF\xFF\xFF"
    ):
        sys.stderr.write("No FX flash chip detected.\n")
        sys.exit(-1)
    return bytearray(jedec_id)


def bootloaderExit():
    global bootloader
    bootloader.write(b"E")
    bootloader.read(1)


################################################################################


def writeFlash(pagenumber, flashdata):
    bootloaderStart()

    # check version
    if getVersion() < 13:
        sys.stderr.write("Bootloader has no flash cart support\nWrite aborted!\n")
        sys.exit(-1)

    ## detect flash cart ##
    jedec_id = getJedecID()
    if jedec_id[0] in manufacturers.keys():
        manufacturer = manufacturers[jedec_id[0]]
    else:
        manufacturer = "unknown"
    capacity = 1 << jedec_id[2]
    print(
        "Detected FX flash chip with ID {:02X}{:02X}{:02X} size {}KB".format(
            jedec_id[0], jedec_id[1], jedec_id[2], capacity // 1024
        )
    )

    oldtime = time.time()
    # when starting partially in a block, preserve the beginning of old block data
    if pagenumber % PAGES_PER_BLOCK:
        blocklen = pagenumber % PAGES_PER_BLOCK * PAGESIZE
        blockaddr = pagenumber // PAGES_PER_BLOCK * PAGES_PER_BLOCK
        # read partial block data start
        bootloader.write(bytearray([ord("A"), blockaddr >> 8, blockaddr & 0xFF]))
        bootloader.read(1)
        bootloader.write(
            bytearray([ord("g"), (blocklen >> 8) & 0xFF, blocklen & 0xFF, ord("C")])
        )
        flashdata = bootloader.read(blocklen) + flashdata
        pagenumber = blockaddr

    # when ending partially in a block, preserve the ending of old block data
    if len(flashdata) % BLOCKSIZE:
        blocklen = BLOCKSIZE - len(flashdata) % BLOCKSIZE
        blockaddr = pagenumber + len(flashdata) // PAGESIZE
        # read partial block data end
        bootloader.write(bytearray([ord("A"), blockaddr >> 8, blockaddr & 0xFF]))
        bootloader.read(1)
        bootloader.write(
            bytearray([ord("g"), (blocklen >> 8) & 0xFF, blocklen & 0xFF, ord("C")])
        )
        flashdata += bootloader.read(blocklen)

    ## write to flash cart ##
    blocks = len(flashdata) // BLOCKSIZE
    for block in range(blocks):
        if (block & 1 == 0) or verifyAfterWrite:
            bootloader.write(b"x\xC2")  # RGB LED RED, buttons disabled
        else:
            bootloader.write(b"x\xC0")  # RGB LED OFF, buttons disabled
        bootloader.read(1)
        sys.stdout.write("\rWriting block {}/{}  ".format(block + 1, blocks))
        sys.stdout.flush()
        blockaddr = pagenumber + block * BLOCKSIZE // PAGESIZE
        blocklen = BLOCKSIZE
        # write block
        bootloader.write(bytearray([ord("A"), blockaddr >> 8, blockaddr & 0xFF]))
        bootloader.read(1)
        bootloader.write(
            bytearray([ord("B"), (blocklen >> 8) & 0xFF, blocklen & 0xFF, ord("C")])
        )
        bootloader.write(flashdata[block * BLOCKSIZE : block * BLOCKSIZE + blocklen])
        bootloader.read(1)
        if verifyAfterWrite:
            sys.stdout.write("\rVerifying block {}/{}".format(block + 1, blocks))
            sys.stdout.flush()
            bootloader.write(b"x\xC1")  # RGB BLUE RED, buttons disabled
            bootloader.read(1)
            bootloader.write(bytearray([ord("A"), blockaddr >> 8, blockaddr & 0xFF]))
            bootloader.read(1)
            bootloader.write(
                bytearray([ord("g"), (blocklen >> 8) & 0xFF, blocklen & 0xFF, ord("C")])
            )
            if (
                bootloader.read(blocklen)
                != flashdata[block * BLOCKSIZE : block * BLOCKSIZE + blocklen]
            ):
                sys.stderr.write(" verify failed!\n\nWrite aborted.")
                bootloader.write(b"x\x40")  # RGB LED off, buttons enabled
                bootloader.read(1)
                sys.exit(-1)

    # write complete
    bootloader.write(b"x\x44")  # RGB LED GREEN, buttons enabled
    bootloader.read(1)
    time.sleep(0.5)
    bootloader.write(b"x\x40")  # RGB LED off, buttons enabled
    bootloader.read(1)
    bootloader.close()
    print("\rFX Data uploaded successfully")


################################################################################

print(title)

if (len(sys.argv) != 2) or (os.path.isfile(sys.argv[1]) != True):
    sys.stderr.write("FX data file not found.\n")

filename = os.path.abspath(sys.argv[1])

verifyAfterWrite = True

print('Uploading FX data from file "{}"'.format(filename))
f = open(filename, "rb")
programdata = bytearray(f.read())
f.close()
if len(programdata) % PAGESIZE:
    programdata += b"\xFF" * (PAGESIZE - (len(programdata) % PAGESIZE))
programpage = MAX_PAGES - (len(programdata) // PAGESIZE)

writeFlash(programpage, programdata)

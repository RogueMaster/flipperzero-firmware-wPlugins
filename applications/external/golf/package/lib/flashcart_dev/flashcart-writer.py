#!/usr/bin/env python3
print("\nArduboy flash cart writer v1.17 by Mr.Blinky May 2018 - Apr.2020\n")

# requires pyserial to be installed. Use "python -m pip install pyserial" on commandline

import sys
import time
import os
from getopt import getopt

try:
    from serial.tools.list_ports import comports
    from serial import Serial
except:
    print("The pySerial module is required but not installed!")
    print("Use 'python -m pip install pyserial' from the commandline to install.")
    sys.exit()

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

lcdBootProgram = b"\xD5\xF0\x8D\x14\xA1\xC8\x81\xCF\xD9\xF1\xAF\x20\x00"


def delayedExit():
    time.sleep(2)
    sys.exit()


def getComPort(verbose):
    global bootloader_active
    devicelist = list(comports())
    for device in devicelist:
        for vidpid in compatibledevices:
            if vidpid in device[2]:
                port = device[0]
                bootloader_active = (compatibledevices.index(vidpid) & 1) == 0
                if verbose:
                    print("Found {} at port {}".format(device[1], port))
                return port
    if verbose:
        print("Arduboy not found.")


def bootloaderStart():
    global bootloader
    ## find and connect to Arduboy in bootloader mode ##
    port = getComPort(True)
    if port is None:
        delayedExit()
    if not bootloader_active:
        print("Selecting bootloader mode...")
        bootloader = Serial(port, 1200)
        bootloader.close()
        time.sleep(0.5)
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
                delayedExit()
            sys.stdout.write(".")
            sys.stdout.flush()
            time.sleep(0.4)
    print()


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
        print("No flash cart detected.")
        delayedExit()
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
        print("Bootloader has no flash cart support\nWrite aborted!")
        delayedExit()

    ## detect flash cart ##
    jedec_id = getJedecID()
    if jedec_id[0] in manufacturers.keys():
        manufacturer = manufacturers[jedec_id[0]]
    else:
        manufacturer = "unknown"
    capacity = 1 << jedec_id[2]
    print(
        "\nFlash cart JEDEC ID    : {:02X}{:02X}{:02X}".format(
            jedec_id[0], jedec_id[1], jedec_id[2]
        )
    )
    print("Flash cart Manufacturer: {}".format(manufacturer))
    if manufacturer != "unknown":
        print("Flash cart capacity    : {} KB\n".format(capacity // 1024))

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
        sys.stdout.write("\rWriting block {}/{}".format(block + 1, blocks))
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
                print(" verify failed!\n\nWrite aborted.")
                break

    # write complete
    bootloader.write(b"x\x44")  # RGB LED GREEN, buttons enabled
    bootloader.read(1)
    time.sleep(0.5)
    bootloaderExit()
    print("\n\nDone in {} seconds".format(round(time.time() - oldtime, 2)))


################################################################################


def usage():
    print(
        "\nUSAGE:\n\n{} [pagenumber] flashdata.bin".format(
            os.path.basename(sys.argv[0])
        )
    )
    print(
        "{} [-d datafile.bin] [-s savefile.bin | -z savesize]".format(
            os.path.basename(sys.argv[0])
        )
    )
    print()
    print("[pagenumber]   Write flashdata.bin to flash starting at pagenumber. When no")
    print("               pagenumber is specified, page 0 is used instead.")
    print("-d --datafile  Write datafile to end of flash for development.")
    print("-s --savefile  Write savedata to end of flash for development.")
    print(
        "-z --savesize  Creates blank savedata (all 0xFF) at end of flash for development"
    )
    delayedExit()


################################################################################

try:
    opts, args = getopt(
        sys.argv[1:], "hd:s:z:", ["datafile=", "savefile=", "savesize="]
    )
except:
    usage()
## verify each block after writing if script name contains verify ##
verifyAfterWrite = os.path.basename(sys.argv[0]).find("verify") >= 0

## handle development writing ##
if len(opts) > 0:
    programdata = bytearray()
    savedata = bytearray()
    for o, a in opts:
        if o == "-d" or o == "--datafile":
            print('Reading program data from file "{}"'.format(a))
            f = open(a, "rb")
            programdata = bytearray(f.read())
            f.close
        elif o == "-s" or o == "--savefile":
            print('Reading save data from file "{}"'.format(a))
            f = open(a, "rb")
            savedata = bytearray(f.read())
            f.close
        elif (o == "-z") or (o == "--savesize"):
            savedata = bytearray(b"\xFF" * int(a))
        else:
            usage()
    if len(programdata) % PAGESIZE:
        programdata += b"\xFF" * (PAGESIZE - (len(programdata) % PAGESIZE))
    if len(savedata) % BLOCKSIZE:
        savedata += b"\xFF" * (BLOCKSIZE - (len(savedata) % BLOCKSIZE))
    savepage = MAX_PAGES - (len(savedata) // PAGESIZE)
    programpage = savepage - (len(programdata) // PAGESIZE)
    writeFlash(programpage, programdata + savedata)
    print("\nPlease use the following line in your program setup function:\n")
    if savepage < MAX_PAGES:
        print("  Cart::begin(0x{:04X}, 0x{:04X});\n".format(programpage, savepage))
    else:
        print("  Cart::begin(0x{:04X});\n".format(programpage))
    print("\nor use defines at the beginning of your program:\n")
    print("#define PROGRAM_DATA_PAGE 0x{:04X}".format(programpage))
    if savepage < MAX_PAGES:
        print("#define PROGRAM_SAVE_PAGE 0x{:04X}".format(savepage))
    print("\nand use the following in your program setup function:\n")
    if savepage < MAX_PAGES:
        print("  Cart::begin(PROGRAM_DATA_PAGE, PROGRAM_SAVE_PAGE);\n")
    else:
        print("  Cart::begin(PROGRAM_DATA_PAGE);\n")

# handle image writing ##
else:
    if len(args) == 1:
        pagenumber = 0
        filename = args[0]
    elif len(args) == 2:
        pagenumber = int(args[0], base=0)
        filename = args[1]
    else:
        usage()

    ## load and pad imagedata to multiple of PAGESIZE bytes ##
    if not os.path.isfile(filename):
        print("File not found. [{}]".format(filename))
        delayedExit()

    print('Reading flash image from file "{}"'.format(filename))
    f = open(filename, "rb")
    flashdata = bytearray(f.read())
    f.close
    if len(flashdata) % PAGESIZE != 0:
        flashdata += b"\xFF" * (PAGESIZE - (len(flashdata) % PAGESIZE))

    ## Apply patch for SSD1309 displays if script name contains 1309 ##
    if os.path.basename(sys.argv[0]).find("1309") >= 0:
        print("Patching image for SSD1309 displays...\n")
        lcdBootProgram_addr = 0
        while lcdBootProgram_addr >= 0:
            lcdBootProgram_addr = flashdata.find(lcdBootProgram, lcdBootProgram_addr)
            if lcdBootProgram_addr >= 0:
                flashdata[lcdBootProgram_addr + 2] = 0xE3
                flashdata[lcdBootProgram_addr + 3] = 0xE3
    writeFlash(pagenumber, flashdata)

delayedExit()

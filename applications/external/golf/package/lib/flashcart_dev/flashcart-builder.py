print("\nArduboy Flashcart image builder v1.08 by Mr.Blinky Jun 2018 - Jun 2021\n")

# requires PILlow. Use 'python -m pip install pillow' to install

import sys
import time
import os
import csv
import math
from hashlib import sha256

try:
    from PIL import Image
except:
    print("The PILlow module is required but not installed!")
    print("Use 'python -m pip install pillow' from the commandline to install.")
    sys.exit()

# CSV indices
ID_LIST = 0
ID_TITLE = 1
ID_TITLESCREEN = 2
ID_HEXFILE = 3
ID_DATAFILE = 4
ID_SAVEFILE = 5
ID_VERSION = 6
ID_DEVELOPER = 7
ID_INFO = 8
ID_LIKES = 9
# ID_EEPROM_START = 10
# ID_EEPROM_SIZE = 11
# ID_EEPROM_FILE = 12
ID_MAX = 10

# Menu patcher data
MenuButtonPatch = (
    b"\x0f\x92\x0f\xb6\x8f\x93\x9f\x93\xef\x93\xff\x93\x80\x91\xcc\x01"
    + b"\x8d\x5f\x8d\x37\x08\xf0\x8d\x57\x80\x93\xcc\x01\xe2\xe4\xf3\xe0"
    + b"\x80\x81\x8e\x4f\x80\x83\x91\x81\x9f\x4f\x91\x83\x82\x81\x8f\x4f"
    + b"\x82\x83\x83\x81\x8f\x4f\x83\x83\xed\xec\xf1\xe0\x80\x81\x8f\x5f"
    + b"\x80\x83\x81\x81\x8f\x4f\x81\x83\x82\x81\x8f\x4f\x82\x83\x83\x81"
    + b"\x8f\x4f\x83\x83\x8f\xb1\x8f\x60\x66\x99\x1c\x9b\x88\x27\x8f\x36"
    + b"\x81\xf4\x80\x91\xFF\x0A\x98\x1b\x96\x30\x68\xf0\xe0\xe0\xf8\xe0"
    + b"\x87\xe7\x80\x83\x81\x83\x88\xe1\x80\x93\x60\x00\xf0\x93\x60\x00"
    + b"\xff\xcf\x90\x93\xFF\x0A\xff\x91\xef\x91\x9f\x91\x8f\x91\x0f\xbe"
    + b"\x0f\x90\x18\x95"
)
MBP_fract_lds = 14
MBP_fract_sts = 26
MBP_millis_r30 = 28
MBP_millis_r31 = 30
MBP_overflow_r30 = 56
MBP_overflow_r31 = 58


def alignSize(length, alignment):
    n = alignment - length % alignment
    if n == alignment:
        n = 0
    return n


def fixPath(filename):
    if os.sep == "\\":
        return filename.replace("/", "\\")
    return filename.replace("\\", "/")


def DelayedExit():
    time.sleep(3)
    sys.exit()


def DefaultHeader():
    return bytearray("ARDUBOY".encode() + (b"\xFF" * 249))


def LoadTitleScreenData(filename):
    if not os.path.isabs(filename):
        filename = path + filename
    if not os.path.isfile(filename):
        print("Error: Title screen '{}' not found.".format(filename))
        DelayedExit()
    img = Image.open(filename).convert("1")
    width, height = img.size
    if (width != 128) or (height != 64):
        if height // (width // 128) != 64:
            print(
                "Error: Title screen '{}' is not 128 x 64 pixels or a multiple of that.".format(
                    filename
                )
            )
            DelayedExit()
        else:
            img = img.resize((128, 64), Image.NEAREST)
            width, height = img.size
    pixels = list(img.getdata())
    bytes = bytearray(int((height // 8) * width))
    i = 0
    b = 0
    for y in range(0, height, 8):
        for x in range(0, width):
            for p in range(0, 8):
                b = b >> 1
                if pixels[(y + p) * width + x] > 0:
                    b |= 0x80
            bytes[i] = b
            i += 1
    return bytes


def LoadHexFileData(filename):
    if not os.path.isabs(filename):
        filename = path + filename
    if not os.path.isfile(filename):
        return bytearray()
    f = open(filename, "r")
    records = f.readlines()
    f.close()
    bytes = bytearray(b"\xFF" * 32768)
    flash_end = 0
    for rcd in records:
        if rcd == ":00000001FF":
            break
        if rcd[0] == ":":
            rcd_len = int(rcd[1:3], 16)
            rcd_typ = int(rcd[7:9], 16)
            rcd_addr = int(rcd[3:7], 16)
            rcd_sum = int(rcd[9 + rcd_len * 2 : 11 + rcd_len * 2], 16)
            if (rcd_typ == 0) and (rcd_len > 0):
                flash_addr = rcd_addr
                checksum = rcd_sum
                for i in range(1, 9 + rcd_len * 2, 2):
                    byte = int(rcd[i : i + 2], 16)
                    checksum = (checksum + byte) & 0xFF
                    if i >= 9:
                        bytes[flash_addr] = byte
                        flash_addr += 1
                        if flash_addr > flash_end:
                            flash_end = flash_addr
                if checksum != 0:
                    print("Error: Hex file '{}' contains errors.".format(filename))
                    DelayedExit()
    flash_end = int((flash_end + 255) / 256) * 256
    return bytes[0:flash_end]


def LoadDataFile(filename):
    if not os.path.isabs(filename):
        filename = path + filename
    if not os.path.isfile(filename):
        return bytearray()

    with open(filename, "rb") as file:
        bytes = bytearray(file.read())
        pagealign = bytearray(b"\xFF" * alignSize(len(bytes), 256))
        return bytes + pagealign


def LoadSaveFile(filename):
    if not os.path.isabs(filename):
        filename = path + filename
    if not os.path.isfile(filename):
        return bytearray()

    with open(filename, "rb") as file:
        bytes = bytearray(file.read())
        pagealign = bytearray(b"\xFF" * alignSize(len(bytes), 4096))
        return bytes + pagealign


def PatchMenuButton():
    global program
    if len(program) < 256:
        return ""
    vector_23 = (program[0x5E] << 1) | (program[0x5F] << 9)  # ISR timer0 vector addr
    p = vector_23
    l = 0
    lds = 0
    branch = 0
    timer0_millis = 0
    timer0_fract = 0
    timer0_overflow_count = 0
    while p < (len(program) - 2):
        p += 2  # handle 2 byte instructions
        if program[p - 2 : p] == b"\x08\x95":  # ret instruction
            l = -1
            break
        if (program[p - 1] & 0xFC == 0xF4) & (
            program[p - 2] & 0x07 == 0x00
        ):  # brcc instruction may jump beyond reti
            branch = ((program[p - 1] & 0x03) << 6) + ((program[p - 2] & 0xF8) >> 2)
            if branch < 128:
                branch = p + branch
            else:
                branch = p - 256 + branch
        if program[p - 2 : p] == b"\x18\x95":  # reti instruction
            l = p - vector_23
            if p > branch:  # there was no branch beyond reti instruction
                break
        if l != 0:  # branced beyond reti, look for rjmp instruction
            if program[p - 1] & 0xF0 == 0xC0:
                l = p - vector_23
                break
        # handle 4 byte instructions
        if (program[p - 1] & 0xFE == 0x90) & (
            program[p - 2] & 0x0F == 0x00
        ):  # lds instruction
            lds += 1
            if lds == 1:
                timer0_millis = program[p] | (program[p + 1] << 8)
            elif lds == 5:
                timer0_fract = program[p] | (program[p + 1] << 8)
            elif lds == 6:
                timer0_overflow_count = program[p] | (program[p + 1] << 8)
            p += 2
        if (program[p - 1] & 0xFE == 0x92) & (
            program[p - 2] & 0x0F == 0x00
        ):  # sts instruction
            p += 2
    if l == -1:
        return "No menu patch applied. ISR contains subroutine."
    elif l < len(MenuButtonPatch):
        return "No menu patch applied. ISR size too small ({} bytes)".format(l)
    elif (timer0_millis == 0) | (timer0_fract == 0) | (timer0_overflow_count == 0):
        return "No menu patch applied. Custom ISR in use."
    else:
        # patch the new ISR code with 'hold UP + DOWN for 2 seconds to start bootloader menu' feature
        program[vector_23 : vector_23 + len(MenuButtonPatch)] = MenuButtonPatch
        # fix timer variables
        program[vector_23 + MBP_fract_lds + 0] = timer0_fract & 0xFF
        program[vector_23 + MBP_fract_lds + 1] = timer0_fract >> 8
        program[vector_23 + MBP_fract_sts + 0] = timer0_fract & 0xFF
        program[vector_23 + MBP_fract_sts + 1] = timer0_fract >> 8
        program[vector_23 + MBP_millis_r30 + 0] = 0xE0 | (timer0_millis >> 0) & 0x0F
        program[vector_23 + MBP_millis_r30 + 1] = 0xE0 | (timer0_millis >> 4) & 0x0F
        program[vector_23 + MBP_millis_r31 + 0] = 0xF0 | (timer0_millis >> 8) & 0x0F
        program[vector_23 + MBP_millis_r31 + 1] = 0xE0 | (timer0_millis >> 12) & 0x0F
        program[vector_23 + MBP_overflow_r30 + 0] = (
            0xE0 | (timer0_overflow_count >> 0) & 0x0F
        )
        program[vector_23 + MBP_overflow_r30 + 1] = (
            0xE0 | (timer0_overflow_count >> 4) & 0x0F
        )
        program[vector_23 + MBP_overflow_r31 + 0] = (
            0xF0 | (timer0_overflow_count >> 8) & 0x0F
        )
        program[vector_23 + MBP_overflow_r31 + 1] = (
            0xE0 | (timer0_overflow_count >> 12) & 0x0F
        )
        return "Menu patch applied"


################################################################################

if len(sys.argv) != 2:
    print("\nUsage: {} flashcart-index.csv\n".format(os.path.basename(sys.argv[0])))
    DelayedExit()

previouspage = 0xFFFF
currentpage = 0
nextpage = 0
csvfile = os.path.abspath(sys.argv[1])
path = os.path.dirname(csvfile) + os.sep
if not os.path.isfile(csvfile):
    print("Error: CSV-file '{}' not found.".format(csvfile))
    DelayedExit()
TitleScreens = 0
Sketches = 0
filename = path + os.path.basename(csvfile).lower().replace("-index", "").replace(
    ".csv", "-image.bin"
)
with open(filename, "wb") as binfile:
    with open(csvfile, "r") as file:
        data = csv.reader(file, quotechar='"', delimiter=";")
        next(data, None)
        print("Building: {}\n".format(filename))
        print(
            "List Title                     Curr. Prev. Next  ProgSize DataSize SaveSize"
        )
        print(
            "---- ------------------------- ----- ----- ----- -------- -------- --------"
        )
        for row in data:
            while len(row) < ID_MAX:
                row.append("")  # add missing cells
            header = DefaultHeader()
            title = LoadTitleScreenData(fixPath(row[ID_TITLESCREEN]))
            program = LoadHexFileData(fixPath(row[ID_HEXFILE]))
            programsize = len(program)
            datafile = LoadDataFile(fixPath(row[ID_DATAFILE]))
            datasize = len(datafile)
            savefile = LoadSaveFile(fixPath(row[ID_SAVEFILE]))
            savesize = len(savefile)
            id = sha256(program + datafile).digest()
            programpage = currentpage + 5
            datapage = programpage + (programsize >> 8)
            alignpage = datapage + (datasize >> 8)
            alignsize = alignSize(alignpage, 16) * 256 if savesize > 0 else 0
            slotsize = ((programsize + datasize + alignsize + savesize) >> 8) + 5
            savepage = alignpage + (alignsize >> 8)
            nextpage += slotsize
            header[7] = int(row[ID_LIST])  # list number
            header[8] = previouspage >> 8
            header[9] = previouspage & 0xFF
            header[10] = nextpage >> 8
            header[11] = nextpage & 0xFF
            header[12] = slotsize >> 8
            header[13] = slotsize & 0xFF
            header[14] = programsize >> 7  # program size in 128 byte pages
            if programsize > 0:
                header[15] = programpage >> 8
                header[16] = programpage & 0xFF
                if datasize > 0:
                    program[0x14] = 0x18
                    program[0x15] = 0x95
                    program[0x16] = datapage >> 8
                    program[0x17] = datapage & 0xFF
                if savesize > 0:
                    program[0x18] = 0x18
                    program[0x19] = 0x95
                    program[0x1A] = savepage >> 8
                    program[0x1B] = savepage & 0xFF
            if datasize > 0:
                header[17] = datapage >> 8
                header[18] = datapage & 0xFF
            if savesize > 0:
                header[19] = savepage >> 8
                header[20] = savepage & 0xFF
            header[25:57] = id
            if programsize > 0:
                stringdata = (
                    row[ID_TITLE].encode("utf-8")
                    + b"\0"
                    + row[ID_VERSION].encode("utf-8")
                    + b"\0"
                    + row[ID_DEVELOPER].encode("utf-8")
                    + b"\0"
                    + row[ID_INFO].encode("utf-8")
                    + b"\0"
                )
            else:
                stringdata = (
                    row[ID_TITLE].encode("utf-8")
                    + b"\0"
                    + row[ID_INFO].encode("utf-8")
                    + b"\0"
                )
            if len(stringdata) > 199:
                stringdata = stringdata[:199]
            header[57 : 57 + len(stringdata)] = stringdata
            binfile.write(header)
            binfile.write(title)
            patchresult = PatchMenuButton()
            binfile.write(program)
            binfile.write(datafile)
            binfile.write(bytearray(b"\xFF" * alignsize))
            binfile.write(savefile)
            if programsize == 0:
                print(
                    "{:4} {:25} {:5} {:5} {:5}".format(
                        row[ID_LIST], row[ID_TITLE], currentpage, previouspage, nextpage
                    )
                )
            else:
                print(
                    "{:4}  {:24} {:5} {:5} {:5} {:8} {:8} {:8} {}".format(
                        row[ID_LIST],
                        row[ID_TITLE][:24],
                        currentpage,
                        previouspage,
                        nextpage,
                        programsize,
                        datasize,
                        savesize,
                        patchresult,
                    )
                )
            previouspage = currentpage
            currentpage = nextpage
            if programsize > 0:
                Sketches += 1
            else:
                TitleScreens += 1
        print(
            "---- ------------------------- ----- ----- ----- -------- -------- --------"
        )
        print(
            "                                Page  Page  Page    Bytes    Bytes    Bytes"
        )

print(
    "\nImage build complete with {} Title screens, {} Sketches, {} Kbyte used.".format(
        TitleScreens, Sketches, (nextpage + 3) / 4
    )
)
DelayedExit

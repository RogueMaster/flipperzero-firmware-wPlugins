# FX data build tool version 1.15 by Mr.Blinky May 2021 - Mar.2023

VERSION = "1.15"

import sys
import os
import re
import platform

constants = [
    # normal bitmap modes
    ("dbmNormal", 0x00),
    ("dbmOverwrite", 0x00),
    ("dbmWhite", 0x01),
    ("dbmReverse", 0x08),
    ("dbmBlack", 0x0D),
    ("dbmInvert", 0x02),
    # masked bitmap modes for frame
    ("dbmMasked", 0x10),
    ("dbmMasked_dbmWhite", 0x11),
    ("dbmMasked_dbmReverse", 0x18),
    ("dbmMasked_dbmBlack", 0x1D),
    ("dbmMasked_dbmInvert", 0x12),
    # bitmap modes for last bitmap in a frame
    ("dbmNormal_end", 0x40),
    ("dbmOverwrite_end", 0x40),
    ("dbmWhite_end", 0x41),
    ("dbmReverse_end", 0x48),
    ("dbmBlack_end", 0x4D),
    ("dbmInvert_end", 0x42),
    # masked bitmap modes for last bitmap in a frame
    ("dbmMasked_end", 0x50),
    ("dbmMasked_dbmWhite_end", 0x51),
    ("dbmMasked_dbmReverse_end", 0x58),
    ("dbmMasked_dbmBlack_end", 0x5D),
    ("dbmMasked_dbmInvert_end", 0x52),
    # bitmap modes for last bitmap of the last frame
    ("dbmNormal_last", 0x80),
    ("dbmOverwrite_last", 0x80),
    ("dbmWhite_last", 0x81),
    ("dbmReverse_last", 0x88),
    ("dbmBlack_last", 0x8D),
    ("dbmInvert_last", 0x82),
    # masked bitmap modes for last bitmap in a frame
    ("dbmMasked_last", 0x90),
    ("dbmMasked_dbmWhite_last", 0x91),
    ("dbmMasked_dbmReverse_last", 0x98),
    ("dbmMasked_dbmBlack_last", 0x9D),
    ("dbmMasked_dbmInvert_last", 0x92),
]


def print(s):
    sys.stdout.write(s + "\n")
    sys.stdout.flush()


print(
    "FX data build tool version {} by Mr.Blinky May 2021 - Jan 2023\nUsing Python version {}".format(
        VERSION, platform.python_version()
    )
)

bytes = bytearray()
symbols = []
header = []
label = ""
indent = ""
blkcom = False
namespace = False
include = False
try:
    toolspath = os.path.dirname(os.path.abspath(sys.argv[0]))
    sys.path.insert(0, toolspath)
    from PIL import Image
except Exception as e:
    sys.stderr.write(str(e) + "\n")
    sys.stderr.write("PILlow python module not found or wrong version.\n")
    sys.stderr.write(
        "Make sure the correct module is installed or placed at {}\n".format(toolspath)
    )
    sys.exit(-1)


def rawData(filename):
    global path
    with open(path + filename, "rb") as file:
        bytes = bytearray(file.read())
        file.close()
        return bytes


def includeFile(filename):
    global path
    print("Including file {}".format(path + filename))
    with open(path + filename, "r") as file:
        lines = file.readlines()
        file.close()
        return lines


def imageData(filename):
    global path, symbols
    filename = path + filename

    ## parse filename ## FILENAME_[WxH]_[S].[EXT]"
    spriteWidth = 0
    spriteHeight = 0
    spacing = 0
    elements = os.path.basename(os.path.splitext(filename)[0]).split("_")
    lastElement = len(elements) - 1
    # get width and height from filename
    i = lastElement
    while i > 0:
        subElements = list(filter(None, elements[i].split("x")))
        if (
            len(subElements) == 2
            and subElements[0].isnumeric()
            and subElements[1].isnumeric()
        ):
            spriteWidth = int(subElements[0])
            spriteHeight = int(subElements[1])
            if i < lastElement and elements[i + 1].isnumeric():
                spacing = int(elements[i + 1])
            break
        else:
            i -= 1

    # load image
    img = Image.open(filename).convert("RGBA")
    pixels = list(img.getdata())
    # check for transparency
    transparency = False
    for i in pixels:
        if i[3] < 255:
            transparency = True
            break

    # check for multiple frames/tiles
    if spriteWidth > 0:
        hframes = (img.size[0] - spacing) // (spriteWidth + spacing)
    else:
        spriteWidth = img.size[0] - 2 * spacing
        hframes = 1
    if spriteHeight > 0:
        vframes = (img.size[1] - spacing) // (spriteHeight + spacing)
    else:
        spriteHeight = img.size[1] - 2 * spacing
        vframes = 1

    # create byte array for bin file
    size = (spriteHeight + 7) // 8 * spriteWidth * hframes * vframes
    if transparency:
        size += size
    bytes = bytearray(
        [spriteWidth >> 8, spriteWidth & 0xFF, spriteHeight >> 8, spriteHeight & 0xFF]
    )
    bytes += bytearray(size)
    i = 4
    b = 0
    m = 0
    fy = spacing
    frames = 0
    for v in range(vframes):
        fx = spacing
        for h in range(hframes):
            for y in range(0, spriteHeight, 8):
                line = "  "
                for x in range(0, spriteWidth):
                    for p in range(0, 8):
                        b = b >> 1
                        m = m >> 1
                        if (
                            y + p
                        ) < spriteHeight:  # for heights that are not a multiple of 8 pixels
                            if pixels[(fy + y + p) * img.size[0] + fx + x][1] > 64:
                                b |= 0x80  # white pixel
                            if pixels[(fy + y + p) * img.size[0] + fx + x][3] > 64:
                                m |= 0x80  # opaque pixel
                            else:
                                b &= 0x7F  # for transparent pixel clear possible white pixel
                    bytes[i] = b
                    i += 1
                    if transparency:
                        bytes[i] = m
                        i += 1
            frames += 1
            fx += spriteWidth + spacing
        fy += spriteHeight + spacing
    label = symbols[-1][0]
    if label.upper() == label:
        writeHeader(
            "{}constexpr uint16_t {}_WIDTH  = {};".format(indent, label, spriteWidth)
        )
        writeHeader(
            "{}constexpr uint16_t {}HEIGHT  = {};".format(indent, label, spriteHeight)
        )
        if frames > 1:
            writeHeader(
                "{}constexpr uint8_t  {}_FRAMES = {};".format(indent, label, frames)
            )
    elif "_" in label:
        writeHeader(
            "{}constexpr uint16_t {}_width  = {};".format(indent, label, spriteWidth)
        )
        writeHeader(
            "{}constexpr uint16_t {}_height = {};".format(indent, label, spriteHeight)
        )
        if frames > 1:
            writeHeader(
                "{}constexpr uint8_t  {}_frames = {};".format(indent, label, frames)
            )
    else:
        writeHeader(
            "{}constexpr uint16_t {}Width  = {};".format(indent, label, spriteWidth)
        )
        writeHeader(
            "{}constexpr uint16_t {}Height = {};".format(indent, label, spriteHeight)
        )
        if frames > 255:
            writeHeader(
                "{}constexpr uint16_t  {}Frames = {};".format(indent, label, frames)
            )
        elif frames > 1:
            writeHeader(
                "{}constexpr uint8_t  {}Frames = {};".format(indent, label, frames)
            )
    writeHeader("")
    return bytes


def addLabel(label, length):
    global symbols
    symbols.append((label, length))
    writeHeader("{}constexpr uint24_t {} = 0x{:06X};".format(indent, label, length))


def writeHeader(s):
    global header
    header.append(s)


################################################################################

if (len(sys.argv) != 2) or (os.path.isfile(sys.argv[1]) != True):
    sys.stderr.write("FX data script file not found.\n")
    sys.exit(-1)

filename = os.path.abspath(sys.argv[1])
datafilename = os.path.splitext(filename)[0] + "-data.bin"
savefilename = os.path.splitext(filename)[0] + "-save.bin"
devfilename = os.path.splitext(filename)[0] + ".bin"
headerfilename = os.path.splitext(filename)[0] + ".h"
path = os.path.dirname(filename) + os.sep
saveStart = -1

with open(filename, "r") as file:
    lines = file.readlines()
    file.close()

print("Building FX data using {}".format(filename))
lineNr = 0
while lineNr < len(lines):
    parts = [
        p
        for p in re.split("([ ,]|[\\'].*[\\'])", lines[lineNr])
        if p.strip() and p != ","
    ]
    for i in range(len(parts)):
        part = parts[i]
        # strip unwanted chars
        if part[:1] == "\t":
            part = part[1:]
        if part[:1] == "{":
            part = part[1:]
        if part[-1:] == "\n":
            part = part[:-1]
        if part[-1:] == ";":
            part = part[:-1]
        if part[-1:] == "}":
            part = part[:-1]
        if part[-1:] == ";":
            part = part[:-1]
        if part[-1:] == ".":
            part = part[:-1]
        if part[-1:] == ",":
            part = part[:-1]
        if part[-2:] == "[]":
            part = part[:-2]
        # handle comments
        if blkcom == True:
            p = part.find("*/", 2)
            if p >= 0:
                part = part[p + 2 :]
                blkcom = False
        else:
            if part[:2] == "//":
                break
            elif part[:2] == "/*":
                p = part.find("*/", 2)
                if p >= 0:
                    part = part[p + 2 :]
                else:
                    blkcom = True
            # handle types
            elif part == "=":
                pass
            elif part == "const":
                pass
            elif part == "PROGMEM":
                pass
            elif part == "align":
                t = 0
            elif part == "int8_t":
                t = 1
            elif part == "uint8_t":
                t = 1
            elif part == "int16_t":
                t = 2
            elif part == "uint16_t":
                t = 2
            elif part == "int24_t":
                t = 3
            elif part == "uint24_t":
                t = 3
            elif part == "int32_t":
                t = 4
            elif part == "uint32_t":
                t = 4
            elif part == "image_t":
                t = 5
            elif part == "raw_t":
                t = 6
            elif part == "String":
                t = 7
            elif part == "string":
                t = 7
            elif part == "include":
                include = True
            elif part == "datasection":
                pass
            elif part == "savesection":
                saveStart = len(bytes)
            # handle namespace
            elif part == "namespace":
                namespace = True
            elif namespace == True:
                namespace = False
                writeHeader("namespace {}\n{{".format(part))
                indent += "  "
            elif part == "namespace_end":
                indent = indent[:-2]
                writeHeader("}\n")
                namespace = False
            # handle strings
            elif (part[:1] == "'") or (part[:1] == '"'):
                if part[:1] == "'":
                    part = part[1 : part.rfind("'")]
                else:
                    part = part[1 : part.rfind('"')]
                # handle include
                if include == True:
                    lines[lineNr + 1 : lineNr + 1] = includeFile(part)
                    include = False
                elif t == 1:
                    bytes += (
                        part.encode("utf-8").decode("unicode_escape").encode("utf-8")
                    )
                elif t == 5:
                    bytes += imageData(part)
                elif t == 6:
                    bytes += rawData(part)
                elif t == 7:
                    bytes += (
                        part.encode("utf-8").decode("unicode_escape").encode("utf-8")
                        + b"\x00"
                    )
                else:
                    sys.stderr.write(
                        "PLATFORM_ERROR in line {}: unsupported string for type\n".format(
                            lineNr
                        )
                    )
                    sys.exit(-1)
            # handle values
            elif part[:1].isnumeric() or (part[:1] == "-" and part[1:2].isnumeric()):
                n = int(part, 0)
                if t == 4:
                    bytes.append((n >> 24) & 0xFF)
                if t >= 3:
                    bytes.append((n >> 16) & 0xFF)
                if t >= 2:
                    bytes.append((n >> 8) & 0xFF)
                if t >= 1:
                    bytes.append((n >> 0) & 0xFF)
                # handle align
                if t == 0:
                    align = len(bytes) % n
                    if align:
                        bytes += b"\xFF" * (n - align)
            # handle labels
            elif part[:1].isalpha():
                for j in range(len(part)):
                    if part[j] == "=":
                        addLabel(label, len(bytes))
                        label = ""
                        part = part[j + 1 :]
                        parts.insert(i + 1, part)
                        break
                    elif part[j].isalnum() or part[j] == "_":
                        label += part[j]
                    else:
                        sys.stderr.write(
                            "PLATFORM_ERROR in line {}: Bad label: {}\n".format(
                                lineNr, label
                            )
                        )
                        sys.exit(-1)
                if (label != "") and (i < len(parts) - 1) and (parts[i + 1][:1] == "="):
                    addLabel(label, len(bytes))
                    label = ""
                # handle included constants
                if label != "":
                    for symbol in constants:
                        if symbol[0] == label:
                            if t == 4:
                                bytes.append((symbol[1] >> 24) & 0xFF)
                            if t >= 3:
                                bytes.append((symbol[1] >> 16) & 0xFF)
                            if t >= 2:
                                bytes.append((symbol[1] >> 8) & 0xFF)
                            if t >= 1:
                                bytes.append((symbol[1] >> 0) & 0xFF)
                            label = ""
                            break
                # handle symbol values
                if label != "":
                    for symbol in symbols:
                        if symbol[0] == label:
                            if t == 4:
                                bytes.append((symbol[1] >> 24) & 0xFF)
                            if t >= 3:
                                bytes.append((symbol[1] >> 16) & 0xFF)
                            if t >= 2:
                                bytes.append((symbol[1] >> 8) & 0xFF)
                            if t >= 1:
                                bytes.append((symbol[1] >> 0) & 0xFF)
                            label = ""
                            break
                if label != "":
                    sys.stderr.write(
                        "PLATFORM_ERROR in line {}: Undefined symbol: {}\n".format(
                            lineNr, label
                        )
                    )
                    sys.exit(-1)
            elif len(part) > 0:
                sys.stderr.write(
                    "PLATFORM_ERROR unable to parse {} in element: {}\n".format(
                        part, str(parts)
                    )
                )
                sys.exit(-1)
    lineNr += 1

if saveStart >= 0:
    dataSize = saveStart
    dataPages = (dataSize + 255) // 256
    saveSize = len(bytes) - saveStart
    savePages = (saveSize + 4095) // 4096 * 16
else:
    dataSize = len(bytes)
    dataPages = (dataSize + 255) // 256
    saveSize = 0
    savePages = 0
    savePadding = 0
dataPadding = dataPages * 256 - dataSize
savePadding = savePages * 256 - saveSize

print("Saving FX data header file {}".format(headerfilename))
with open(headerfilename, "w") as file:
    file.write("#pragma once\n\n")
    file.write(
        "/**** FX data header generated by fxdata-build.py tool version {} ****/\n\n".format(
            VERSION
        )
    )
    file.write("using uint24_t = __uint24;\n\n")
    file.write(
        "// Initialize FX hardware using  FX::begin(FX_DATA_PAGE); in the setup() function.\n\n"
    )
    file.write(
        "constexpr uint16_t FX_DATA_PAGE  = 0x{:04x};\n".format(
            65536 - dataPages - savePages
        )
    )
    file.write("constexpr uint24_t FX_DATA_BYTES = {};\n\n".format(dataSize))
    if saveSize > 0:
        file.write(
            "constexpr uint16_t FX_SAVE_PAGE  = 0x{:04x};\n".format(65536 - savePages)
        )
        file.write("constexpr uint24_t FX_SAVE_BYTES = {};\n\n".format(saveSize))
    for line in header:
        file.write(line + "\n")
    file.close()

print("Saving {} bytes FX data to {}".format(dataSize, datafilename))
with open(datafilename, "wb") as file:
    file.write(bytes[0:dataSize])
    file.close()
if saveSize > 0:
    print("Saving {} bytes FX savedata to {}".format(saveSize, savefilename))
    with open(savefilename, "wb") as file:
        file.write(bytes[saveStart : len(bytes)])
        file.close()
print("Saving FX development data to {}".format(devfilename))
with open(devfilename, "wb") as file:
    file.write(bytes[0:dataSize])
    if dataPadding > 0:
        file.write(b"\xFF" * dataPadding)
    if saveSize > 0:
        file.write(bytes[saveStart : len(bytes)])
        if savePadding > 0:
            file.write(b"\xFF" * savePadding)
    file.close()

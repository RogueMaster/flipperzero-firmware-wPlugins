framename = "Title_Credits_Frame"

yPos = 64
torchIdx = 0

head = "    {}[] = {{\n\n".format(framename)
foot = "    }\n"

lastY = 64 - 136 - 64
frames = 0
with open(framename + ".txt", "w") as file:
    file.write(head)
    while yPos >= lastY:
        last = yPos == lastY
        file.write(
            "        int16_t   0, 54, uint24_t Credits_BG, uint8_t 0, dbmNormal\n"
        )
        file.write(
            "        int16_t   7, 39, uint24_t Torches,    uint8_t {}, dbmMasked\n".format(
                torchIdx
            )
        )
        file.write(
            "        int16_t 119, 39, uint24_t Torches,    uint8_t {}, dbmMasked\n".format(
                torchIdx
            )
        )
        file.write(
            "        int16_t  14, 34, uint24_t Torches,    uint8_t {}, dbmMasked\n".format(
                torchIdx
            )
        )
        file.write(
            "        int16_t 112, 34, uint24_t Torches,    uint8_t {}, dbmMasked\n".format(
                torchIdx
            )
        )
        file.write(
            "        int16_t  27, {}, uint24_t Title_Credits, uint8_t 0, dbmNormal\n".format(
                yPos
            )
        )
        file.write(
            "        int16_t   0, 0, int24_t Title_PoP,    int8_t 0, dbmMasked{}\n\n".format(
                "_last" if last else "_end"
            )
        )
        torchIdx = (torchIdx + 1) % 3
        yPos -= 1
        frames += 1
    file.write(foot)
    file.close()
print("{} bytes".format(frames * 7 * 9))

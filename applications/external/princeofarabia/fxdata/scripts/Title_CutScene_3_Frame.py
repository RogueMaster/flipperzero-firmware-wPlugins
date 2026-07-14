framename = "Title_CutScene_3_Frame"

count = 0
countEnd = 95
princess_x = 52

head = "    {}[] = {{\n\n".format(framename)
foot = "    }\n"
frames = 0
with open(framename + ".txt", "w") as file:
    file.write(head)
    while count <= countEnd:
        last = count == countEnd
        #
        if (frames <= 30) or (frames >= 37 and frames <= 67) or (frames >= 74):
            princess_image = 10
        elif (
            (frames == 31)
            or (frames == 32)
            or (frames == 35)
            or (frames == 36)
            or (frames == 68)
            or (frames == 69)
            or (frames == 72)
            or (frames == 73)
        ):
            princess_image = 24
        elif (frames == 33) or (frames == 34) or (frames == 70) or (frames == 71):
            princess_image = 25
        hourglass_image = 1 * 3 + (frames % 3)
        #
        # background with torches
        torch = (frames // 2) % 3
        file.write(
            "        int16_t  0,   0, uint24_t Chambers_BG, uint8_t 0, dbmNormal\n"
        )
        file.write(
            "        int16_t  10, 37, uint24_t Torches,     uint8_t {}, dbmMasked\n".format(
                torch
            )
        )
        file.write(
            "        int16_t 114, 37, uint24_t Torches,     uint8_t {}, dbmMasked\n".format(
                torch
            )
        )
        # princess, and hourglass
        file.write(
            "        int16_t {:3d}, 24, uint24_t Princess     uint8_t {}, dbmMasked\n".format(
                princess_x, princess_image
            )
        )
        file.write(
            "        int16_t  54, 40, uint24_t HourGlasses  uint8_t {}, dbmMasked\n".format(
                hourglass_image
            )
        )
        # forground
        file.write(
            "        int16_t  0,   0, uint24_t Chambers_FG, uint8_t 0, uint8_t {}\n\n".format(
                "dbmMasked_last" if last else "dbmMasked_end"
            )
        )
        count += 1
        frames += 1
    file.write(foot)
    file.close()

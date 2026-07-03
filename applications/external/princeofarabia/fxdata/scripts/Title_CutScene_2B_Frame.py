framename = "Title_CutScene_2B_Frame"

count = 0
# countEnd = 95
countEnd = 95 * 2 + 1
princess_x = 63

head = "    {}[] = {{\n\n".format(framename)
foot = "    }\n"
frames = 0
ix = -8
iy = 28 - 24
ii = 6
with open(framename + ".txt", "w") as file:
    file.write(head)
    while count <= countEnd:
        last = count == countEnd
        #
        # if (frames % 8) < 4:
        if ((frames // 2) % 8) < 4:
            princess_image = 4
        else:
            princess_image = 9
        # hourglass_image = 1 * 3 + ((frames // 2) % 3)
        hourglass_image = 1 * 3 + ((frames // 4) % 3)
        #
        # background with torches
        # torch = (frames // 2) % 3
        torch = (frames // 4) % 3
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
        # invader
        file.write(
            "        int16_t {:3d},{:3d}, uint24_t Invaders     uint8_t {}, dbmMasked\n".format(
                ix, iy, ii
            )
        )
        ii = 6 + ((frames // 4) & 1)
        if frames < 59:
            ix += 2
        elif frames < 63:
            iy += 2
        elif frames < 122:
            ix -= 2
        elif frames < 126:
            iy += 2
        elif frames < 155:
            ix += 2
        elif frames < 159:
            ix = 51
            iy += 2
        else:
            iy = 28
            ii = 6
        # princess, and hourglass
        file.write(
            "        int16_t {:3d}, 28, uint24_t Princess     uint8_t {}, dbmMasked\n".format(
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

framename = "Title_CutScene_4_Frame"

count = 0
countEnd = 47
princess_x = 67

head = "    {}[] = {{\n\n".format(framename)
foot = "    }\n"
frames = 0
with open(framename + ".txt", "w") as file:
    file.write(head)
    while count <= countEnd:
        last = count == countEnd
        #
        princess_image = 26 + ((frames // 2) % 7)
        hourglass_image = 1 * 3 + ((frames // 1) % 3)
        #
        # background with torches
        torch = (frames // 1) % 3
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

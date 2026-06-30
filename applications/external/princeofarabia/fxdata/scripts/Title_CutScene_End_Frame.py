framename = "Title_CutScene_End_Frame"

Scene9 = [
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (15, 0, 0, 0),
    (12, -3, 0, 0),
    (14, -3, 0, 0),
    (7, 0, 0, 0),
    (8, -3, 1, 0),
    (9, -3, 1, 0),
    (10, -3, 2, 0),
    (11, -3, 2, 0),
    (12, -3, 3, 0),
    (13, -3, 3, 0),
    (12, -3, 4, 0),
    (14, -3, 4, 0),
    (7, -3, 4, 0),
    (8, -3, 4, 0),
    (9, -3, 4, 0),
    (10, -3, 4, 0),
    (11, -3, 4, 0),
    (12, -3, 4, 0),
    (13, -3, 4, 0),
    (14, -3, 11, 1),
    (7, -3, 12, 1),
    (8, -3, 13, 1),
    (9, -3, 14, 1),
    (10, -3, 15, 1),
    (11, -3, 16, 2),
    (12, -3, 17, 2),
    (13, -3, 18, 1),
    (14, -3, 19, 1),
    (5, 0, 20, 1),
    (4, 0, 21, 1),
    (3, 0, 22, 0),
    (2, 0, 23, 0),
    (1, 0, 23, 0),
    (15, 0, 23, 0),
    (15, 0, 23, 0),
    (15, 0, 23, 0),
    (170, 0, 23, 0),
    (170, 0, 23, 0),
    (171, 0, 23, 0),
    (171, 0, 23, 0),
    (172, 0, 23, 0),
    (172, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (63, -2, 23, 0),
    (64, -1, 23, 0),
    (65, -1, 23, 0),
    (72, -1, 23, 0),
    (73, 0, 23, 0),
    (74, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
    (173, 0, 23, 0),
]

count = 0
countEnd = 127
heart_x = 60
heart_y = 25
princess_x = 29
princess_image = 0
prince_x = 132
prince_image = 15

head = "    {}[] = {{\n\n".format(framename)
foot = "    }\n"
frames = 0
with open(framename + ".txt", "w") as file:
    file.write(head)
    while count <= countEnd:
        last = count == countEnd
        #
        heart_image = 255
        heart_count = 2 * count
        if ((heart_count >= 190) and (heart_count <= 193)) or (
            (heart_count >= 198) and (heart_count <= 201)
        ):
            heart_image = 2
        elif ((heart_count >= 194) and (heart_count <= 197)) or (
            (heart_count >= 202) and (heart_count <= 205)
        ):
            heart_image = 3
        elif (heart_count == 206) or (heart_count == 208):
            heart_y -= 1
            heart_image = 1
        elif (heart_count == 207) or (heart_count == 209):
            heart_image = 1
        elif (heart_count == 210) or (heart_count == 212):
            heart_y -= 1
            heart_image = 0
        elif (heart_count == 211) or (heart_count == 213):
            heart_image = 0
        #
        # background with torches
        torch = (frames // 2) % 3
        file.write(
            "        int16_t  0, 0,   uint24_t Chambers_BG, uint8_t 0, dbmNormal\n"
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
        # prince and princess
        if prince_x < 128:
            file.write(
                "        int16_t {:3d}, 29, uint24_t Prince_Left       uint8_t {}, dbmMasked\n".format(
                    prince_x, prince_image - 1
                )
            )
        file.write(
            "        int16_t {:3d}, 28, uint24_t Princess     uint8_t {}, dbmMasked\n".format(
                princess_x, princess_image
            )
        )
        if heart_image != 255:
            file.write(
                "        int16_t {:3d},{:3d}, uint24_t Hearts       uint8_t {}, dbmNormal\n".format(
                    heart_x, heart_y, heart_image
                )
            )
        # forground
        file.write(
            "        int16_t  0, 0,   uint24_t Chambers_FG, uint8_t 0, uint8_t dbmMasked{}\n\n".format(
                "_last" if last else "_end"
            )
        )
        prince_image = Scene9[count][0]
        prince_x = prince_x + Scene9[count][1]
        princess_image = Scene9[count][2]
        princess_x = princess_x + Scene9[count][3]
        count += 1
        frames += 1
    file.write(foot)
    file.close()

framename = "Title_IntroGame_End_Frame"

yPos = 0
yStep = 1
yLast = 160 + 64 + 12 - 1  # 223 + 12

head = "    {}[] = {{\n\n".format(framename)
foot = "    }\n"
frames = 0
with open(framename + ".txt", "w") as file:
    file.write(head)
    while yPos <= yLast:
        last = yPos == yLast
        file.write(
            "        int16_t  2,{:3d}, int24_t IntroGame_9,   uint8_t 0, dbmNormal{}\n\n".format(
                64 - yPos, "_last" if last else "_end"
            )
        )
        yPos += yStep
        frames += 1
    file.write(foot)
    file.close()

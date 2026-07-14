framename = "Title_IntroGame_1B_Frame"

yPos = 0
yStep = 1
yLast = 165

head = "    {}[] = {{\n\n".format(framename)
foot = "    }\n"
frames = 0
with open(framename + ".txt", "w") as file:
    file.write(head)
    while yPos <= yLast:
        last = yPos == yLast
        yTitle = -32 + (yPos // 2)
        file.write(
            "        int16_t  3,{:3d}, int24_t IntroGame_1B,   uint8_t 0, dbmNormal{}\n\n".format(
                64 - yPos, "_last" if last else "_end"
            )
        )
        yPos += yStep
        frames += 1
    file.write(foot)
    file.close()
print("{} bytes".format(frames * 9))

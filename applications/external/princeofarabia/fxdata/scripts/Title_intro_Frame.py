framename = "Title_Intro_Frame"

yPos = 0
yStep = 1
yLast = 64

head = "    {}[] = {{\n\n".format(framename)
foot = "    }\n"
frames = 0
with open(framename + ".txt", "w") as file:
    file.write(head)
    while yPos <= yLast:
        last = yPos == yLast
        yTitle = -32 + (yPos // 2)
        if last:
            file.write("\n    Title_Intro_Last_Frame[] =\n\n")
        file.write(
            "        int16_t  0,{:3d}, uint24_t Title_Main, uint8_t 0, dbmNormal\n".format(
                64 - frames
            )
        )
        file.write(
            "        int16_t  0,{:3d}, int24_t Title_PoP,   uint8_t 0, dbmMasked{}\n\n".format(
                yTitle, "_last" if last else "_end"
            )
        )
        yPos += yStep
        frames += 1
    file.write(foot)
    file.close()
print("{} bytes".format(frames * 7 * 9))

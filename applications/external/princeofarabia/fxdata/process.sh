#!/bin/bash

python3 scripts/ReplaceTiles.py ../levels/Level1_BG.csv ../levels/Level1_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level1_FG.csv ../levels/Level1_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level2_BG.csv ../levels/Level2_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level2_FG.csv ../levels/Level2_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level3_BG.csv ../levels/Level3_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level3_FG.csv ../levels/Level3_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level4_BG.csv ../levels/Level4_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level4_FG.csv ../levels/Level4_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level5_BG.csv ../levels/Level5_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level5_FG.csv ../levels/Level5_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level6_BG.csv ../levels/Level6_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level6_FG.csv ../levels/Level6_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level7_BG.csv ../levels/Level7_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level7_FG.csv ../levels/Level7_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level8_BG.csv ../levels/Level8_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level8_FG.csv ../levels/Level8_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level9_BG.csv ../levels/Level9_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level9_FG.csv ../levels/Level9_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level10_BG.csv ../levels/Level10_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level10_FG.csv ../levels/Level10_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level11_BG.csv ../levels/Level11_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level11_FG.csv ../levels/Level11_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level12_BG.csv ../levels/Level12_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level12_FG.csv ../levels/Level12_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level13_BG.csv ../levels/Level13_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level13_FG.csv ../levels/Level13_FG_convert.csv

python3 scripts/ReplaceTiles.py ../levels/Level14_StandingJumps_bg.csv ../levels/Level14_StandingJumps_bg_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level14_StandingJumps_fg.csv ../levels/Level14_StandingJumps_fg_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level15_RunningJumps_BG.csv ../levels/Level15_RunningJumps_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level15_RunningJumps_FG.csv ../levels/Level15_RunningJumps_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level20_RunningJumps_BG.csv ../levels/Level20_RunningJumps_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level20_RunningJumps_FG.csv ../levels/Level20_RunningJumps_FG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level28_RunningJumps_BG.csv ../levels/Level28_RunningJumps_BG_convert.csv
python3 scripts/ReplaceTiles.py ../levels/Level28_RunningJumps_FG.csv ../levels/Level28_RunningJumps_FG_convert.csv

#montage ../images/prince/left/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 15x15 ../images/prince/left/Prince_Left_36x36.png
#montage ../images/prince/right/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 15x15 ../images/prince/right/Prince_Right_36x36.png
#montage ../images/mirror/left/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 15x9 ../images/mirror/left/Mirror_Left_36x36.png
#montage ../images/mirror/right/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 15x9 ../images/mirror/right/Mirror_Right_36x36.png
#montage ../images/enemy/left/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 10x4 ../images/enemy/left/Enemy_Left_36x36.png
#montage ../images/enemy/right/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 10x4 ../images/enemy/right/Enemy_Right_36x36.png
#montage ../images/skeleton/left/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 10x4 ../images/skeleton/left/Skeleton_Left_36x36.png
#montage ../images/skeleton/right/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 10x4 ../images/skeleton/right/Skeleton_Right_36x36.png

python3 scripts/img2sheet.py ../images/prince ../images/Prince_Left False
python3 scripts/img2sheet.py ../images/prince ../images/Prince_Right True
python3 scripts/img2sheet.py ../images/mirror ../images/Mirror_Left False
python3 scripts/img2sheet.py ../images/mirror ../images/Mirror_Right True
python3 scripts/img2sheet.py ../images/enemy ../images/Enemy_Left False
python3 scripts/img2sheet.py ../images/enemy ../images/Enemy_Right True
python3 scripts/img2sheet.py ../images/skeleton ../images/Skeleton_Left False
python3 scripts/img2sheet.py ../images/skeleton ../images/Skeleton_Right True

python3 scripts/img2sheet.py ../images/princess/Hearts ../images/Hearts False
python3 scripts/img2sheet.py ../images/princess/Princess ../images/Princess False
python3 scripts/img2sheet.py ../images/princess/Princess_Bounce ../images/Princess_Bounce False


#montage ../images/princess/Princess/*.png -geometry 32x32+0+0 -background none -tile 33x1 ../images/princess/Princess_32x32.png
#montage ../images/princess/Princess_Bounce/*.png -geometry 48x32+0+0 -background none -tile 17x1 ../images/princess/Princess_Bounce_48x32.png
#montage ../images/princess/Hearts/*.png -geometry 7x6+0+0 -background none -tile 4x1 ../images/princess/Hearts_7x6.png



python3 scripts/createMovements.py ../src/utils/Constants.h movements.txt
python3 scripts/createMovements2.py ../src/utils/Constants.h movements2.txt
python3 scripts/createSEQData.py ../src/utils/Constants.h ../src/utils/movements_SEQData.h
python3 scripts/createOFFSETSData.py ../src/utils/Constants.h ../src/utils/movements_OFFSETSData.h

python3 scripts/ReplaceImages.py movements.txt movements_convert.txt

python3 ./Arduboy-Python-Utilities-master/fxdata-build.py fxdata.txt

cp fxdata.bin ../../build/fxdata.bin



# montage ???_*.png -geometry 36x36+0+0 -background none -tile 15x15 Mirror_Right_36x36.png
# ./midiConverter -o2 -k12 -fa pop_ending

# montage ../images/enemy/left/Backup/???_*.png -geometry 36x36+0+0 -background none -tile 15x15 ../images/enemy/left/Enemy_Left_36x36.png

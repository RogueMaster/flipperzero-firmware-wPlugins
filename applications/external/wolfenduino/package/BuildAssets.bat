set IMAGE_ENCODER=Debug\ImageEncoder.exe
set OUTPUT_FOLDER=Wolf\Generated

%IMAGE_ENCODER% Assets\pistol.png %OUTPUT_FOLDER%\Data_Pistol Data_pistolSprite sprite
%IMAGE_ENCODER% Assets\machinegun.png %OUTPUT_FOLDER%\Data_Machinegun Data_machinegunSprite sprite
%IMAGE_ENCODER% Assets\chaingun.png %OUTPUT_FOLDER%\Data_Chaingun Data_chaingunSprite sprite
%IMAGE_ENCODER% Assets\knife.png %OUTPUT_FOLDER%\Data_Knife Data_knifeSprite sprite
%IMAGE_ENCODER% Assets\items.png %OUTPUT_FOLDER%\Data_Items Data_itemSprites sprite
%IMAGE_ENCODER% Assets\guard.png %OUTPUT_FOLDER%\Data_Guard Data_guardSprite sprite
%IMAGE_ENCODER% Assets\dog.png %OUTPUT_FOLDER%\Data_Dog Data_dogSprite sprite
%IMAGE_ENCODER% Assets\ss.png %OUTPUT_FOLDER%\Data_SS Data_ssSprite sprite
%IMAGE_ENCODER% Assets\boss.png %OUTPUT_FOLDER%\Data_Boss Data_bossSprite sprite
%IMAGE_ENCODER% Assets\walls.png %OUTPUT_FOLDER%\Data_Walls Data_wallTextures texture
%IMAGE_ENCODER% Assets\decorations.png %OUTPUT_FOLDER%\Data_Decorations Data_decorations sprite
%IMAGE_ENCODER% Assets\blockingDecorations.png %OUTPUT_FOLDER%\Data_BlockingDecorations Data_blockingDecorations sprite
%IMAGE_ENCODER% Assets\font.png %OUTPUT_FOLDER%\Data_Font Data_font font
%IMAGE_ENCODER% Assets\ui.png %OUTPUT_FOLDER%\Data_UI Data_uiSprite sprite
%IMAGE_ENCODER% Assets\title.png %OUTPUT_FOLDER%\Data_TitleBG Data_titleBG background
%IMAGE_ENCODER% Assets\help.png %OUTPUT_FOLDER%\Data_HelpBG Data_helpBG background
%IMAGE_ENCODER% Assets\win.png %OUTPUT_FOLDER%\Data_WinBG Data_winBG background
%IMAGE_ENCODER% Assets\floorComplete1.png %OUTPUT_FOLDER%\Data_FloorCompleteBG1 Data_floorCompleteBG1 background
%IMAGE_ENCODER% Assets\floorComplete2.png %OUTPUT_FOLDER%\Data_FloorCompleteBG2 Data_floorCompleteBG2 background

set AUDIO_ENCODER=Debug\AudioEncoder.exe
set AUD=Assets\RawAudio\audio

rem %AUDIO_ENCODER% %OUTPUT_FOLDER%\Data_Audio %AUD%04.raw %AUD%05.raw %AUD%06.raw %AUD%09.raw %AUD%10.raw %AUD%11.raw %AUD%12.raw %AUD%16.raw %AUD%18.raw %AUD%19.raw %AUD%21.raw %AUD%22.raw %AUD%23.raw %AUD%24.raw %AUD%25.raw %AUD%26.raw %AUD%30.raw %AUD%31.raw %AUD%32.raw %AUD%33.raw %AUD%34.raw %AUD%35.raw %AUD%36.raw %AUD%37.raw %AUD%38.raw %AUD%39.raw %AUD%41.raw %AUD%42.raw %AUD%43.raw %AUD%44.raw %AUD%45.raw %AUD%46.raw %AUD%49.raw %AUD%50.raw %AUD%51.raw %AUD%56.raw %AUD%58.raw %AUD%59.raw %AUD%60.raw %AUD%68.raw
%AUDIO_ENCODER% %OUTPUT_FOLDER%\Data_Audio %AUD%

python fxdata-build.py fxdata.txt
move fxdata.h Wolf\Generated\fxdata.h

pause
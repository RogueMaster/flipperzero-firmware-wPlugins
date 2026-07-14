@echo off

cd /d %~dp0

rem compile fxdata
python fxdata-build.py fxdata.txt

if %errorlevel%==0 goto postbuildfx

pause
exit /b 1

:postbuildfx

set dir=%temp%/arduboy_minigolf_build
arduino-cli.exe compile -v --log-level info ^
    -b arduboy-homemade:avr:arduboy-fx . ^
    --output-dir "%dir%" ^
    --build-property compiler.c.elf.extra_flags="-Wl,--relax" ^
    --build-property compiler.c.extra_flags="-mcall-prologues -DARDUGOLF_FX=1" ^
    --build-property compiler.cpp.extra_flags="{compiler.c.extra_flags}"

if %errorlevel%==0 goto postbuild

pause
exit /b 1

:postbuild

rem copy hex file to dir
echo F|xcopy /S /Q /Y /F "%dir%/arduboy_minigolf.ino.hex" "ardugolf_fx.hex" > nul

rem copy elf file to dir
echo F|xcopy /S /Q /Y /F "%dir%/arduboy_minigolf.ino.elf" "_elf_fx.elf" > nul
if NOT %errorlevel%==0 goto error

rem create _asm.txt file
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-objdump.exe" -S "%dir%/arduboy_minigolf.ino.elf" > _asm_fx.txt

rem create _sizes.txt
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-nm.exe" --size-sort -C -r -t d "%dir%/arduboy_minigolf.ino.elf" > _sizes_fx.txt

rem create _ram.txt
findstr /c:" b " /c:" B " /c:" d " /c:" D " _sizes_fx.txt > _ram_fx.txt

rem create _map.txt
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-nm.exe" --numeric-sort -C -t x "%dir%/arduboy_minigolf.ino.elf" > _map_fx2.txt
findstr /c:" b " /c:" B " /c:" d " /c:" D " _map_fx2.txt > _map_fx.txt
del _map_fx2.txt

rem create arduboy file
tar -a -cf ardugolf_fx.zip ardugolf_fx.hex fxdata-data.bin fxdata-save.bin info.json LICENSE.txt img/banner_700x192.png img/banner_128x64.png
move /y ardugolf_fx.zip ardugolf_fx.arduboy > nul

pause

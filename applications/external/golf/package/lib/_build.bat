@echo off

cd /d %~dp0

set dir=%temp%/arduboy_minigolf_build
arduino-cli.exe compile -v --log-level info ^
    -b arduboy-homemade:avr:arduboy . ^
    --output-dir "%dir%" ^
    --build-property compiler.c.elf.extra_flags="-Wl,--relax" ^
    --build-property compiler.c.extra_flags="-mcall-prologues -mstrict-X -fweb -frename-registers -DARDUGOLF_FX=0" ^
    --build-property compiler.cpp.extra_flags="{compiler.c.extra_flags}"

if %errorlevel%==0 goto postbuild

pause
exit /b 1

:postbuild

rem copy hex file to dir
echo F|xcopy /S /Q /Y /F "%dir%/arduboy_minigolf.ino.hex" "ardugolf.hex" > nul

rem copy elf file to dir
echo F|xcopy /S /Q /Y /F "%dir%/arduboy_minigolf.ino.elf" "_elf.elf" > nul
if NOT %errorlevel%==0 goto error

rem create _asm.txt file
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-objdump.exe" -S "%dir%/arduboy_minigolf.ino.elf" > _asm.txt

rem create _sizes.txt
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-nm.exe" --size-sort -C -r -t d "%dir%/arduboy_minigolf.ino.elf" > _sizes.txt

rem create _ram.txt
findstr /c:" b " /c:" B " /c:" d " /c:" D " _sizes.txt > _ram.txt

rem create _debug.txt
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-readelf.exe" -w "%dir%/arduboy_minigolf.ino.elf" > _debug.txt

rem create _map.txt
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avr-nm.exe" --numeric-sort -C -t x "%dir%/arduboy_minigolf.ino.elf" > _map2.txt
findstr /c:" b " /c:" B " /c:" d " /c:" D " _map2.txt > _map.txt
del _map2.txt

pause

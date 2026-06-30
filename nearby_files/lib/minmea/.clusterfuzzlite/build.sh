#!/bin/bash
$CC $CFLAGS -c minmea.c

llvm-ar rcs libfuzz.a *.o

$CC $CFLAGS $LIB_FUZZING_ENGINE $SRC/fuzzer.c -Wl,--whole-archive $SRC/minmea/libfuzz.a -Wl,--allow-multiple-definition -I$SRC/minmea/ -I$SRC/minmea/compat  -o $OUT/fuzzer

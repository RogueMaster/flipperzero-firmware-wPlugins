# FS class implementation

FS.h, FS.cpp, and FSImpl.h are included here to satisfy
ESPAsyncWebServer's dependency on the FS class for serving static
file.  Arduino-Emulator does not implement the FS class, which is a
ESP addition that is not part of the Arduino-Core API.  It is present
in some but not all Arduino implementations on various architectures.

The FS files herein are taken verbatim from the
earlephilhower/arduino-pico core, with the addition of one line in
FS.cpp to provide a null implementation of DEBUGV()

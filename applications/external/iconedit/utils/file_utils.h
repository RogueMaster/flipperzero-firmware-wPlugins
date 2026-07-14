#pragma once

#include "../icon.h"

// XBM converters
uint8_t* xbm_decode(const uint8_t* xbm, int32_t w, int32_t h);

// Returns icon as text string, binary files are in hex bytes (i.e. "013CFF...")
FuriString* c_file_generate(IEIcon* icon, bool current_frame_only);
FuriString* bmx_file_generate_frame(IEIcon* icon, size_t frame);
FuriString* png_file_generate_frame(IEIcon* icon, size_t frame);

// Saves current frame or all frames as .C source
bool c_file_save(IEIcon* icon, bool current_frame_only);

bool xbm_file_save(IEIcon* icon);
bool png_file_save(IEIcon* icon, bool current_frame_only);
bool bmx_file_save(IEIcon* icon);

IEIcon* png_file_open(const char* icon_path);
IEIcon* bmx_file_open(const char* icon_path);

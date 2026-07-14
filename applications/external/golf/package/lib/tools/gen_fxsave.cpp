#include <stdio.h>
#include <string.h>

#include "../game.hpp"

array<face, MAX_FACES> fs;
array<dvec2, MAX_VERTS> vs;
uint8_t fx_course;

int main(int argc, char** argv)
{
    if(argc < 2) return 1;
    FILE* f = fopen(argv[1], "w");
    if(!f) return 1;
    auto& d = savedata;
    memset(&d, 0xff, sizeof(course_save_data));
    memcpy(&d.ident[0], "ARDUGOLF", 8);
    for(auto& t : d.best_game) t = 0xff;
    for(auto& t : d.best_holes) t = 0xff;
    d.num_played = 0;
    d.checksum = checksum();
    for(int i = 0; i < 8192 / 64; ++i)
        fwrite(&d, sizeof(course_save_data), 1, f);
    fclose(f);
    return 0;
}

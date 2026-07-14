#include <stdio.h>
#include <stdint.h>

int main(int argc, char** argv)
{
    // args: <infile> <outfile> <symbol>
    if(argc < 4) return 1;
    
    FILE* fi = fopen(argv[1], "rb");
    if(!fi) return 1;
    FILE* fo = fopen(argv[2], "w");
    if(!fo)
    {
        fclose(fi);
        return 1;
    }
    
    fprintf(fo, "#pragma once\n\n");
    fprintf(fo, "static uint8_t %s[] =\n{\n    ", argv[3]);
    
    int n = 0;
    while(!feof(fi))
    {
        uint8_t d;
        fread(&d, 1, 1, fi);
        fprintf(fo, "%3d,%s", (int)d, n % 8 == 7 ? "\n    " : " ");
        ++n;
    }
    
    fprintf(fo, "\n};\n");
    
    fclose(fi);
    fclose(fo);
    
    return 0;
}

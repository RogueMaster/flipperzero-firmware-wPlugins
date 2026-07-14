#include <stdio.h>
#include <math.h>

#define FIXED_SHIFT 7
#define FIXED_ONE (1 << FIXED_SHIFT)
#define OUTPUT_FILE "TrigLUT.h"
#define M_PI 3.141592654

int main(int argc, char* argv[])
{
	FILE* fs = fopen(OUTPUT_FILE, "w");
	
	if(fs)
	{
		fprintf(fs, "uint8_t TrigLUT[] PROGMEM = {\n\t");
		for(int i = 0; i <= 64; i++)
		{
			int value = (int)((sin((double)i * M_PI / 128.0) * FIXED_ONE) + 0.5);
			fprintf(fs, "0x%x", value);
			if(i < 64)
			{
				fprintf(fs, ",");
			}
		}
		fprintf(fs, "\n};\n");
		fclose(fs);
	}
	
	return 0;
}

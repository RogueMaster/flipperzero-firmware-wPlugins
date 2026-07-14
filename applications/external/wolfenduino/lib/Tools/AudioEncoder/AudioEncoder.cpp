#include <stdio.h>
#include <stdint.h>
#include <vector>

using namespace std;

#define TONES_END 0x8000

#define NUM_NOTES 59
#define MUTE_NOTE 63

#define INSTRUMENT_COMMAND(x) (((x) << 6) | (1 << 2) | 1)
#define NOTE_COMMAND(note, duration) (((note) << 2) | ((duration) << 8))

float noteFrequencies[NUM_NOTES] = 
{
	116.5f,	// A#2
	123.5f,	// B
	130.8f,	// C3
	138.6f,	// C#
	146.8f,	// D
	155.6f,	// D#
	164.8f,	// E .
	174.6f,	// F
	185.0f,	// F#
	196.0f,	// G
	207.7f,	// G#
	220.0f,	// A
	233.1f,	// A#
	246.9f,	// B
	261.6f,	// C4 .
	277.2f,	
	293.7f,	
	311.1f,	
	329.6f,	
	349.2f,	
	370.0f,	
	392.0f,	
	415.3f,	
	440.0f,	
	466.2f,	
	493.9f,
	523.3f,	
	554.4f,	
	587.3f,	
	622.3f,	
	659.3f,	
	698.5f,	
	740.0f,	
	784.0f,	
	830.6f,	
	880.0f,	
	932.3f,	
	987.8f,
	1047,	
	1109,	
	1175,	
	1245,	
	1319,	
	1397,	
	1480,	
	1568,	
	1661,	
	1760,	
	1865,	
	1976,
	2093,	
	2217,	
	2349,	
	2489,	// D#7
	2637,	
	2794,	
	2960,	
	3136,	
	3322
};

struct AudioNote
{
	uint8_t note;
	bool noisy;
};

struct AudioPattern
{
	vector<uint16_t> data;
};

uint8_t getBestNote(float frequency)
{
	if(frequency == 0.0f)
		return MUTE_NOTE;

	uint8_t closest = MUTE_NOTE;
	float closestDistance = 0.0f;

	for(int n = 0; n < NUM_NOTES; n++)
	{
		float distance = frequency - noteFrequencies[n];
		if(distance < 0) 
			distance = -distance;

		if(closest == MUTE_NOTE || distance < closestDistance)
		{
			closest = n;
			closestDistance = distance;
		}
	}

	return closest;
}

AudioPattern processData(uint8_t* data, long length)
{
	const int stepSize = 5;
	int averageCount = 0;
	float averageFrequency = 0;
	float frequencyMin, frequencyMax;
	int stepCount = 0;
	vector<AudioNote> notes;

	for(int n = 0; n < length; n++)
	{
		float frequency = 1193181.0f / (data[n] * 60.0f);
		if(data[n] == 0) 
		{
			frequency = 0.0f;
		}
		else
		{
			if(averageCount == 0)
			{
				frequencyMin = frequencyMax = frequency;
			}
			else
			{
				if(frequency < frequencyMin)
				{
					frequencyMin = frequency;
				}
				if(frequency > frequencyMax)
				{
					frequencyMax = frequency;
				}
			}
			averageFrequency += frequency;
			averageCount++;
		}
		printf("%d[%d], ", (int)frequency, getBestNote(frequency));

		stepCount ++;
		if(stepCount == stepSize || n == length - 1)
		{
			AudioNote note;
			
			// If more than half of the frequencies are zero, then force to zero
			if(notes.size() > 0 && n < length - 1)
			{
				if(averageCount <= stepSize / 2)
				{
					averageCount = 0;
				}
			}

			if(averageCount == 0)
			{
				note.note = MUTE_NOTE;
				note.noisy = false;
			}
			else
			{
				averageFrequency /= averageCount;
				float range = frequencyMax - frequencyMin;
				note.note = getBestNote(averageFrequency);

				int noteRange = getBestNote(frequencyMax) - getBestNote(frequencyMin);

				note.noisy = noteRange > 4;
			}
			notes.push_back(note);

			stepCount = 0;
			averageCount = 0;
			averageFrequency = 0.0f;
			frequencyMax = 0.0f;
			frequencyMin = 0.0f;
		}
	}
	
	while(notes.size() > 0 && notes.back().note == MUTE_NOTE)
		notes.pop_back();

	vector<uint16_t> commandBuffer;
	bool isNoisy = notes[0].noisy;
	int currentDuration = 0;
	uint8_t currentNote = MUTE_NOTE;

	if(isNoisy)
	{
		commandBuffer.push_back(INSTRUMENT_COMMAND(1));
	}
	else
	{
		commandBuffer.push_back(INSTRUMENT_COMMAND(0));
	}

	printf("\nCommand instrument: %d\n", isNoisy ? 1 : 0);

	for(int n = 0; n < notes.size(); n++)
	{
		if((isNoisy != notes[n].noisy && notes[n].note != MUTE_NOTE) || currentNote != notes[n].note)
		{
			if(currentDuration > 0)
			{
				commandBuffer.push_back(NOTE_COMMAND(currentNote, currentDuration));
				printf("Command note: %d, %d\n", currentNote, currentDuration);

				currentDuration = 0;
			}
			if(isNoisy != notes[n].noisy && notes[n].note != MUTE_NOTE)
			{
				isNoisy = notes[n].noisy;
				if(isNoisy)
				{
					commandBuffer.push_back(INSTRUMENT_COMMAND(1));
				}
				else
				{
					commandBuffer.push_back(INSTRUMENT_COMMAND(0));
				}
				printf("Command instrument: %d\n", isNoisy ? 1 : 0);
			}
			currentNote = notes[n].note;
			currentDuration = 1;
		}
		else
		{
			currentDuration ++;
		}
	}

	if(currentDuration > 0)
	{
		commandBuffer.push_back(NOTE_COMMAND(currentNote, currentDuration));
		printf("Command note: %d, %d\n", currentNote, currentDuration);
	}
	
	commandBuffer.push_back(0);

	printf("Command buffer size: %d bytes\n", commandBuffer.size() * 2);
	
	AudioPattern pattern;
	pattern.data = commandBuffer;
	return pattern;
}

AudioPattern processDataTones(uint8_t* data, long length)
{
	AudioPattern pattern;

	for (int n = 0; n < length; n++)
	{
		uint16_t freq;
		if (data[n] == 0)
		{
			freq = 0;

			//printf("0 - > 0\n");
		}
		else
		{
			float frequency;
			frequency = 1193181.0f / (data[n] * 60.0f);
			freq = (uint16_t)frequency;
			//printf("%d - > %f -> %d\n", data[n], frequency, freq);
		}

		if (freq > 4000)
		{
			printf("Clipping frequency of %d Hz\n", freq);
			freq = 0;
		}

		if (pattern.data.size() == 0 || pattern.data[pattern.data.size() - 2] != freq)
		{
			pattern.data.push_back(freq);
			pattern.data.push_back(1);
		}
		else
		{
			pattern.data[pattern.data.size() - 1]++;
		}
	}

	for (int n = 1; n < pattern.data.size(); n += 2)
	{
		pattern.data[n] = (pattern.data[n] * 1024) / 140;
	}

	pattern.data.push_back(TONES_END);

	return pattern;
}


AudioPattern loadData(char* filename)
{
	FILE* fs = NULL;
	AudioPattern pattern;
	
	fopen_s(&fs, filename, "rb");

	if(!fs)
	{
		printf("Error opening %s\n", filename);
		return pattern;
	}

	long fileSize;

	fseek (fs , 0 , SEEK_END);
	fileSize = ftell (fs);
	rewind (fs);

	uint8_t* buffer = new uint8_t[fileSize];
	fread(buffer, 1, fileSize, fs);

	printf("Processing %s..\n", filename);

	pattern = processDataTones(buffer, fileSize);

	//
	//for (int n = 0; n < pattern.data.size() - 1; n += 2)
	//{
	//	printf("%d - %dms\n", pattern.data[n], pattern.data[n + 1]);
	//}
	//printf("--\n");

	delete[] buffer;

	fclose(fs);

	return pattern;
}

void writeData(char* filename, vector<AudioPattern>& patterns)
{
	FILE* fs = NULL;
	char path[100];
	snprintf(path, 100, "%s.h", filename);

	if(!fopen_s(&fs, path, "w"))
	{
		/*
		for(int p = 0; p < patterns.size(); p++)
		{
			fprintf(fs, "const uint16_t Data_Audio%02d[] PROGMEM = {\n\t", p);
			for(int n = 0; n < patterns[p].data.size(); n++)
			{
				fprintf(fs, "0x%04x", patterns[p].data[n]);
				
				if(n != patterns[p].data.size() - 1)
				{
					fprintf(fs, ",");
					
					if(n > 0 && (n % 20) == 0)
					{
						fprintf(fs, "\n\t");
					}
				}
			}
			fprintf(fs, "\n};\n\n");
		}
		*/

		fprintf(fs, "#define NUM_AUDIO_PATTERNS %d\n", patterns.size());
		fprintf(fs, "const uint16_t Data_AudioPatterns[] PROGMEM = {\n");
		long offset = 0;
		for(int p = 0; p < patterns.size(); p++)
		{
			//fprintf(fs, "\tData_Audio%02d,\n", p);
			fprintf(fs, "\t%d,\n", offset);
			offset += patterns[p].data.size() * 2;
		}
		fprintf(fs, "\t%d,\n", offset);

		fprintf(fs, "};\n\n");

		fclose(fs);
	}
	else
	{
		printf("Unable to open %s for write\n", path);
	}

	snprintf(path, 100, "%s.bin", filename);

	if (!fopen_s(&fs, path, "wb"))
	{
		for (int p = 0; p < patterns.size(); p++)
		{
			fwrite(patterns[p].data.data(), 2, patterns[p].data.size(), fs);
		}

		fclose(fs);
	}
	else
	{
		printf("Unable to open %s for write\n", path);
	}
}


	
int main(int argc, char* argv[])
{
	if(argc < 3)
	{
		printf("Usage:\n%s [output file] [input files]\n", argv[0]);
		return 0;
	}

	int numInputFiles = argc - 2;
	vector<AudioPattern> patterns;
	
	for(int n = 0; n < numInputFiles; n++)
	{
		AudioPattern pattern = loadData(argv[n + 2]);
		patterns.push_back(pattern);
	}

	if(numInputFiles == 1 && patterns[0].data.size() == 0)
	{
		patterns.clear();
		bool reading = true;
		int counter = 0;

		while(reading)
		{
			char filename[100];
			sprintf_s(filename, "%s%02d.raw", argv[2], counter);
			counter++;
			AudioPattern pattern = loadData(filename);
			if(pattern.data.size() > 0)
			{
				patterns.push_back(pattern);
			}
			else reading = false;
		}
	}
	
	writeData(argv[1], patterns);

	int longest = 0;
	for (int n = 0; n < patterns.size(); n++)
	{
		if (patterns[n].data.size() > longest)
		{
			longest = patterns[n].data.size();
		}
	}

	printf("Longest audio clip: %d bytes\n", longest);

	return 0;
}

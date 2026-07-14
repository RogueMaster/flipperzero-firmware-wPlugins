/*

Takes a raw output of the wolf3d map (64x64) and puts into a format that the game can read

*/

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <cassert>
#include "TileTypes.h"

#define MAP_SIZE 64
#define MAP_CHUNK_WIDTH 16
#define NUM_MAP_CHUNKS 16
#define MAP_CHUNKS_PER_MAP_WIDTH 4
#define RUN_LENGTH_BIT (1 << 7)

#define AREATILE 107
#define RLE_TOKEN 0xff
#define USE_RLE_TOKEN 0

using namespace std;

#if 0
struct HuffmanNode
{
	HuffmanNode* left;
	HuffmanNode* right;
	bool isLeaf;
	int value;
};

struct HuffmanEntry
{
	int value;
	int frequency;
	HuffmanNode* node;
};

HuffmanNode* huffmanTree = NULL;
vector<uint8_t> huffmanCodes[256];
int huffmanFrequency[256];

bool CompareHuffmanEntries(HuffmanEntry& a, HuffmanEntry& b)
{
	return a.frequency > b.frequency;
}

void TraverseHuffmanTree(HuffmanNode* node, vector<uint8_t>& code)
{
	if(node->left)
	{
		code.push_back(0);
		TraverseHuffmanTree(node->left, code);
		code.pop_back();
	}
	if(node->right)
	{
		code.push_back(1);
		TraverseHuffmanTree(node->right, code);
		code.pop_back();
	}
	if(node->isLeaf)
	{
		huffmanCodes[node->value] = code;
	}
}

void AddHuffmanFrequencies(vector<uint8_t> sampleData)
{
	for(int n = 0; n < sampleData.size(); n++)
	{
		huffmanFrequency[sampleData[n]]++;
	}
}

void GenerateHuffmanTree()
{
	vector<HuffmanEntry> entries;

/*	for(int n = 0; n < 256; n++)
	{
		frequency[n] = 0;
	}
	for(int n = 0; n < MAP_SIZE * MAP_SIZE; n++)
	{
		frequency[outlayer[n]] ++;
	}
	*/
	for(int n = 0; n < 256; n++)
	{
		if(huffmanFrequency[n] > 0)
		{
			HuffmanEntry entry;
			entry.value = n;
			entry.frequency = huffmanFrequency[n];
			entry.node = NULL;
			entries.push_back(entry);
		}
	}

	while(entries.size() > 1)
	{
		sort(entries.begin(), entries.end(), CompareHuffmanEntries);

		HuffmanEntry a = entries[entries.size() - 1];
		entries.pop_back();
		HuffmanNode* left = a.node;
		if(!left)
		{
			left = new HuffmanNode();
			left->left = NULL;
			left->right = NULL;
			left->isLeaf = true;
			left->value = a.value;
		}

		HuffmanEntry b = entries[entries.size() - 1];
		entries.pop_back();
		HuffmanNode* right = b.node;
		if(!right)
		{
			right = new HuffmanNode();
			right->left = NULL;
			right->right = NULL;
			right->isLeaf = true;
			right->value = b.value;
		}

		HuffmanNode* parent = new HuffmanNode();
		parent->isLeaf = false;
		parent->left = left;
		parent->right = right;

		HuffmanEntry newEntry;
		newEntry.frequency = a.frequency + b.frequency;
		newEntry.node = parent;
		newEntry.value = 0;

		entries.push_back(newEntry);
	}

	HuffmanEntry top = entries[0];
	huffmanTree = top.node;

	vector<uint8_t> code;
	TraverseHuffmanTree(huffmanTree, code);

	for(int n = 0; n < 256; n++)
	{
		if(huffmanCodes[n].size() > 0)
		{
			printf("%d: ", n);
			for(int i = 0; i < huffmanCodes[n].size(); i++)
			{
				printf("%d", huffmanCodes[n][i]);
			}
			printf("\n");
		}
	}
}

vector<uint8_t> CompressBlock(vector<uint8_t>& inputData)
{
	vector<uint8_t> compressed;

	for(int n = 0; n < inputData.size(); n++)
	{
		vector<uint8_t>& code = huffmanCodes[inputData[n]];
		assert(code.size() > 0);
		compressed.insert(compressed.end(), code.begin(), code.end());
	}

	while((compressed.size() % 8) > 0)
	{
		compressed.push_back(0);
	}

	return compressed;
}

void CompressMap()
{
	vector< vector<uint8_t> > rleCompressedChunks;
	vector<uint8_t> compressedMap;

	for(int chunkY = 0; chunkY < MAP_CHUNKS_PER_MAP_WIDTH; chunkY++)
	{
		for(int chunkX = 0; chunkX < MAP_CHUNKS_PER_MAP_WIDTH; chunkX++)
		{
			vector<uint8_t> chunkData;
			int lastData = 0;
			int runLength = 0;
			//outChunkOffsets[chunkY * MAP_CHUNKS_PER_MAP_WIDTH + chunkX] = outPtr - outData;

			for(int j = 0; j < MAP_CHUNK_WIDTH; j++)
			{
				for(int i = 0; i < MAP_CHUNK_WIDTH; i++)
				{
					int x = chunkX * MAP_CHUNKS_PER_MAP_WIDTH + i;
					int y = chunkY * MAP_CHUNKS_PER_MAP_WIDTH + j;
					int data = outlayer[y * MAP_SIZE + x];
					if(runLength > 0 && lastData == data)
					{
						runLength ++;
					}
					else
					{
						if(runLength > 1)
						{
#if USE_RLE_TOKEN
							chunkData.push_back(RLE_TOKEN);
							chunkData.push_back(lastData);
#else
							chunkData.push_back(lastData | RUN_LENGTH_BIT);
#endif
							chunkData.push_back(runLength);
						}
						else
						{
							chunkData.push_back(lastData);
						}
					
						lastData = data;
						runLength = 1;
					}
				}
			}

			// End of chunk
			if(runLength > 1)
			{
#if USE_RLE_TOKEN
				chunkData.push_back(RLE_TOKEN);
				chunkData.push_back(lastData);
#else
				chunkData.push_back(lastData | RUN_LENGTH_BIT);
#endif
				chunkData.push_back(runLength);
			}
			else
			{
				chunkData.push_back(lastData);
			}
			
			rleCompressedChunks.push_back(chunkData);
		}
	}

	for(int n = 0; n < rleCompressedChunks.size(); n++)
	{
		AddHuffmanFrequencies(rleCompressedChunks[n]);
	}

	GenerateHuffmanTree();

	for(int n = 0; n < rleCompressedChunks.size(); n++)
	{
		vector<uint8_t> compressed = CompressBlock(rleCompressedChunks[n]);

		compressedMap.insert(compressedMap.end(), compressed.begin(), compressed.end());
	}

	printf("Total data size: %d\n", compressedMap.size() / 8);
}
#endif

class Map
{
public:
	// input 
	uint8_t layer1[MAP_SIZE * MAP_SIZE];
	uint8_t layer2[MAP_SIZE * MAP_SIZE];

	// output
	uint8_t outlayer[MAP_SIZE * MAP_SIZE];

	uint8_t outData[MAP_SIZE * MAP_SIZE];
	uint8_t idData[MAP_SIZE * MAP_SIZE];

	int outChunkOffsets[NUM_MAP_CHUNKS];
	int outDataLength;

	int numSecrets;

	void ConvertTiles()
	{
		numSecrets = 0;
		for(int y = 0; y < MAP_SIZE; y++)
		{
			for(int x = 0; x < MAP_SIZE; x++)
			{
				uint8_t l1 = layer1[y * MAP_SIZE + x];
				uint8_t l2 = layer2[y * MAP_SIZE + x];
				uint8_t outTile = 0;
				if(l1 < AREATILE)
				{
					switch(l1)
					{
					case 90:
						outTile = Tile_Door_Generic_Vertical;
						break;
					case 92:
						outTile = Tile_Door_Locked1_Vertical;
						break;
					case 94:
						outTile = Tile_Door_Locked2_Vertical;
						break;
					case 96:
					case 98:
					case 100:
						outTile = Tile_Door_Elevator_Vertical;
						break;
					case 91:
						outTile = Tile_Door_Generic_Horizontal;
						break;
					case 93:
						outTile = Tile_Door_Locked1_Horizontal;
						break;
					case 95:
						outTile = Tile_Door_Locked2_Horizontal;
						break;
					case 97:
					case 99:
					case 101:
						outTile = Tile_Door_Elevator_Horizontal;
						break;
					case 21:
						if((y > 0 && (layer1[(y - 1) * MAP_SIZE + x] == 0 || layer1[(y - 1) * MAP_SIZE + x] >= AREATILE))
						|| (y < MAP_SIZE - 1 && (layer1[(y + 1) * MAP_SIZE + x] == 0 || layer1[(y + 1) * MAP_SIZE + x] >= AREATILE)))
							outTile = 22;
						else
							outTile = 21;
						break;
					case 25:	// Bit of a hack : change the purple + blood splat texture to just the purple texture
						outTile = 19;
						break;
					default:
						outTile = l1;
						break;
					}
				}

				if (l1 == 107)
				{
					// Secret exit marker tile
					outTile = Tile_SecretExit;
				}

				switch(l2)
				{
					// Player starts
				case 19:
				case 20:
				case 21:
				case 22:
					outTile = Tile_PlayerStart_North + l2 - 19;
					break;

				case 99:
					outTile = Tile_CastleExit;
					break;
				
					// Guard
				case 180:
				case 181:
				case 182:
				case 183:
				case 184:
				case 185:
				case 186:
				case 187:
					outTile = Tile_Actor_Guard_Hard;
					break;
				case 144:
				case 145:
				case 146:
				case 147:
				case 148:
				case 149:
				case 150:
				case 151:
					outTile = Tile_Actor_Guard_Medium;
					break;
				case 108:
				case 109:
				case 110:
				case 111:
				case 112:
				case 113:
				case 114:
				case 115:
					outTile = Tile_Actor_Guard_Easy;
					break;
				case 124:
					outTile = Tile_Decoration_DeadGuard;
					break;

					// SS
				case 198:
				case 199:
				case 200:
				case 201:
				case 202:
				case 203:
				case 204:
				case 205:
					outTile = Tile_Actor_SS_Hard;
					break;
				case 162:
				case 163:
				case 164:
				case 165:
				case 166:
				case 167:
				case 168:
				case 169:
					outTile = Tile_Actor_SS_Medium;
					break;
				case 126:
				case 127:
				case 128:
				case 129:
				case 130:
				case 131:
				case 132:
				case 133:
					outTile = Tile_Actor_SS_Easy;
					break;

					// Dog:
				case 206:
				case 207:
				case 208:
				case 209:
				case 210:
				case 211:
				case 212:
				case 213:
					outTile = Tile_Actor_Dog_Hard;
					break;
				case 170:
				case 171:
				case 172:
				case 173:
				case 174:
				case 175:
				case 176:
				case 177:
					outTile = Tile_Actor_Dog_Medium;
					break;
				case 134:
				case 135:
				case 136:
				case 137:
				case 138:
				case 139:
				case 140:
				case 141:
					outTile = Tile_Actor_Dog_Easy;
					break;

					// boss
				case 214:
					outTile = Tile_Actor_Boss;
					break;

					// static items:
				case 23:
					// puddle
					break;
				case 24:
					outTile = Tile_BlockingDecoration_Barrel;
					break;
				case 25:
					outTile = Tile_BlockingDecoration_TableChairs;
					break;
				case 26:
					outTile = Tile_BlockingDecoration_FloorLamp;
					break;
				case 27:
					outTile = Tile_Decoration_Chandelier;
					break;
				case 28:
					outTile = Tile_BlockingDecoration_HangingSkeleton;
					break;
				case 29:
					outTile = Tile_Item_BadFood;
					break;
				case 30:
					outTile = Tile_BlockingDecoration_Pillar;
					break;
				case 31:
					outTile = Tile_BlockingDecoration_Tree;
					break;
				case 32:
					outTile = Tile_Decoration_Skeleton;
					break;
				case 33:
					outTile = Tile_BlockingDecoration_Sink;
					break;
				case 34:
					outTile = Tile_BlockingDecoration_Plant;
					break;
				case 35:
					outTile = Tile_BlockingDecoration_Vase;
					break;
				case 36:
					outTile = Tile_BlockingDecoration_Table;
					break;
				case 37:
					outTile = Tile_Decoration_OverheadLamp;
					break;
				case 38:
					outTile = Tile_Decoration_KitchenStuff;
					break;
				case 39:
					outTile = Tile_BlockingDecoration_SuitOfArmour;
					break;
				case 40:
					// hanging cage
					break;
				case 41:
					// skeleton in cage
					break;
				case 42:
					// skeleton relaxed
					break;
				case 43:
					outTile = Tile_Item_Key1;
					break;
				case 44:
					outTile = Tile_Item_Key2;
					break;
				case 45:
					// bed (blocking)
					break;
				case 46:
					// pot (non blocking)
					break;
				case 47:
					outTile = Tile_Item_Food;
					break;
				case 48:
					outTile = Tile_Item_FirstAid;
					break;
				case 49:
					outTile = Tile_Item_Clip;
					break;
				case 50:
					outTile = Tile_Item_MachineGun;
					break;
				case 51:
					outTile = Tile_Item_ChainGun;
					break;
				case 52:
					outTile = Tile_Item_Cross;
					break;
				case 53:
					outTile = Tile_Item_Chalice;
					break;
				case 54:
					outTile = Tile_Item_Bible;
					break;
				case 55:
					outTile = Tile_Item_Crown;
					break;
				case 56:
					outTile = Tile_Item_1UP;
					break;
				case 57:
					outTile = Tile_BlockingDecoration_Barrel;
					break;
				case 58:
					//outTile = Tile_BlockingDecoration_Well;
					outTile = Tile_BlockingDecoration_Barrel;
					break;
				case 59:
					//outTile = Tile_BlockingDecoration_Well;
					outTile = Tile_BlockingDecoration_Barrel;
					break;
				case 60:
					// gibs?
					break;
				case 61:
					// flag
					break;
				case 98: // secret push wall
					idData[y * MAP_SIZE + x] = outTile;
					outTile = Tile_SecretPushWall;
					numSecrets++;
					break;
				}

				outlayer[y * MAP_SIZE + x] = outTile;
			}
		}

		printf("Num secrets: %d\n", numSecrets);
	}

	void GenerateIds()
	{
		uint8_t itemId = 0;
		uint8_t actorId = 0;

		for(int y = 0; y < MAP_SIZE; y++)
		{
			for(int x = 0; x < MAP_SIZE; x++)
			{
				uint8_t id = 0;
				uint8_t tile = outlayer[y * MAP_SIZE + x];
				if(tile >= Tile_FirstActor && tile <= Tile_LastActor)
				{
					id = actorId;
					actorId++;
				}
				if(tile >= Tile_FirstItem && tile <= Tile_LastItem)
				{
					id = itemId;
					itemId++;
				}

				if(tile == Tile_SecretPushWall)
				{
					id = idData[y * MAP_SIZE + x];
				}

				idData[y * MAP_SIZE + x] = id;
			}
		}
	}

	void Write(FILE* fs)
	{
		for(int y = 0; y < MAP_SIZE; y++)
		{
			for(int x = 0; x < MAP_SIZE; x++)
			{
				fwrite(&outlayer[y * MAP_SIZE + x], 1, 1, fs);
				fwrite(&idData[y * MAP_SIZE + x], 1, 1, fs);
			}
		}
		for(int x = 0; x < MAP_SIZE; x++)
		{
			for(int y = 0; y < MAP_SIZE; y++)
			{
				fwrite(&outlayer[y * MAP_SIZE + x], 1, 1, fs);
				fwrite(&idData[y * MAP_SIZE + x], 1, 1, fs);
			}
		}
	}
};

int main(int argc, char* argv[])
{
#if 0
	if(argc != 3)
	{
		printf("Usage: %s [input] [output]\n\n", argv[0]);
		return 0;
	}
	FILE* fs = fopen(argv[1], "rb");
	if(!fs)
	{
		printf("Error opening %s\n", argv[1]);
		return 0;
	}
#endif

	vector<Map> maps;

	for(int n = 0; n < 10; n++)
	{
		Map map;
		char filename[50];
		sprintf_s(filename, "Assets/RawMaps/rawmap%d.dat", n);
		FILE* fs;
		
		if (!fopen_s(&fs, filename, "rb"))
		{
			fread(map.layer1, 1, MAP_SIZE * MAP_SIZE, fs);
			fread(map.layer2, 1, MAP_SIZE * MAP_SIZE, fs);

			fclose(fs);

			map.ConvertTiles();
			map.GenerateIds();
			maps.push_back(map);
		}
		else
		{
			printf("Could not open %s!\n", filename);
		}
	}

	FILE* fs;
	char* outputPath = "Wolf/Generated/maps.bin";
	
	if (!fopen_s(&fs, outputPath, "wb"))
	{
		for (int n = 0; n < 10; n++)
		{
			maps[n].Write(fs);
		}

		fclose(fs);
	}
	else
	{
		printf("Could not open %s\n", outputPath);
	}

#if 0
	fs = fopen(argv[2], "w");
	if(!fs)
	{
		printf("Error opening %s\n", argv[2]);
		return 0;
	}

	fprintf(fs, "const uint8_t mapData[] PROGMEM = {\n\t");


	for(int y = 0; y < MAP_SIZE; y++)
	{
		for(int x = 0; x < MAP_SIZE; x++)
		{
			fprintf(fs, "0x%02x", outlayer[y * MAP_SIZE + x]);
			if(x < MAP_SIZE - 1 || y < MAP_SIZE - 1)
			{
				fprintf(fs, ",");
			}
		}
		if(y < MAP_SIZE - 1)
		{
			fprintf(fs, "\n\t");
		}
	}

	for(int chunkY = 0; chunkY < MAP_CHUNKS_PER_MAP_WIDTH; chunkY++)
	{
	}

	fprintf(fs, "\n};\n\n");

	fclose(fs);

	CompressMap();
#endif
	return 0;
}

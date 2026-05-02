#include "map.hpp"

#ifdef ENGINE_STORAGE_INCLUDE
#include ENGINE_STORAGE_INCLUDE
#endif

bool mapPackLoadFromFile(const char *filename, map_data_t *outMapData)
{
#if defined(ENGINE_STORAGE_INCLUDE) && defined(ENGINE_STORAGE_READ)
    const size_t expectedSize = sizeof(map_data_t);
    return ENGINE_STORAGE_READ(filename, outMapData, expectedSize) == expectedSize;
#else
    (void)filename;
    (void)outMapData;
    return false;
#endif
}

bool mapPackSaveToFile(const char *filename, const map_data_t *mapData)
{
#if defined(ENGINE_STORAGE_INCLUDE) && defined(ENGINE_STORAGE_WRITE)
    return ENGINE_STORAGE_WRITE(filename, mapData, sizeof(map_data_t));
#else
    (void)filename;
    (void)mapData;
    return false;
#endif
}
#include <stdbool.h>

typedef enum {
    FlipCheckersFileBoard,
    FlipCheckersFileOther,
} FlipCheckersFile;

bool flipcheckers_has_file(const FlipCheckersFile file_type, const char* file_name, const bool remove);
bool flipcheckers_load_file(char* contents, const FlipCheckersFile file_type, const char* file_name);
bool flipcheckers_save_file(
    const char* contents,
    const FlipCheckersFile file_type,
    const char* file_name,
    const bool append,
    const bool overwrite);

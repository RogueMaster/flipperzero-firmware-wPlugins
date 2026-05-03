#include "storage.h"

static char to_lower_manual(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + 32;
    }
    return c;
}

static bool storage_glob_match(const char *pattern, const char *str)
{
    while (*pattern)
    {
        if (*pattern == '*')
        {
            while (*pattern == '*')
                pattern++;
            if (*pattern == '\0')
                return true;
            while (*str)
            {
                if (storage_glob_match(pattern, str))
                    return true;
                str++;
            }
            return false;
        }
        if (*pattern == '?' || to_lower_manual(*pattern) == to_lower_manual(*str))
        {
            pattern++;
            str++;
        }
        else
        {
            return false;
        }
    }
    return *str == '\0';
}

uint16_t storage_file_list(const char *pattern, char **output, uint16_t offset, uint16_t max_files)
{
    Storage *storage = (Storage *)furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    uint16_t count = 0;
    uint16_t index = 0;

    // Split pattern into directory path and filename glob at the last '/'
    const char *last_slash = strrchr(pattern, '/');
    char dir_path[128];
    const char *file_pattern;

    if (last_slash != NULL)
    {
        size_t dir_len = last_slash - pattern;
        if (dir_len == 0)
        {
            dir_path[0] = '/';
            dir_path[1] = '\0';
        }
        else
        {
            snprintf(dir_path, sizeof(dir_path), "%.*s", (int)dir_len, pattern);
        }
        file_pattern = last_slash + 1;
    }
    else
    {
        dir_path[0] = '/';
        dir_path[1] = '\0';
        file_pattern = pattern;
    }

    if (storage_dir_open(file, dir_path))
    {
        FileInfo file_info;
        char name[128];
        char filename[128];
        while (storage_dir_read(file, &file_info, name, sizeof(name)))
        {
            if (storage_glob_match(file_pattern, name))
            {
                if (index >= offset && count < max_files)
                {
                    // get the filename only (after last '/')
                    const char *last_slash_in_name = strrchr(name, '/');
                    if (last_slash_in_name)
                    {
                        snprintf(filename, sizeof(filename), "%s", last_slash_in_name + 1);
                    }
                    else
                    {
                        snprintf(filename, sizeof(filename), "%s", name);
                    }
                    snprintf(output[count], 256, "%s", filename);
                    count++;
                }
                index++;
            }
        }
        storage_dir_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return count;
}

size_t storage_read(const char *file_path, void *buffer, size_t buffer_size)
{
    // ignore .wav (for now)
    if (strstr(file_path, ".wav") != nullptr)
    {
        return buffer_size;
    }

    Storage *storage = (Storage *)furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    // Open the file for reading
    if (!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return 0;
    }

    // Read data into the buffer
    size_t read_count = storage_file_read(file, buffer, buffer_size);
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E("storage", "Error reading from file.");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return 0;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return read_count;
}

bool storage_write(const char *file_path, const void *data, size_t data_size)
{
    Storage *storage = (Storage *)furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    // Open the file for writing
    if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Write data to the file
    size_t written_count = storage_file_write(file, data, data_size);
    if (storage_file_get_error(file) != FSE_OK)
    {
        FURI_LOG_E("storage", "Error writing to file.");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return written_count == data_size;
}
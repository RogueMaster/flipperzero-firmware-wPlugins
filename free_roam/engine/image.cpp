#include "image.hpp"

#include ENGINE_MEM_INCLUDE

#ifdef ENGINE_STORAGE_INCLUDE
#include ENGINE_STORAGE_INCLUDE
#endif

bool Image::getData(void *buffer, size_t buffer_size)
{
    if (this->data != nullptr)
    {
        // If data is already loaded in memory, copy it to the buffer
        size_t data_size = size.x * size.y * (is_8bit ? 1 : 2); // Calculate data size based on color depth
        if (data_size > buffer_size)
        {
            return false; // Buffer too small
        }
        memcpy(buffer, this->data, data_size);
        return true;
    }

    if (path == nullptr || path[0] == '\0')
        return false;
#ifdef ENGINE_STORAGE_READ
    size_t bytes_read = ENGINE_STORAGE_READ(this->path, buffer, buffer_size);
    return bytes_read > 0;
#else
    return false;
#endif
}

void Image::render(Draw *draw, const Vector &position)
{
    if (draw != nullptr)
    {
        if (is_8bit)
        {
            uint8_t *data_buffer = (uint8_t *)ENGINE_MEM_MALLOC(size.x * size.y);
            if (!data_buffer)
            {
                return;
            }
            if (!getData(data_buffer, size.x * size.y))
            {
                ENGINE_MEM_FREE(data_buffer);
                return;
            }
            draw->image(position, data_buffer, size);
            ENGINE_MEM_FREE(data_buffer);
        }
        else
        {
            uint16_t *data_buffer = (uint16_t *)ENGINE_MEM_MALLOC(size.x * size.y * 2);
            if (!data_buffer)
            {
                return;
            }
            if (!getData(data_buffer, size.x * size.y * 2))
            {
                ENGINE_MEM_FREE(data_buffer);
                return;
            }
            draw->image(position, data_buffer, size);
            ENGINE_MEM_FREE(data_buffer);
        }
    }
}

void Image::render(Draw *draw, int16_t x, int16_t y)
{
    if (draw != nullptr)
    {
        if (is_8bit)
        {
            uint8_t *data_buffer = (uint8_t *)ENGINE_MEM_MALLOC(size.x * size.y);
            if (!data_buffer)
            {
                return;
            }
            if (!getData(data_buffer, size.x * size.y))
            {
                ENGINE_MEM_FREE(data_buffer);
                return;
            }
            draw->image(x, y, data_buffer, size.x, size.y);
            ENGINE_MEM_FREE(data_buffer);
        }
        else
        {
            uint16_t *data_buffer = (uint16_t *)ENGINE_MEM_MALLOC(size.x * size.y * 2);
            if (!data_buffer)
            {
                return;
            }
            if (!getData(data_buffer, size.x * size.y * 2))
            {
                ENGINE_MEM_FREE(data_buffer);
                return;
            }
            draw->image(x, y, data_buffer, size.x, size.y);
            ENGINE_MEM_FREE(data_buffer);
        }
    }
}
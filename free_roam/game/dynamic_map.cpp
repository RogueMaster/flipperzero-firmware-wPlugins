#include "dynamic_map.hpp"
#include "engine/vector.hpp"

DynamicMap::DynamicMap(const char *name, uint8_t w, uint8_t h, bool addBorder) : width(w), height(h), name(name), renderWallCount(0)
{
    memset(tiles, 0, sizeof(tiles));
    memset(renderWalls, 0, sizeof(renderWalls));
    if (addBorder)
    {
        this->addBorderWalls();
    }
}

DynamicMap::~DynamicMap()
{
    for (uint8_t i = 0; i < renderWallCount; i++)
    {
        if (renderWalls[i] != nullptr)
        {
            delete renderWalls[i];
            renderWalls[i] = nullptr;
        }
    }
}

void DynamicMap::addBorderWalls(float height, float depth)
{
    // Add walls around the entire map border
    addHorizontalWall(0, width - 1, 0, height, depth, TILE_WALL);          // Top border
    addHorizontalWall(0, width - 1, height - 1, height, depth, TILE_WALL); // Bottom border
    addVerticalWall(0, 0, height - 1, height, depth, TILE_WALL);           // Left border
    addVerticalWall(width - 1, 0, height - 1, height, depth, TILE_WALL);   // Right border
}

void DynamicMap::addCorridor(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    // Simple L-shaped corridor
    if (x1 == x2)
    {
        // Vertical corridor
        uint8_t start_y = (y1 < y2) ? y1 : y2;
        uint8_t end_y = (y1 < y2) ? y2 : y1;
        for (uint8_t y = start_y; y <= end_y; y++)
        {
            setTile(x1, y, TILE_EMPTY);
        }
    }
    else if (y1 == y2)
    {
        // Horizontal corridor
        uint8_t start_x = (x1 < x2) ? x1 : x2;
        uint8_t end_x = (x1 < x2) ? x2 : x1;
        for (uint8_t x = start_x; x <= end_x; x++)
        {
            setTile(x, y1, TILE_EMPTY);
        }
    }
    else
    {
        // L-shaped corridor (horizontal then vertical)
        uint8_t start_x = (x1 < x2) ? x1 : x2;
        uint8_t end_x = (x1 < x2) ? x2 : x1;
        for (uint8_t x = start_x; x <= end_x; x++)
        {
            setTile(x, y1, TILE_EMPTY);
        }

        uint8_t start_y = (y1 < y2) ? y1 : y2;
        uint8_t end_y = (y1 < y2) ? y2 : y1;
        for (uint8_t y = start_y; y <= end_y; y++)
        {
            setTile(x2, y, TILE_EMPTY);
        }
    }
}

void DynamicMap::addDoor(uint8_t x, uint8_t y)
{
    setTile(x, y, TILE_DOOR);
}

void DynamicMap::addHorizontalWall(uint8_t x1, uint8_t x2, uint8_t y, float height, float depth, TileType type)
{
    for (uint8_t x = x1; x <= x2; x++)
    {
        setTile(x, y, type);
    }
    // Create Sprite3D wall for rendering
    if (type == TILE_WALL && renderWallCount < MAX_RENDER_WALLS)
    {
        float len = (float)(x2 - x1 + 1);
        renderWalls[renderWallCount] = new Sprite3D();
        renderWalls[renderWallCount]->setPosition(Vector((float)x1 + len * 0.5f, (float)y + 0.5f));
        renderWalls[renderWallCount]->setRotation(0.0f);
        renderWalls[renderWallCount]->createWall(0, 0.75f, 0, len, height, depth);
        renderWalls[renderWallCount]->setActive(true);
        renderWallCount++;
    }
}

void DynamicMap::addRoom(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, float height, float depth, bool add_walls)
{
    // Clear the room area
    for (uint8_t y = y1; y <= y2; y++)
    {
        for (uint8_t x = x1; x <= x2; x++)
        {
            setTile(x, y, TILE_EMPTY);
        }
    }

    // Add walls around the room
    if (add_walls)
    {
        addHorizontalWall(x1, x2, y1, height, depth, TILE_WALL); // Top wall
        addHorizontalWall(x1, x2, y2, height, depth, TILE_WALL); // Bottom wall
        addVerticalWall(x1, y1, y2, height, depth, TILE_WALL);   // Left wall
        addVerticalWall(x2, y1, y2, height, depth, TILE_WALL);   // Right wall
    }
}

void DynamicMap::addVerticalWall(uint8_t x, uint8_t y1, uint8_t y2, float height, float depth, TileType type)
{
    for (uint8_t y = y1; y <= y2; y++)
    {
        setTile(x, y, type);
    }
    // Create Sprite3D wall for rendering
    if (type == TILE_WALL && renderWallCount < MAX_RENDER_WALLS)
    {
        float len = (float)(y2 - y1 + 1);
        renderWalls[renderWallCount] = new Sprite3D();
        renderWalls[renderWallCount]->setPosition(Vector((float)x + 0.5f, (float)y1 + len * 0.5f));
        renderWalls[renderWallCount]->setRotation((float)(M_PI / 2.0));
        renderWalls[renderWallCount]->createWall(0, 0.75f, 0, len, height, depth);
        renderWalls[renderWallCount]->setActive(true);
        renderWallCount++;
    }
}

uint8_t DynamicMap::releaseRenderWalls(Sprite3D **out, uint8_t maxCount)
{
    uint8_t count = (renderWallCount < maxCount) ? renderWallCount : maxCount;
    for (uint8_t i = 0; i < count; i++)
    {
        out[i] = renderWalls[i];
        renderWalls[i] = nullptr; // ownership transferred – destructor will skip these
    }
    renderWallCount = 0;
    return count;
}

uint8_t DynamicMap::getBlockAt(uint8_t x, uint8_t y) const
{
    // Make sure we're checking within bounds of our actual map
    if (x >= width || y >= height)
    {
        return 0x0; // Out of bounds is always empty
    }

    TileType tile = tiles[y][x];
    switch (tile)
    {
    case TILE_WALL:
    case TILE_DOOR:
        return 0xF;
    default:
        return 0x0;
    }
}

void DynamicMap::getMiniMap(uint8_t output[MAX_MAP_HEIGHT][MAX_MAP_WIDTH]) const
{
    for (uint8_t y = 0; y < MAX_MAP_HEIGHT; y++)
    {
        for (uint8_t x = 0; x < MAX_MAP_WIDTH; x++)
        {
            if (x < width && y < height)
                output[y][x] = (uint8_t)tiles[y][x];
            else
                output[y][x] = 0;
        }
    }
}

TileType DynamicMap::getTile(uint8_t x, uint8_t y) const
{
    if (x >= width || y >= height)
    {
        return TILE_EMPTY; // Out of bounds = empty (no walls)
    }
    return tiles[y][x];
}

void DynamicMap::renderMiniMap(Draw *const canvas, Vector position, Vector size,
                               Vector player_pos, Vector player_dir, uint16_t foreground_color, uint16_t background_color) const
{
    if (!canvas || size.x == 0 || size.y == 0)
        return;

    // Background
    canvas->fillRectangle(position, size, background_color);
    canvas->rectangle(position, size, foreground_color);

    // Scale factors: pixels per map tile
    float scale_x = (float)size.x / width;
    float scale_y = (float)size.y / height;

    Vector _pos = Vector(scale_x, scale_y);
    Vector _size = Vector(scale_x, scale_y);
    for (uint8_t ty = 0; ty < height; ty++)
    {
        for (uint8_t tx = 0; tx < width; tx++)
        {
            TileType tile = tiles[ty][tx];
            if (tile == TILE_EMPTY)
                continue;

            _pos.x = (uint8_t)(position.x + tx * scale_x);
            _pos.y = (uint8_t)(position.y + ty * scale_y);
            _size.x = (uint8_t)(scale_x + 0.5f);
            _size.y = (uint8_t)(scale_y + 0.5f);
            if (_size.x < 1)
                _size.x = 1;
            if (_size.y < 1)
                _size.y = 1;

            canvas->fillRectangle(_pos, _size, foreground_color);
        }
    }

    // Draw player dot + direction arrow
    if (player_pos.x >= 0 && player_pos.y >= 0)
    {
        int16_t ppx = (int16_t)(position.x + player_pos.x * scale_x);
        int16_t ppy = (int16_t)(position.y + player_pos.y * scale_y);

        // 3x3 white square so the dot is visible over walls
        _pos.x = ppx - 1;
        _pos.y = ppy - 1;
        _size.x = 3;
        _size.y = 3;
        canvas->fillRectangle(_pos, _size, background_color);

        // Direction arrow: line from centre out 4px in facing direction
        if (player_dir.x != 0.0f || player_dir.y != 0.0f)
        {
            int16_t tip_x = ppx + (int16_t)(player_dir.x * 4.0f);
            int16_t tip_y = ppy + (int16_t)(player_dir.y * 4.0f);
            _pos.x = ppx;
            _pos.y = ppy;
            _size.x = tip_x;
            _size.y = tip_y;
            canvas->line(_pos, _size, foreground_color);
            // Arrowhead: two lines from tip back to flanking points
            int16_t base_x = tip_x - (int16_t)(player_dir.x * 2.0f);
            int16_t base_y = tip_y - (int16_t)(player_dir.y * 2.0f);
            int16_t perp_x = (int16_t)(player_dir.y * 2.0f);
            int16_t perp_y = (int16_t)(-player_dir.x * 2.0f);
            _pos.x = tip_x;
            _pos.y = tip_y;
            _size.x = base_x + perp_x;
            _size.y = base_y + perp_y;
            canvas->line(_pos, _size, foreground_color);
            _size.x = base_x - perp_x;
            _size.y = base_y - perp_y;
            canvas->line(_pos, _size, foreground_color);
        }

        // Centre dot on top
        _pos.x = ppx;
        _pos.y = ppy;
        canvas->pixel(_pos, foreground_color);
    }
}

void DynamicMap::setTile(uint8_t x, uint8_t y, TileType type)
{
    if (x < width && y < height)
    {
        tiles[y][x] = type;
    }
}
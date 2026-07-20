#include <ChunkPrint.h>

ChunkPrint::ChunkPrint(uint8_t* destination, size_t from, size_t len)
    : _destination(destination), _to_skip(from), _to_write(len), _pos{0} {}

size_t ChunkPrint::write(uint8_t c) {
  if (_to_skip > 0) {
    _to_skip--;
    return 1;
  } else if (_to_write > 0) {
    _to_write--;
    _destination[_pos++] = c;
    return 1;
  }
  return 0;
}
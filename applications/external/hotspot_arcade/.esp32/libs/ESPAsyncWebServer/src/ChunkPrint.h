#ifndef CHUNKPRINT_H
#define CHUNKPRINT_H

#include <Print.h>

class ChunkPrint : public Print {
  private:
    uint8_t* _destination;
    size_t _to_skip;
    size_t _to_write;
    size_t _pos;

  public:
    ChunkPrint(uint8_t* destination, size_t from, size_t len);
    size_t write(uint8_t c);
    size_t write(const uint8_t* buffer, size_t size) { return this->Print::write(buffer, size); }
};
#endif

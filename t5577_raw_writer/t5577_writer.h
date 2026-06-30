#define T5577_WRITER_FILE_EXTENSION ".t5577"

void uint32_to_byte_buffer(uint32_t block_data, uint8_t byte_buffer[4]) {
    byte_buffer[0] = (block_data >> 24) & 0xFF;
    byte_buffer[1] = (block_data >> 16) & 0xFF;
    byte_buffer[2] = (block_data >> 8) & 0xFF;
    byte_buffer[3] = block_data & 0xFF;
}

uint32_t byte_buffer_to_uint32(uint8_t byte_buffer[4]) {
    uint32_t block_data = 0;
    block_data |= ((uint32_t)byte_buffer[0] << 24);
    block_data |= ((uint32_t)byte_buffer[1] << 16);
    block_data |= ((uint32_t)byte_buffer[2] << 8);
    block_data |= ((uint32_t)byte_buffer[3]);
    return block_data;
}

#include "bitstream.h"

// bitwriter allows us to write individual bits to a byte stream.
// this is useful for packing boolean flags (match vs literal) efficiently.

void BitWriter::WriteBit(bool bit) {
    // if the bit is 1, we set the corresponding bit in the current byte.
    // we fill from most significant bit (7) down to 0.
    if (bit) {
        current_byte |= (1 << (7 - bit_count));
    }
    bit_count++;
    
    // if the byte is full (8 bits), we push it to the buffer and start a new one.
    if (bit_count == 8) {
        buffer.push_back(current_byte);
        current_byte = 0;
        bit_count = 0;
    }
}

void BitWriter::WriteBits(uint64_t value, int num_bits) {
    for (int i = num_bits - 1; i >= 0; --i) {
        WriteBit((value >> i) & 1);
    }
}

void BitWriter::Flush() {
    // if there are leftover bits in the current byte, we push it now.
    // the remaining bits will be 0 (padding).
    if (bit_count > 0) {
        buffer.push_back(current_byte);
        current_byte = 0;
        bit_count = 0;
    }
}

const std::vector<uint8_t>& BitWriter::GetData() const {
    return buffer;
}

BitReader::BitReader(const std::vector<uint8_t>& data) : data(data) {}

bool BitReader::ReadBit() {
    if (byte_index >= data.size()) return 0; // eof
    
    // extract the bit at the current position.
    bool bit = (data[byte_index] >> (7 - bit_index)) & 1;
    bit_index++;
    
    // move to next byte if we've read all 8 bits.
    if (bit_index == 8) {
        byte_index++;
        bit_index = 0;
    }
    return bit;
}

uint64_t BitReader::ReadBits(int num_bits) {
    uint64_t value = 0;
    for (int i = 0; i < num_bits; ++i) {
        value = (value << 1) | ReadBit();
    }
    return value;
}

#pragma once
#include <vector>
#include <cstdint>
#include <string>

class BitWriter {
public:
    void WriteBit(bool bit);
    void WriteBits(uint64_t value, int num_bits);
    void Flush();
    const std::vector<uint8_t>& GetData() const;

private:
    std::vector<uint8_t> buffer;
    uint8_t current_byte = 0;
    int bit_count = 0;
};

class BitReader {
public:
    BitReader(const std::vector<uint8_t>& data);
    bool ReadBit();
    uint64_t ReadBits(int num_bits);

private:
    const std::vector<uint8_t>& data;
    size_t byte_index = 0;
    int bit_index = 0;
};

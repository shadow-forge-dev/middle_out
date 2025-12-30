#pragma once
#include <vector>
#include <cstdint>

// rANS Encoder/Decoder
// This will implement a static probability model rANS for simplicity first.

class RansEncoder {
public:
    void Init();
    void BuildModel(const std::vector<uint8_t>& data); // Added
    void Encode(uint8_t symbol);
    void Flush();
    std::vector<uint8_t> GetOutput() const;
    std::vector<uint8_t> GetModelData() const;
};

// Helper function declaration
void BuildRansModel(const std::vector<uint8_t>& data);


class RansDecoder {
public:
    void Init(const std::vector<uint8_t>& data);
    void SetModel(const std::vector<uint8_t>& model_data); // Added
    uint8_t Decode();
};

#pragma once
#include <vector>
#include <cstdint>

// Constructs the Suffix Array (SA) for the given input data.
// SA[i] is the starting index of the i-th lexicographically smallest suffix.
std::vector<int> ConstructSuffixArray(const std::vector<uint8_t>& data);

// Constructs the Longest Common Prefix (LCP) array.
// LCP[i] is the length of the longest common prefix between suffix SA[i-1] and SA[i].
std::vector<int> ConstructLCPArray(const std::vector<uint8_t>& data, const std::vector<int>& sa);

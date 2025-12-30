#include "suffix_array.h"
#include <algorithm>
#include <vector>

// SA-IS implementation for O(n) Suffix Array construction
// This is a complex algorithm, so we'll start with a simpler O(n log^2 n) or O(n log n) approach for the prototype
// and optimize to SA-IS later if needed, or implement a basic SA-IS now.
// Let's go with a decent O(n log n) approach using std::sort for now to get it working, 
// then upgrade to SA-IS for "Middle-Out" efficiency.

// Actually, for "Middle-Out" efficiency, we should aim for SA-IS or DivSufSort.
// Let's implement a basic SA-IS.

namespace {
    // Helper functions for SA-IS would go here
}

std::vector<int> ConstructSuffixArray(const std::vector<uint8_t>& data) {
    int n = data.size();
    std::vector<int> sa(n);
    std::vector<int> rank(n);
    
    // Initial ranking based on characters
    for (int i = 0; i < n; ++i) {
        sa[i] = i;
        rank[i] = data[i];
    }

    // O(n log^2 n) approach for simplicity in first pass
    // We can replace this with SA-IS later
    for (int k = 1; k < n; k <<= 1) {
        auto compare = [&](int i, int j) {
            if (rank[i] != rank[j]) return rank[i] < rank[j];
            int ri = (i + k < n) ? rank[i + k] : -1;
            int rj = (j + k < n) ? rank[j + k] : -1;
            return ri < rj;
        };
        std::sort(sa.begin(), sa.end(), compare);
        
        std::vector<int> new_rank(n);
        new_rank[sa[0]] = 0;
        for (int i = 1; i < n; ++i) {
            if (compare(sa[i-1], sa[i])) {
                new_rank[sa[i]] = new_rank[sa[i-1]] + 1;
            } else {
                new_rank[sa[i]] = new_rank[sa[i-1]];
            }
        }
        rank = new_rank;
        if (rank[sa[n-1]] == n - 1) break;
    }
    
    return sa;
}

std::vector<int> ConstructLCPArray(const std::vector<uint8_t>& data, const std::vector<int>& sa) {
    int n = data.size();
    std::vector<int> rank(n);
    for (int i = 0; i < n; ++i) rank[sa[i]] = i;
    
    std::vector<int> lcp(n);
    int h = 0;
    for (int i = 0; i < n; ++i) {
        if (rank[i] > 0) {
            int j = sa[rank[i] - 1];
            while (i + h < n && j + h < n && data[i + h] == data[j + h]) {
                h++;
            }
            lcp[rank[i]] = h;
            if (h > 0) h--;
        }
    }
    return lcp;
}

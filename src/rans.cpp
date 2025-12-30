#include "rans.h"
#include <algorithm>
#include <stdexcept>

// Constants for rANS
constexpr uint32_t PROB_BITS = 12; // 12-bit precision for probabilities
constexpr uint32_t PROB_SCALE = 1 << PROB_BITS;
constexpr uint32_t RANS_L = 1 << 16; // Lower bound for renormalization

struct SymbolStats {
    uint32_t freqs[256];
    uint32_t cum_freqs[257];

    void Count(const std::vector<uint8_t>& data) {
        std::fill(std::begin(freqs), std::end(freqs), 0);
        for (uint8_t b : data) freqs[b]++;
        
        // Normalize to PROB_SCALE
        uint64_t total = data.size();
        if (total == 0) return;

        uint32_t current_total = 0;
        for (int i = 0; i < 256; ++i) {
            if (freqs[i] > 0) {
                // Ensure at least 1 count if it exists
                uint64_t scaled = (uint64_t)freqs[i] * PROB_SCALE / total;
                if (scaled == 0) scaled = 1;
                freqs[i] = scaled;
            }
            current_total += freqs[i];
        }

        // Adjust to sum exactly to PROB_SCALE
        if (current_total < PROB_SCALE) {
            freqs[0] += PROB_SCALE - current_total;
        } else if (current_total > PROB_SCALE) {
            // Naive adjustment, just subtract from largest or first
            // For robustness, this should be better, but for now:
            int i = 0;
            while (current_total > PROB_SCALE) {
                if (freqs[i] > 1) {
                    freqs[i]--;
                    current_total--;
                }
                i = (i + 1) % 256;
            }
        }

        cum_freqs[0] = 0;
        for (int i = 0; i < 256; ++i) {
            cum_freqs[i+1] = cum_freqs[i] + freqs[i];
        }
    }
};

class RansEncoderImpl {
public:
    uint32_t state = RANS_L;
    std::vector<uint8_t> buffer;
    SymbolStats stats;

    void Init() {
        state = RANS_L;
        buffer.clear();
    }

    void BuildModel(const std::vector<uint8_t>& data) {
        stats.Count(data);
    }

    void Encode(uint8_t symbol) {
        uint32_t freq = stats.freqs[symbol];
        uint32_t start = stats.cum_freqs[symbol];
        
        // renormalization:
        // rans works by holding the entire state of the stream in a single integer 'state'.
        // as we encode symbols, this state grows.
        // if it grows too large (overflows), we need to shrink it.
        // we do this by writing the lower bits to the output stream.
        // this keeps the state within a manageable range (between L and H).
        while (state >= (1u << (31 - PROB_BITS)) * freq) {
            buffer.push_back(state & 0xFF);
            state >>= 8;
        }
        
        // state update:
        // this is the magic formula of rans.
        // x' = floor(x / freq) * scale + (x % freq) + start
        // it effectively "pushes" the symbol onto the state stack, weighted by its probability.
        // symbols with higher frequency increase the state less (better compression).
        state = ((state / freq) << PROB_BITS) + (state % freq) + start;
    }

    void Flush() {
        buffer.push_back(state & 0xFF);
        buffer.push_back((state >> 8) & 0xFF);
        buffer.push_back((state >> 16) & 0xFF);
        buffer.push_back((state >> 24) & 0xFF);
    }
};

// Pimpl wrappers
static RansEncoderImpl enc_impl;
static SymbolStats shared_stats;

void RansEncoder::Init() {
    enc_impl.Init();
}

// we need to build the model before we can encode anything
// this sets up the frequency tables
void RansEncoder::BuildModel(const std::vector<uint8_t>& data) {
    enc_impl.BuildModel(data);
}

void RansEncoder::Encode(uint8_t symbol) {
    enc_impl.Encode(symbol);
}

void RansEncoder::Flush() {
    enc_impl.Flush();
}

std::vector<uint8_t> RansEncoder::GetOutput() const {
    // we return the raw buffer which contains the encoded data
    return enc_impl.buffer;
}

std::vector<uint8_t> RansEncoder::GetModelData() const {
    std::vector<uint8_t> model_data;
    for (int i = 0; i < 256; ++i) {
        uint32_t f = enc_impl.stats.freqs[i];
        model_data.push_back(f & 0xFF);
        model_data.push_back((f >> 8) & 0xFF);
    }
    return model_data;
}

class RansDecoderImpl {
public:
    uint32_t state;
    const std::vector<uint8_t>& data;
    size_t ptr;
    SymbolStats stats;

    RansDecoderImpl(const std::vector<uint8_t>& d) : data(d) {
        ptr = d.size();
        state = RANS_L; 
        if (ptr >= 4) {
            ptr -= 4;
            state = data[ptr] | (data[ptr+1] << 8) | (data[ptr+2] << 16) | (data[ptr+3] << 24);
        }
    }

    void Init(const std::vector<uint8_t>& model_data) {
        if (model_data.size() < 512) return;
        for (int i = 0; i < 256; ++i) {
            uint32_t f = model_data[2*i] | (model_data[2*i+1] << 8);
            stats.freqs[i] = f;
        }
        stats.cum_freqs[0] = 0;
        for (int i = 0; i < 256; ++i) {
            stats.cum_freqs[i+1] = stats.cum_freqs[i] + stats.freqs[i];
        }
    }

    uint8_t Decode() {
        // decoding is the reverse of encoding.
        // we start with the final state and "pop" symbols off it.

        // step 1: find the symbol.
        // the lower bits of the state tell us which "slot" in the probability table the symbol occupies.
        uint32_t slot = state & (PROB_SCALE - 1);
        
        // we search the cumulative frequency table to find which symbol owns this slot.
        uint8_t symbol = 0;
        for (int i = 0; i < 256; ++i) {
            if (slot < stats.cum_freqs[i+1]) {
                symbol = i;
                break;
            }
        }

        uint32_t freq = stats.freqs[symbol];
        uint32_t start = stats.cum_freqs[symbol];

        // step 2: update state.
        // we remove the symbol from the state, reversing the encoding formula.
        state = (state >> PROB_BITS) * freq + slot - start;

        // step 3: renormalize.
        // if the state is too small (below our lower bound L), we need to read more bits from the stream.
        // this refills the state so we can decode more symbols.
        while (state < RANS_L && ptr > 0) {
            state = (state << 8) | data[--ptr];
        }

        return symbol;
    }
};

static RansDecoderImpl* dec_impl = nullptr;

void RansDecoder::Init(const std::vector<uint8_t>& data) {
    // This signature is now used for passing the compressed stream
    // We need a separate way to pass the model, or pack it.
    // Let's assume 'data' contains ONLY the compressed stream for now,
    // and we add a method SetModel.
    if (dec_impl) delete dec_impl;
    dec_impl = new RansDecoderImpl(data);
}

void RansDecoder::SetModel(const std::vector<uint8_t>& model_data) {
    if (dec_impl) dec_impl->Init(model_data);
}

uint8_t RansDecoder::Decode() {
    return dec_impl->Decode();
}

void BuildRansModel(const std::vector<uint8_t>& data) {
    enc_impl.BuildModel(data);
}

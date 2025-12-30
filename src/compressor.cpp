#include "compressor.h"
#include <iostream>
#include <fstream>
#include <vector>
#include "suffix_array.h"
#include "rans.h"
#include "bitstream.h"

#include <chrono>
#include <cmath>
#include <iomanip>

// ... (includes)

struct Match {
    int distance;
    int length;
};

Match FindLongestMatch(const std::vector<uint8_t>& data, int pos, int window_size) {
    int max_len = 0;
    int best_dist = 0;
    // we limit match length to 255 to fit in a single byte
    int limit = std::min((int)data.size(), pos + 255);
    int start_search = std::max(0, pos - window_size);

    for (int i = start_search; i < pos; ++i) {
        int len = 0;
        while (pos + len < limit && i + len < pos && data[i + len] == data[pos + len]) {
            len++;
        }
        if (len > max_len) {
            max_len = len;
            best_dist = pos - i;
        }
    }
    
    if (max_len < 3) return {0, 0};
    return {best_dist, max_len};
}

void Compress(const std::string& input_path, const std::string& output_path) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open input file: " << input_path << "\n";
        return;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (data.empty()) return;

    std::cout << "Input size: " << data.size() << " bytes\n";

    // step 1: modeling
    // we need to understand the data before we compress it.
    // we build a frequency table (histogram) of all the bytes in the file.
    // this tells the rans encoder which bytes are common (cheap to encode) and which are rare (expensive).
    RansEncoder rans;
    rans.Init();
    rans.BuildModel(data);

    std::vector<uint8_t> literals;
    std::vector<Match> matches;
    std::vector<bool> is_match;

    // step 2: parsing (lz77)
    // we walk through the file and look for patterns we've seen before.
    // this is the "middle-out" part where we exploit the structure of the data.
    int pos = 0;
    while (pos < data.size()) {
        // we look backwards to see if the string starting at 'pos' appeared earlier.
        Match m = FindLongestMatch(data, pos, 32768);
        
        if (m.length >= 3) {
            // if we found a match of length 3 or more, it's worth compressing.
            // instead of writing the bytes, we write a "reference" to the previous occurrence.
            matches.push_back(m);
            is_match.push_back(true);
            pos += m.length;
        } else {
            // if no match found, we just keep the literal byte.
            literals.push_back(data[pos]);
            is_match.push_back(false);
            pos++;
        }
    }

    std::cout << "LZ77: " << matches.size() << " matches, " << literals.size() << " literals.\n";

    BitWriter flags_out;
    std::vector<uint8_t> packed_matches;
    
    // step 3: encoding
    // we now have three streams of information:
    // 1. flags: telling us if the next item is a match or a literal.
    // 2. matches: the distance and length of repeated patterns.
    // 3. literals: the raw bytes that couldn't be compressed with lz77.

    // first, we write the flags. the decoder needs these first to know what to do.
    for (bool flag : is_match) {
        flags_out.WriteBit(flag);
    }
    
    // next, we pack the matches.
    // we store distance and length simply. in a pro version, we'd compress these too.
    for (const auto& m : matches) {
        packed_matches.push_back(m.distance & 0xFF);
        packed_matches.push_back((m.distance >> 8) & 0xFF);
        packed_matches.push_back(m.length & 0xFF);
    }
    
    // finally, we encode the literals using rans.
    // rans is a "stack-based" entropy coder, meaning it's last-in-first-out (lifo).
    // to make sure the decoder reads the first literal first, we must encode them in reverse order (last literal first).
    for (int i = literals.size() - 1; i >= 0; --i) {
        rans.Encode(literals[i]);
    }

    flags_out.Flush();
    rans.Flush();

    // we get the compressed bitstreams.
    std::vector<uint8_t> rans_out = rans.GetOutput();
    std::vector<uint8_t> model_data = rans.GetModelData();

    // step 4: file format
    // we package everything into a single file with a header.
    // header: [magic] [sizes...]
    // body: [rans_data] [flags] [matches] [model]
    std::ofstream out(output_path, std::ios::binary);
    
    uint32_t magic = 0x4D49444F; // "MIDO"
    uint32_t orig_size = data.size();
    uint32_t rans_size = rans_out.size();
    uint32_t flags_size = flags_out.GetData().size();
    uint32_t match_size = packed_matches.size();
    uint32_t model_size = model_data.size();

    out.write((char*)&magic, 4);
    out.write((char*)&orig_size, 4);
    out.write((char*)&rans_size, 4);
    out.write((char*)&flags_size, 4);
    out.write((char*)&match_size, 4);
    out.write((char*)&model_size, 4);

    out.write((char*)rans_out.data(), rans_size);
    out.write((char*)flags_out.GetData().data(), flags_size);
    out.write((char*)packed_matches.data(), match_size);
    out.write((char*)model_data.data(), model_size);
    
    out.close();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    double time_s = elapsed.count();
    
    uint64_t compressed_size = 24 + rans_size + flags_size + match_size + model_size;
    double ratio = (double)orig_size / compressed_size;
    
    // weissman score: a metric from silicon valley to measure compression efficiency.
    // it balances compression ratio and speed.
    double weissman_score = ratio * std::log10(1.0 / (time_s + 0.0001)) * 10.0;
    if (weissman_score < 0) weissman_score = 0;

    std::cout << "--------------------------------------------------\n";
    std::cout << "Middle-Out Compression Results\n";
    std::cout << "--------------------------------------------------\n";
    std::cout << "Original Size   : " << orig_size << " bytes\n";
    std::cout << "Compressed Size : " << compressed_size << " bytes\n";
    std::cout << "Ratio           : " << std::fixed << std::setprecision(2) << ratio << "\n";
    std::cout << "Time            : " << std::fixed << std::setprecision(4) << time_s << " s\n";
    std::cout << "Weissman Score  : " << std::fixed << std::setprecision(2) << weissman_score << "\n";
    std::cout << "--------------------------------------------------\n";
}


void Decompress(const std::string& input_path, const std::string& output_path) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open input file: " << input_path << "\n";
        return;
    }
    
    uint32_t magic, orig_size, rans_size, flags_size, match_size, model_size;
    in.read((char*)&magic, 4);
    if (magic != 0x4D49444F) {
        std::cerr << "Invalid magic number\n";
        return;
    }
    in.read((char*)&orig_size, 4);
    in.read((char*)&rans_size, 4);
    in.read((char*)&flags_size, 4);
    in.read((char*)&match_size, 4);
    in.read((char*)&model_size, 4);

    std::vector<uint8_t> rans_data(rans_size);
    std::vector<uint8_t> flags_data(flags_size);
    std::vector<uint8_t> match_data(match_size);
    std::vector<uint8_t> model_data(model_size);

    in.read((char*)rans_data.data(), rans_size);
    in.read((char*)flags_data.data(), flags_size);
    in.read((char*)match_data.data(), match_size);
    in.read((char*)model_data.data(), model_size);
    in.close();

    RansDecoder rans;
    rans.Init(rans_data);
    rans.SetModel(model_data);

    BitReader flags_in(flags_data);
    
    std::vector<uint8_t> output;
    output.reserve(orig_size);

    size_t match_ptr = 0;
    int op_count = 0;
    while (output.size() < orig_size) {
        bool flag = flags_in.ReadBit();
        op_count++;
        if (!flag) {
            uint8_t lit = rans.Decode();
            output.push_back(lit);
            // std::cout << "Lit: " << (char)lit << "\n";
        } else {
            if (match_ptr + 3 > match_data.size()) {
                std::cerr << "Match data underflow!\n";
                break;
            }
            uint16_t dist = match_data[match_ptr] | (match_data[match_ptr+1] << 8);
            uint8_t len = match_data[match_ptr+2];
            match_ptr += 3;

            // std::cout << "Match: dist=" << dist << " len=" << (int)len << "\n";

            if (dist > output.size()) {
                 std::cerr << "Invalid distance: " << dist << " > " << output.size() << "\n";
                 break;
            }

            int start = output.size() - dist;
            for (int i = 0; i < len; ++i) {
                output.push_back(output[start + i]);
            }
        }
    }
    std::cout << "Decompression ops: " << op_count << "\n";

    std::ofstream out(output_path, std::ios::binary);
    out.write((char*)output.data(), output.size());
    out.close();
    std::cout << "Decompressed " << output.size() << " bytes.\n";
}

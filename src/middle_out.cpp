#include <iostream>
#include <string>
#include <vector>
#include "compressor.h"

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <command> <input_file> <output_file>\n";
    std::cerr << "Commands:\n";
    std::cerr << "  -c   Compress\n";
    std::cerr << "  -d   Decompress\n";
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    std::string input_path = argv[2];
    std::string output_path = argv[3];

    if (command == "-c") {
        std::cout << "Compressing " << input_path << " to " << output_path << "...\n";
        Compress(input_path, output_path);
    } else if (command == "-d") {
        std::cout << "Decompressing " << input_path << " to " << output_path << "...\n";
        Decompress(input_path, output_path);
    } else {
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}

cmake_minimum_required(VERSION 3.10)
project(middle_out)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(middle_out
    src/middle_out.cpp
    src/compressor.cpp
    src/suffix_array.cpp
    src/rans.cpp
    src/bitstream.cpp
)

target_include_directories(middle_out PRIVATE src)

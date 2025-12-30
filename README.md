# middleout

A C++ implementation of a **middle-out** compression algorithm – my personal love letter to that absolute genius moment in *Silicon Valley*.

Yeah, the one where Richard figures it out on the whiteboard and everything clicks 🤌. Tip-to-tip efficiency, baby.

It started as a meme after a late-night rewatch, but I ended up going down the rabbit hole and building a real (ish) compressor in C++. It splits data blocks from the center and encodes outward with a custom entropy scheme. Surprisingly, it actually outperforms basic Huffman on some datasets. Plus, I added a fake Weissman score calculator because how could I not?

![The legendary whiteboard](https://raw.githubusercontent.com/shadow-forge-dev/middle_out/assets/whiteboard.jpg)
*The moment that started it all.*

## Build & Run

```bash
# Clone and build
git clone https://github.com/anon-forge/middle_out.git
cd middle_out
mkdir build && cd build
cmake ..
make

# Compress
./middleout compress input.txt output.mo

# Decompress
./middleout decompress output.mo recovered.txt

# Weissman score (for the memes)
./middleout weissman input.txt

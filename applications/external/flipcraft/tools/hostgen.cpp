//   c++ -std=c++17 -O2 -o /tmp/hostgen tools/hostgen.cpp
//   /tmp/hostgen out.fcw 16 12345      # chunks per side: 16/32/64/128
#include "../src/world/gen_core.h"

#include <cstdio>
#include <cstdlib>

static bool fileWriteAt(void* ctx, uint32_t offset, const void* data, size_t n) {
    FILE* f = reinterpret_cast<FILE*>(ctx);
    return fseek(f, (long)offset, SEEK_SET) == 0 && fwrite(data, 1, n, f) == n;
}

static void onProgress(void*, uint8_t percent) {
    fprintf(stderr, "\r%3u%%", percent);
}

int main(int argc, char** argv) {
    if(argc < 2) {
        fprintf(stderr, "usage: %s out.fcw [chunks=16] [seed=random]\n", argv[0]);
        return 2;
    }
    int chunks = argc > 2 ? atoi(argv[2]) : 16;
    uint32_t seed = argc > 3 ? (uint32_t)strtoul(argv[3], nullptr, 0) : (uint32_t)rand();

    FILE* f = fopen(argv[1], "wb");
    if(!f) {
        perror("fopen");
        return 1;
    }
    fcgen::Writer out = {fileWriteAt, f};
    bool ok = fcgen::generate(chunks, seed, out, onProgress, nullptr);
    fclose(f);
    fprintf(stderr, "\n%s: chunks=%d seed=%u -> %s\n", ok ? "OK" : "FAIL", chunks, seed, argv[1]);
    return ok ? 0 : 1;
}

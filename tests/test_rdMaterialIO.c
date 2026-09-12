/* Asset-free regression: actual loader I/O helper, synthetic packed cels. */
#include "Engine/rdMaterialIO.h"
#include <assert.h>
#include <stdio.h>

static unsigned char input[4096];
static size_t cursor, available, chunk;
static size_t readMemory(intptr_t file, void* out, size_t count)
{
    (void)file;
    if (count > chunk) count = chunk;
    if (count > available - cursor) count = available - cursor;
    memcpy(out, input + cursor, count);
    cursor += count;
    return count;
}

static void checkCels(size_t width, size_t height, size_t bytesPerPixel, size_t count, size_t shortChunk)
{
    size_t rowBytes = width * bytesPerPixel;
    size_t stride = (rowBytes + 3) & ~(size_t)3;
    unsigned char output[512];
    cursor = 0; available = 0; chunk = shortChunk;
    for (size_t cel = 0; cel < count; ++cel) {
        uint32_t header[6] = {(uint32_t)width, (uint32_t)height, 0, 0, 0, 1};
        memcpy(input + available, header, sizeof(header)); available += sizeof(header);
        for (size_t pixel = 0; pixel < rowBytes * height; ++pixel) input[available++] = (unsigned char)(cel + pixel + 1);
    }
    input[available++] = 0xa5;
    for (size_t cel = 0; cel < count; ++cel) {
        uint32_t header[6] = {0};
        assert(rdMaterial_ReadExact(readMemory, 0, header, sizeof(header)));
        assert(header[0] == width && header[1] == height && header[5] == 1);
        memset(output, 0xcc, sizeof(output));
        assert(rdMaterial_ReadPackedRows(readMemory, 0, output + 1, stride * height, rowBytes, height, stride));
        assert(output[0] == 0xcc && output[1 + stride * height] == 0xcc);
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < rowBytes; ++x) assert(output[1+y*stride+x] == (unsigned char)(cel+y*rowBytes+x+1));
            for (size_t x = rowBytes; x < stride; ++x) assert(output[1+y*stride+x] == 0);
        }
    }
    assert(cursor + 1 == available && input[cursor] == 0xa5);
}

int main(void)
{
    checkCels(2, 2, 1, 14, 4096); /* Castle Conflict's legitimate shape. */
    checkCels(2, 2, 1, 14, 1);    /* Resume short reads without losing cel alignment. */
    checkCels(1, 1, 1, 3, 2);
    checkCels(3, 4, 1, 3, 4096);
    checkCels(4, 4, 1, 3, 4096);
    checkCels(1, 2, 2, 3, 3);
    checkCels(3, 4, 2, 3, 4096);
    unsigned char output[16];
    memset(output, 0xcc, sizeof(output)); cursor = 0; available = 3; chunk = 4096;
    assert(!rdMaterial_ReadPackedRows(readMemory, 0, output, sizeof(output), 2, 2, 4));
    assert(cursor == 3); /* truncated pixels fail instead of eating another record */
    cursor = 0;
    assert(!rdMaterial_ReadPackedRows(readMemory, 0, output, sizeof(output), 4, 2, 2));
    assert(!rdMaterial_ReadPackedRows(readMemory, 0, output, 7, 2, 2, 4));
    assert(cursor == 0);
    assert(rdMaterial_ValidTextureLayout(2, 2, 1, 4));
    assert(rdMaterial_ValidTextureLayout(8, 8, 4, 4));
    assert(!rdMaterial_ValidTextureLayout(2, 2, 3, 4));
    assert(!rdMaterial_ValidTextureLayout(64, 64, 5, 4));
    assert(!rdMaterial_ValidTextureLayout(2, 1, 2172748239u, 4));
    assert(!rdMaterial_ValidTextureLayout(0, 2, 1, 4));
    puts("MAT packed-row regression controls passed.");
    return 0;
}

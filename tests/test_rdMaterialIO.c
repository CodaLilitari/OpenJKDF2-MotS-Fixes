/* Asset-free regression: actual loader I/O helper, synthetic packed cels. */
#include "Engine/rdMaterialIO.h"
#include <assert.h>
#include <stdio.h>

static unsigned char input[400000];
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

/* Complete packed chains followed by another cel and an inline palette sentinel. */
static void checkExtendedCels(uint32_t width, uint32_t height, uint32_t mips,
                              uint32_t bpp, uint32_t expectedResident, size_t shortChunk)
{
    static unsigned char output[262144 + 2];
    uint32_t bytesPerPixel = bpp / 8;
    cursor = 0; available = 0; chunk = shortChunk;
    for (unsigned cel = 0; cel < 2; ++cel) {
        uint32_t header[6] = {width, height, 0, 0, 0, mips};
        memcpy(input + available, header, sizeof(header)); available += sizeof(header);
        uint32_t w = width, h = height;
        for (uint32_t level = 0; level < mips; ++level) {
            size_t bytes = (size_t)w * h * bytesPerPixel;
            assert(available + bytes + 768 < sizeof(input));
            memset(input + available, 1 + cel * 16 + level, bytes); available += bytes;
            w = w > 1 ? w >> 1 : 1; h = h > 1 ? h >> 1 : 1;
        }
    }
    memset(input + available, 0xa5, 768); available += 768;
    for (unsigned cel = 0; cel < 2; ++cel) {
        uint32_t header[6];
        assert(rdMaterial_ReadExact(readMemory, 0, header, sizeof(header)));
        assert(header[0] == width && header[1] == height && header[5] == mips);
        assert(rdMaterial_ValidTextureLayout(width, height, mips, 32));
        uint32_t resident = rdMaterial_ResidentMipCount(width, height, mips, 4);
        assert(resident == expectedResident && resident <= 4);
        uint32_t w = width, h = height;
        for (uint32_t level = 0; level < mips; ++level) {
            if (level < resident) {
                size_t rowBytes = w * bytesPerPixel, stride = (rowBytes + 3) & ~(size_t)3;
                size_t capacity = stride * h;
                assert(capacity + 2 < sizeof(output));
                memset(output, 0xcc, capacity + 2);
                assert(rdMaterial_ReadPackedRows(readMemory, 0, output + 1, capacity, rowBytes, h, stride));
                assert(output[0] == 0xcc && output[1 + capacity] == 0xcc);
                for (uint32_t y = 0; y < h; ++y) {
                    for (size_t x = 0; x < rowBytes; ++x) assert(output[1 + y * stride + x] == 1 + cel * 16 + level);
                    for (size_t x = rowBytes; x < stride; ++x) assert(output[1 + y * stride + x] == 0);
                }
            } else assert(rdMaterial_DiscardMip(readMemory, 0, w, h, bpp));
            w = w > 1 ? w >> 1 : 1; h = h > 1 ? h >> 1 : 1;
        }
    }
    assert(cursor + 768 == available);
    for (size_t i = cursor; i < available; ++i) assert(input[i] == 0xa5);
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
    assert(!rdMaterial_ValidTextureLayout(8, 8, 33, 64)); /* Reject shifts wider than uint32_t. */
    assert(!rdMaterial_ValidTextureLayout(0, 2, 1, 4));
    assert(rdMaterial_ValidTextureLayout(256, 1, 9, 32)); /* disk chain clamps the short axis */
    checkExtendedCels(256, 256, 9, 8, 4, 4096);
    checkExtendedCels(256, 256, 9, 16, 4, 31);
    checkExtendedCels(256, 1, 9, 8, 1, 1);
    checkExtendedCels(1, 128, 8, 16, 1, 3);
    checkExtendedCels(128, 32, 8, 8, 4, 7);
    checkExtendedCels(4, 64, 7, 16, 3, 13);
    checkExtendedCels(3, 5, 3, 8, 2, 2);
    assert(!rdMaterial_ValidTextureLayout(256, 256, 10, 32));
    assert(!rdMaterial_ValidTextureLayout(1, 1, 2, 32));
    assert(!rdMaterial_ValidTextureLayout(UINT32_MAX, 1, 32, 32));
    assert(!rdMaterial_ValidTextureLayout(256, 256, UINT32_MAX, 32));
    assert(rdMaterial_ValidTextureLayout(1, 1, 0, 32)); /* retain legacy empty-cel allowance */
    assert(rdMaterial_ResidentMipCount(256, 256, 0, 4) == 0);
    assert(rdMaterial_ResidentMipCount(256, 256, 9, 0) == 0);
    cursor = 0; available = 255; chunk = 7;
    assert(!rdMaterial_DiscardMip(readMemory, 0, 16, 16, 8));
    assert(cursor == 255); /* do not hide truncated tail behind an unchecked seek */
    cursor = 0; available = 256;
    assert(!rdMaterial_DiscardMip(readMemory, 0, 16, 16, 24));
    assert(!rdMaterial_DiscardMip(readMemory, 0, 0, 16, 8));
    assert(cursor == 0);
    puts("MAT packed-row regression controls passed.");
    return 0;
}

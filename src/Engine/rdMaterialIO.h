#ifndef RDMATERIAL_IO_H
#define RDMATERIAL_IO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef size_t (*rdMaterialReadFn)(intptr_t, void*, size_t);

/* MAT pixels are packed on disk; destination rows may contain SDL padding. */
static inline int rdMaterial_ReadExact(rdMaterialReadFn readFn, intptr_t file,
                                      void* destination, size_t size)
{
    unsigned char* out = (unsigned char*)destination;
    while (size) {
        size_t received = readFn(file, out, size);
        if (!received || received > size) return 0;
        out += received;
        size -= received;
    }
    return 1;
}

static inline int rdMaterial_ReadPackedRows(rdMaterialReadFn readFn, intptr_t file,
    void* destination, size_t capacity, size_t rowBytes, size_t rows, size_t stride)
{
    if (!destination || !rowBytes || !rows || stride < rowBytes || rows > capacity / stride)
        return 0;
    unsigned char* row = (unsigned char*)destination;
    for (size_t y = 0; y < rows; ++y, row += stride) {
        if (!rdMaterial_ReadExact(readFn, file, row, rowBytes)) return 0;
        memset(row + rowBytes, 0, stride - rowBytes);
    }
    return 1;
}

/* The file may carry a full chain; resident arrays keep their legacy capacity. */
static inline int rdMaterial_ValidTextureLayout(uint32_t width, uint32_t height,
                                               uint32_t mips, size_t maximumMips)
{
    if (!width || !height || width > INT32_MAX || height > INT32_MAX || mips > maximumMips)
        return 0;
    uint32_t longest = width > height ? width : height;
    uint32_t levels = 1;
    while (longest > 1) { longest >>= 1; ++levels; }
    return mips <= levels;
}

/* Rasterizers shift both axes. Do not expose a resident zero-width/height mip. */
static inline uint32_t rdMaterial_ResidentMipCount(uint32_t width, uint32_t height,
                                                  uint32_t diskMips, size_t capacity)
{
    uint32_t count = 0;
    while (count < diskMips && count < capacity && width && height) {
        ++count;
        width >>= 1;
        height >>= 1;
    }
    return count;
}

/* Reading (rather than seeking) also detects a truncated discarded mip. */
static inline int rdMaterial_DiscardMip(rdMaterialReadFn readFn, intptr_t file,
                                       uint32_t width, uint32_t height, uint32_t bpp)
{
    unsigned char scratch[256];
    if (!width || !height || (!bpp || bpp > 32 || bpp % 8)) return 0;
    size_t bytesPerPixel = bpp / 8;
    if (width > SIZE_MAX / bytesPerPixel) return 0;
    size_t rowBytes = (size_t)width * bytesPerPixel;
    if (height > SIZE_MAX / rowBytes) return 0;
    size_t bytes = rowBytes * height;
    while (bytes) {
        size_t n = bytes < sizeof(scratch) ? bytes : sizeof(scratch);
        if (!rdMaterial_ReadExact(readFn, file, scratch, n)) return 0;
        bytes -= n;
    }
    return 1;
}
#endif

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

static inline int rdMaterial_ValidTextureLayout(uint32_t width, uint32_t height,
                                               uint32_t mips, size_t maximumMips)
{
    if (!width || !height || width > INT32_MAX || height > INT32_MAX || mips > maximumMips)
        return 0;
    return !mips || (mips <= 32 && (width >> (mips - 1)) && (height >> (mips - 1)));
}
#endif

# Packed MAT rows and padded display surfaces

MAT files store each mip as tightly packed pixel rows. SDL display surfaces may pad each destination row. For a 2-by-2 8-bit cel, the file contains four pixel bytes while the surface pitch is four bytes per row and its allocation is eight bytes.

Previously rdMaterial_LoadEntry_Common read the whole padded allocation from the MAT file. A valid animated material with fourteen 2-by-2 cels exposed this: reading eight instead of four bytes consumed the next cel header. A later pixel word was interpreted as a huge mip count, and the loader indexed beyond its four mip slots.

The loader now reads exactly the packed bytes for each row, writes them at the display stride, zeros destination padding, and resumes short reads. Truncated payloads fail explicitly. Texture headers are read exactly and their dimensions/mip counts are validated before indexing the fixed arrays. These changes preserve source materials and their authored animation; they do not require changing a map or redistributing game assets.

## Asset-free regression

The test uses the same I/O helper as the engine and constructs its own synthetic pixel data. It checks fourteen sequential 2-by-2 cels, pixel values and cel-header alignment, guard bytes, zeroed padding, 1/2/3/4-byte row widths, 8/16-bit formats, short reads, truncation, invalid strides/capacities and invalid mip layouts.

Run with a C11 compiler from the source root, writing the executable outside the source checkout:

```sh
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Isrc tests/test_rdMaterialIO.c -o /tmp/test_rdMaterialIO
/tmp/test_rdMaterialIO
```

The test also compiles with MinGW-w64 and runs as a Windows console executable. No original level, texture, or other game asset is needed.

## Runtime validation boundary

The unchanged Castle Conflict conversion passed a 30-second MotS multiplayer host smoke using an isolated candidate runtime. The test observed the expected episode/world, a local player, advancing simulation and no resource diagnostics. Original and staged archives stayed unchanged. Separate 15-second native MotS MP and SP baseline smokes also passed with the expected worlds, advancing simulation, no resource diagnostics and unchanged archives. This establishes the loading correction; it does not certify rendered appearance, complete map gameplay, second-client synchronization or retail parity.

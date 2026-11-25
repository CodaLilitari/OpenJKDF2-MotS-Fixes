#define STDCAPTURE_UNIT_TEST
#include "Main/stdCapture.c"
#include <assert.h>

int main(int argc, char** argv)
{
    assert(argc == 3);
    char* capture[] = {"game", "-capture", "-autostart", "-sp", "-episode", "ep", "-map", "map.jkl"};
    assert(!setenv("OPENJKDF2_CAPTURE_REQUEST", argv[1], 1));
    assert(!setenv("OPENJKDF2_CAPTURE_BACKEND", "sdl-offscreen", 1));
    assert(stdCapture_Init(8, capture));
    snprintf(stdCapture_request.output, sizeof(stdCapture_request.output), "%s", argv[2]);
    unsigned char pixels[64*64*3] = {0};
    assert(!stdCapture_WriteRgb(pixels, 64, 64));
    assert(!stdCapture_RecordGraphics("x11", "3.4.12", "Mesa", "llvmpipe", "4.5"));
    assert(!stdCapture_RecordGraphics(NULL, "3.4.12", "Mesa", "llvmpipe", "4.5"));
    assert(!stdCapture_RecordGraphics("offscreen", "3.4.12", NULL, "llvmpipe", "4.5"));
    assert(!stdCapture_RecordGraphics("offscreen", "3.4.12", "Mesa", "", "4.5"));
    assert(!stdCapture_RecordGraphics("offscreen", "3.4.12", "Mesa", "llvmpipe", ""));
    assert(!stdCapture_RecordGraphics("offscreen", "3.4.12", "Mesa", "line\nbreak", "4.5"));
    assert(!stdCapture_RecordGraphics("offscreen", "3.4.12", "Mesa", "\x80", "4.5"));
    assert(!stdCapture_RecordGraphics("offscreen", "3.4.x", "Mesa", "llvmpipe", "4.5"));
    assert(!stdCapture_RecordGraphics("offscreen", "3.4.12.1", "Mesa", "llvmpipe", "4.5"));
    char longValue[257]; memset(longValue, 'A', sizeof(longValue)); longValue[256] = 0;
    assert(!stdCapture_RecordGraphics("offscreen", "3.4.12", "Mesa", longValue, "4.5"));
    longValue[255] = 0;
    assert(stdCapture_RecordGraphics("offscreen", "3.4.12", "Mesa", longValue, "4.5"));
    // Failed refresh invalidates an earlier success; it cannot publish stale data.
    assert(!stdCapture_RecordGraphics("wayland", "3.4.12", "Mesa", "llvmpipe", "4.5"));
    assert(!stdCapture_WriteRgb(pixels, 64, 64));
    assert(stdCapture_RecordGraphics("offscreen", "3.4.12", "Mesa", "quoted \"GPU\" \\ test", "4.5"));
    assert(stdCapture_WriteRgb(pixels, 64, 64));
    stdCapture_ClearGraphics();
    assert(!stdCapture_WriteRgb(pixels, 64, 64));
    puts("capture graphics tests passed (observations, bounds, escaping, stale report rejection)");
    return 0;
}

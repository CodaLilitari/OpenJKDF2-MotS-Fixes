#define STDCAPTURE_UNIT_TEST
#include "Main/stdCapture.c"
#include <assert.h>

int main(int argc, char** argv)
{
    assert(argc == 2);
    char* capture[] = {"game", "-capture", "-autostart", "-sp", "-episode", "ep", "-map", "map.jkl"};
    assert(!setenv("OPENJKDF2_CAPTURE_REQUEST", argv[1], 1));
    const char* invalid[] = {"", "auto", "offscreen", "sdl-offscreen,xvfb", "SDL-OFFSCREEN", "../xvfb"};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        active = 0;
        assert(!setenv("OPENJKDF2_CAPTURE_BACKEND", invalid[i], 1));
        assert(!stdCapture_Init(8, capture));
        assert(!stdCapture_IsActive());
    }
    assert(!unsetenv("OPENJKDF2_CAPTURE_BACKEND"));
    assert(stdCapture_Init(8, capture));
    assert(stdCapture_IsActive());
    assert(!setenv("OPENJKDF2_CAPTURE_BACKEND", "native", 1));
    assert(stdCapture_Init(8, capture));
    assert(!setenv("OPENJKDF2_CAPTURE_BACKEND", "sdl-offscreen", 1));
#ifdef __linux__
    assert(stdCapture_Init(8, capture));
#else
    assert(!stdCapture_Init(8, capture));
#endif
    assert(!setenv("OPENJKDF2_CAPTURE_BACKEND", "xvfb", 1));
#ifdef __linux__
    assert(stdCapture_Init(8, capture));
#else
    assert(!stdCapture_Init(8, capture));
#endif
    // Reinitialization cannot retain capture state or an invalid backend.
    assert(!unsetenv("OPENJKDF2_CAPTURE_REQUEST"));
    char* ordinary[] = {"game"};
    assert(stdCapture_Init(1, ordinary));
    assert(!stdCapture_IsActive());
    puts("capture backend tests passed (explicit intent, Linux-only modes, ordinary launch)");
    return 0;
}

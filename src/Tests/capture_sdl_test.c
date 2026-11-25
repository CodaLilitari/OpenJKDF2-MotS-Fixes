// Exercise the production SDL boundary with controlled failures, without SDL.
#define STDCAPTURE_UNIT_TEST
#include "Main/stdCapture.c"
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>

typedef struct { int unused; } SDL_Window;
typedef void* SDL_GLContext;
typedef void (*SDL_FunctionPointer)(void);
#define SDL_HINT_VIDEO_DRIVER "SDL_VIDEO_DRIVER"
#define SDL_HINT_OVERRIDE 2
#define SDL_INIT_VIDEO 0x20
#define SDL_VERSIONNUM_MAJOR(v) ((v) / 1000000)
#define SDL_VERSIONNUM_MINOR(v) (((v) / 1000) % 1000)
#define SDL_VERSIONNUM_MICRO(v) ((v) % 1000)
static int hintCalls, initCalls, currentCalls;
static bool hintOk = true, initOk = true, currentOk = true, procOk = true;
static const char* driver = "offscreen";
static const char* vendor = "Mesa";
static SDL_GLContext observedContext;
static char error[256];
static bool SDL_SetHintWithPriority(const char* key, const char* value, int priority)
{
    ++hintCalls;
    assert(!strcmp(key, SDL_HINT_VIDEO_DRIVER) && !strcmp(value, "offscreen"));
    assert(priority == SDL_HINT_OVERRIDE);
    return hintOk;
}
static bool SDL_Init(unsigned int flags) { ++initCalls; assert(flags == SDL_INIT_VIDEO); return initOk; }
static const char* SDL_GetCurrentVideoDriver(void) { return driver; }
static bool SDL_SetError(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt); vsnprintf(error, sizeof(error), fmt, ap); va_end(ap); return false;
}
static bool SDL_GL_MakeCurrent(SDL_Window* window, SDL_GLContext context)
{ ++currentCalls; assert(window && context); return currentOk; }
static SDL_GLContext SDL_GL_GetCurrentContext(void) { return observedContext; }
static int SDL_GetVersion(void) { return 3004012; }
static const unsigned char* getString(unsigned int name)
{
    if (name == 0x1f00) return (const unsigned char*)vendor;
    if (name == 0x1f01) return (const unsigned char*)"llvmpipe";
    assert(name == 0x1f02); return (const unsigned char*)"4.5";
}
static SDL_FunctionPointer SDL_GL_GetProcAddress(const char* name)
{ assert(!strcmp(name, "glGetString")); return procOk ? (SDL_FunctionPointer)getString : NULL; }
#include "Platform/SDL2/stdCaptureGraphics.h"

int main(int argc, char** argv)
{
    assert(argc == 2);
    char* capture[] = {"game", "-capture", "-autostart", "-sp", "-episode", "ep", "-map", "map.jkl"};
    assert(!setenv("OPENJKDF2_CAPTURE_REQUEST", argv[1], 1));
    assert(!setenv("OPENJKDF2_CAPTURE_BACKEND", "sdl-offscreen", 1));
    assert(stdCapture_Init(8, capture));
    hintOk = false;
    assert(!stdCapture_InitVideo() && hintCalls == 1 && initCalls == 0);
    hintOk = true; initOk = false;
    assert(!stdCapture_InitVideo() && hintCalls == 2 && initCalls == 1);
    initOk = true; driver = "x11";
    assert(!stdCapture_InitVideo() && hintCalls == 3 && initCalls == 2);
    driver = NULL;
    assert(!stdCapture_InitVideo() && hintCalls == 4 && initCalls == 3);
    driver = "offscreen";
    assert(stdCapture_InitVideo() && hintCalls == 5 && initCalls == 4);
    SDL_Window window = {0}; SDL_GLContext context = &window; observedContext = context;
    assert(stdCapture_MakeCurrent(&window, context) && graphics.ready);
    currentOk = false;
    assert(!stdCapture_MakeCurrent(&window, context) && !graphics.ready);
    currentOk = true; observedContext = NULL;
    assert(!stdCapture_MakeCurrent(&window, context) && !graphics.ready);
    observedContext = context; procOk = false;
    assert(!stdCapture_MakeCurrent(&window, context) && !graphics.ready);
    procOk = true; vendor = NULL;
    assert(!stdCapture_MakeCurrent(&window, context) && !graphics.ready);
    vendor = "Mesa";
    assert(stdCapture_MakeCurrent(&window, context) && graphics.ready);
    assert(currentCalls == 6);
    assert(!strcmp(graphics.sdlVersion, "3.4.12"));
    assert(!unsetenv("OPENJKDF2_CAPTURE_REQUEST"));
    char* ordinary[] = {"game"}; assert(stdCapture_Init(1, ordinary));
    assert(!stdCapture_InitVideo() && initCalls == 4 && hintCalls == 5);
    assert(!stdCapture_MakeCurrent(&window, context) && currentCalls == 6);
    puts("capture SDL boundary tests passed (driver, initialization, current context, observations, no fallback)");
    return 0;
}

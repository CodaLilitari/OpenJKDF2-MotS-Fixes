#ifndef OPENJKDF2_STDCAPTURE_GRAPHICS_H
#define OPENJKDF2_STDCAPTURE_GRAPHICS_H

// Include after SDL. Shared by the production window path and its graphics
// smoke test, without requiring the engine's GL loader to observe a context.
#include "Main/stdCapture.h"
#include <stdio.h>
#include <string.h>

static int stdCapture_InitVideo(void)
{
    if (!stdCapture_IsActive()) return SDL_SetError("capture: no active request");
    const char* expected = stdCapture_ExpectedVideoDriver();
    if (expected && !SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, expected, SDL_HINT_OVERRIDE))
        return SDL_SetError("capture: cannot select requested SDL video driver");
    if (!SDL_Init(SDL_INIT_VIDEO)) return 0;
    const char* driver = SDL_GetCurrentVideoDriver();
    if (!driver || !*driver || (expected && strcmp(driver, expected)))
        return SDL_SetError("capture: initialized SDL driver does not match request");
    return 1;
}

#ifdef _WIN32
#define STDCAPTURE_GL_CALL __stdcall
#else
#define STDCAPTURE_GL_CALL
#endif
typedef const unsigned char* (STDCAPTURE_GL_CALL *stdCaptureGlGetString)(unsigned int);
#undef STDCAPTURE_GL_CALL

static int stdCapture_MakeCurrent(SDL_Window* window, SDL_GLContext context)
{
    stdCapture_ClearGraphics();
    if (!stdCapture_IsActive()) return SDL_SetError("capture: no active request");
    if (!SDL_GL_MakeCurrent(window, context)) return 0;
    if (!context || SDL_GL_GetCurrentContext() != context)
        return SDL_SetError("capture: requested OpenGL context is not current");
    stdCaptureGlGetString getString = (stdCaptureGlGetString)SDL_GL_GetProcAddress("glGetString");
    if (!getString) return SDL_SetError("capture: glGetString is unavailable");
    int version = SDL_GetVersion();
    char sdlVersion[32];
    snprintf(sdlVersion, sizeof(sdlVersion), "%d.%d.%d", SDL_VERSIONNUM_MAJOR(version),
             SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version));
    if (!stdCapture_RecordGraphics(SDL_GetCurrentVideoDriver(), sdlVersion,
            (const char*)getString(0x1f00), (const char*)getString(0x1f01), (const char*)getString(0x1f02)))
        return SDL_SetError("capture: invalid or mismatched graphics observations");
    return 1;
}

#endif

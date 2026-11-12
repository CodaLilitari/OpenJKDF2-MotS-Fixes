#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
typedef unsigned int GLenum;
typedef int GLint;
#define GL_TRUE 1
#define GLEW_OK 0
#define GLEW_ERROR_NO_GLX_DISPLAY 4
static int glewExperimental, gl33, gl30 = 1, gl20 = 1, gl15 = 1, calls;
static int activeTexture = 1, blendEquation = 1, major = 4, minor = 5;
#define glActiveTexture activeTexture
#define glBlendEquation blendEquation
#define GLEW_VERSION_3_3 gl33
#define GLEW_VERSION_3_0 gl30
#define GLEW_VERSION_2_0 gl20
#define GLEW_VERSION_1_5 gl15
#define GL_MAJOR_VERSION 0x821B
#define GL_MINOR_VERSION 0x821C
void glGetIntegerv(GLenum name, GLint* value) { *value = name == GL_MAJOR_VERSION ? major : minor; }
static GLenum status;
static const char* backend = "sdl-offscreen";
static GLenum glewInit(void) { ++calls; return status; }
const char* stdCapture_GraphicsBackend(void) { return backend; }
#include "Platform/SDL2/stdCaptureGlLoader.h"

int main(void)
{
    GLenum observed = 99;
    gl33 = 1; status = GLEW_OK;
    assert(stdCapture_InitGlLoader(&observed) && observed == status && glewExperimental == GL_TRUE);
    status = GLEW_ERROR_NO_GLX_DISPLAY;
#ifdef __linux__
    assert(stdCapture_InitGlLoader(&observed) && observed == status);
#else
    assert(!stdCapture_InitGlLoader(&observed));
#endif
    gl33 = 0;
    assert(!stdCapture_InitGlLoader(&observed));
    gl33 = 1; status = 1;
    assert(!stdCapture_InitGlLoader(&observed));
    status = GLEW_OK; major = 3; minor = 2;
    assert(!stdCapture_InitGlLoader(&observed));
    minor = 3;
    assert(stdCapture_InitGlLoader(&observed));
    gl20 = 0;
    assert(!stdCapture_InitGlLoader(&observed));
    gl20 = 1; gl30 = 0;
    assert(!stdCapture_InitGlLoader(&observed));
    gl30 = 1; gl15 = 0;
    assert(!stdCapture_InitGlLoader(&observed));
    gl15 = 1; activeTexture = 0;
    assert(!stdCapture_InitGlLoader(&observed));
    activeTexture = 1; blendEquation = 0;
    assert(!stdCapture_InitGlLoader(&observed));
    blendEquation = 1;
    backend = "native"; glewExperimental = 0;
    assert(stdCapture_InitGlLoader(NULL) && !glewExperimental);
    assert(calls == 12);
    puts("capture GL loader policy passed (verified EGL/GLX case, GL3.3 requirement, ordinary behavior)");
    return 0;
}

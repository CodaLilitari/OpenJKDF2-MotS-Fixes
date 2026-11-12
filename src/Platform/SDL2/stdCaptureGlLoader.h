#ifndef OPENJKDF2_STDCAPTURE_GL_LOADER_H
#define OPENJKDF2_STDCAPTURE_GL_LOADER_H

// Include after GLEW. SDL's offscreen EGL context has no GLX display. GLEW may
// report that absence after successfully loading desktop GL; accept it only
// for explicit Linux offscreen capture with the required GL3.3 entry points.
#include "Main/stdCapture.h"
#include <string.h>

static int stdCapture_InitGlLoader(GLenum* observedStatus)
{
    int offscreen = !strcmp(stdCapture_GraphicsBackend(), "sdl-offscreen");
    if (offscreen) glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if (observedStatus) *observedStatus = status;
    if (!offscreen) return 1; // Preserve existing normal/capture launch policy.
    int loaded = status == GLEW_OK;
#ifdef __linux__
    loaded = loaded || status == GLEW_ERROR_NO_GLX_DISPLAY;
#endif
    // With glewExperimental, each version flag reflects its own function
    // group; GL3.3 alone neither proves earlier groups nor the context version.
    // These are the dynamic core groups used by the engine renderer. Check the
    // two older functions directly: their whole legacy groups include APIs
    // removed from core profiles that the engine does not need.
    if (!loaded || !GLEW_VERSION_1_5 || !GLEW_VERSION_2_0
        || !GLEW_VERSION_3_0 || !GLEW_VERSION_3_3 || !glActiveTexture || !glBlendEquation) return 0;
    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    return major > 3 || (major == 3 && minor >= 3);
}

#endif

// Asset-free graphics integration, not a game or camera acceptance test.
#define STDCAPTURE_UNIT_TEST
#include "Main/stdCapture.c"
#include <assert.h>
#include <GL/glew.h>
#include <SDL3/SDL.h>
#include "Platform/SDL2/stdCaptureGraphics.h"
#include "Platform/SDL2/stdCaptureGlLoader.h"

#ifdef STDCAPTURE_MISSING_SHADER
// GLEW's version flags are independent. Remove one real loader lookup to
// establish that the production guard rejects a missing shader entry point.
typedef void (*CaptureTestProc)(void);
extern CaptureTestProc __real_glXGetProcAddressARB(const unsigned char*);
CaptureTestProc __wrap_glXGetProcAddressARB(const unsigned char* name)
{
    if (!strcmp((const char*)name, "glCreateShader")) return NULL;
    return __real_glXGetProcAddressARB(name);
}
#endif

static GLuint shader(GLenum kind, const char* source)
{
    GLuint object = glCreateShader(kind); assert(object);
    glShaderSource(object, 1, &source, NULL);
    glCompileShader(object);
    GLint ok = 0; glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(object, sizeof(log), NULL, log);
        fprintf(stderr, "shader: %s\n", log);
    }
    assert(ok); return object;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    assert(!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"));
    assert(!setenv("OPENJKDF2_CAPTURE_REQUEST", argv[1], 1));
    assert(!setenv("OPENJKDF2_CAPTURE_BACKEND", "sdl-offscreen", 1));
    char* capture[] = {"test", "-capture", "-autostart", "-sp", "-episode", "ep", "-map", "map.jkl"};
    assert(stdCapture_Init(8, capture));
    if (!stdCapture_InitVideo()) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));
#ifdef STDCAPTURE_GL32_CONTEXT
    assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2));
#else
    assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
#endif
    assert(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
    assert(SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1));
    SDL_Window* window = SDL_CreateWindow("capture graphics test", 64, 64,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE);
    if (!window) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context || !stdCapture_MakeCurrent(window, context)) {
        fprintf(stderr, "%s\n", SDL_GetError()); return 1;
    }
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    assert((flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE)) == (SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE));
    assert(!(flags & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_FULLSCREEN)));
    GLenum loader;
    assert(stdCapture_InitGlLoader(&loader));
    printf("GLEW_STATUS=%u\n", loader);
    // Preserve any loader diagnostics separately from the operations below.
    GLenum startupError = glGetError();
    printf("GLEW_GL_ERROR=%u\n", startupError);
    assert(glGetError() == GL_NO_ERROR);
    const char* vertex = "#version 330 core\nvoid main(){vec2 p[3]=vec2[3](vec2(-1,-1),vec2(3,-1),vec2(-1,3));gl_Position=vec4(p[gl_VertexID],0,1);}";
    const char* fragment = "#version 330 core\nout vec4 color;void main(){color=vec4(gl_FragCoord.x<32?1:0,gl_FragCoord.y<32?1:0,gl_FragCoord.x>=32?1:0,1);}";
    GLuint vs = shader(GL_VERTEX_SHADER, vertex), fs = shader(GL_FRAGMENT_SHADER, fragment);
    GLuint program = glCreateProgram(); assert(program);
    glAttachShader(program, vs); glAttachShader(program, fs); glLinkProgram(program);
    GLint linked = 0; glGetProgramiv(program, GL_LINK_STATUS, &linked); assert(linked);
    GLuint vao = 0; glGenVertexArrays(1, &vao); assert(vao); glBindVertexArray(vao);
    glUseProgram(program); glViewport(0, 0, 64, 64);
    glDisable(GL_DITHER); glDisable(GL_BLEND); glDisable(GL_DEPTH_TEST);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
#ifndef STDCAPTURE_NEGATIVE_DRAW
    glDrawArrays(GL_TRIANGLES, 0, 3);
#endif
    glFinish();
    unsigned char pixels[64*64*3];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 64, 64, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    assert(glGetError() == GL_NO_ERROR);
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
        int offset = (y*64+x)*3;
        assert(pixels[offset] == (x < 32 ? 255 : 0));
        assert(pixels[offset+1] == (y < 32 ? 255 : 0));
        assert(pixels[offset+2] == (x >= 32 ? 255 : 0));
    }
    assert(stdCapture_WriteRgb(pixels, 64, 64));
    glDeleteVertexArrays(1, &vao); glDeleteProgram(program);
    glDeleteShader(vs); glDeleteShader(fs);
    assert(glGetError() == GL_NO_ERROR);
    assert(SDL_GL_DestroyContext(context)); SDL_DestroyWindow(window); SDL_Quit();
    puts("capture SDL offscreen GL test passed: shader draw, 4096 exact RGB pixels, production receipt");
    return 0;
}

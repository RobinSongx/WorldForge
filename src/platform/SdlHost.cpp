#include "platform/SdlHost.h"
#include <glad/gl.h>
#include <stdexcept>
#include <algorithm>

namespace wf {
SdlHost::SdlHost(bool hidden) {
    if(!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    try {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,5);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
        const auto flags=SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY|(hidden?SDL_WINDOW_HIDDEN:0);
        window_=SDL_CreateWindow("WorldForge | Studio",1440,900,flags);
        if(!window_) throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window_,900,600);
        context_=SDL_GL_CreateContext(window_);
        if(!context_) throw std::runtime_error(SDL_GetError());
        if(!SDL_GL_MakeCurrent(window_,context_)) throw std::runtime_error(SDL_GetError());
        if(!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) || !GLAD_GL_VERSION_4_5)
            throw std::runtime_error("OpenGL 4.5 is required. Update your graphics driver.");
        SDL_GL_SetSwapInterval(hidden?0:1);
    } catch(...) {
        if(context_) SDL_GL_DestroyContext(context_);
        if(window_) SDL_DestroyWindow(window_);
        SDL_Quit(); throw;
    }
}
SdlHost::~SdlHost() { if(context_) SDL_GL_DestroyContext(context_); if(window_) SDL_DestroyWindow(window_); SDL_Quit(); }
float SdlHost::scale() const { return std::max(SDL_GetWindowDisplayScale(window_),1.0f); }
void SdlHost::swap() { SDL_GL_SwapWindow(window_); }
std::string SdlHost::gpuDescription() const {
    return std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))) + " | GL " + reinterpret_cast<const char*>(glGetString(GL_VERSION));
}
}

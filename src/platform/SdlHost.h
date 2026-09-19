#pragma once
#include <SDL3/SDL.h>
#include <string>
namespace wf {
class SdlHost {
public:
    explicit SdlHost(bool hidden);
    ~SdlHost();
    SdlHost(const SdlHost&)=delete;
    SdlHost& operator=(const SdlHost&)=delete;
    SDL_Window* window() const { return window_; }
    SDL_GLContext context() const { return context_; }
    float scale() const;
    void swap();
    std::string gpuDescription() const;
private:
    SDL_Window* window_{};
    SDL_GLContext context_{};
};
}

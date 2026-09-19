#include "studio/StudioApp.h"
#include "platform/SdlHost.h"
#include "render/ViewportRenderer.h"
#include "ui/StudioPanels.h"
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <glad/gl.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace wf {
namespace {
struct ImGuiLifetime {
    bool platform{},renderer{};
    ~ImGuiLifetime(){if(renderer)ImGui_ImplOpenGL3_Shutdown();if(platform)ImGui_ImplSDL3_Shutdown();ImGui::DestroyContext();}
};
void screenshot(SDL_Window* window,const std::filesystem::path& path){
    int w{},h{};SDL_GetWindowSizeInPixels(window,&w,&h);
    std::vector<unsigned char> pixels(static_cast<size_t>(w)*h*4),flipped(pixels.size());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    for(int y=0;y<h;++y)std::copy_n(pixels.data()+static_cast<size_t>(h-1-y)*w*4,static_cast<size_t>(w)*4,flipped.data()+static_cast<size_t>(y)*w*4);
    auto* surface=SDL_CreateSurfaceFrom(w,h,SDL_PIXELFORMAT_RGBA32,flipped.data(),w*4);
    if(!surface)throw std::runtime_error(SDL_GetError());
    const bool saved=SDL_SaveBMP(surface,path.string().c_str());SDL_DestroySurface(surface);
    if(!saved)throw std::runtime_error(SDL_GetError());
}
}
int runStudio(bool smokeTest,const std::filesystem::path& output){
    SdlHost host(smokeTest);
    std::string settingsPath;
    IMGUI_CHECKVERSION();ImGui::CreateContext();ImGuiLifetime ui;
    auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_NavEnableKeyboard;
    if(smokeTest){io.IniFilename=nullptr;std::filesystem::create_directories(output);}
    else {
        char* pref=SDL_GetPrefPath("WorldForge","Studio");
        if(pref){settingsPath=std::string(pref)+"layout.ini";SDL_free(pref);io.IniFilename=settingsPath.c_str();}
        else io.IniFilename=nullptr;
    }
    char* windowsDir{}; size_t windowsDirSize{};
    _dupenv_s(&windowsDir,&windowsDirSize,"WINDIR");
    if(windowsDir){
        auto font=std::filesystem::path(windowsDir)/"Fonts"/"segoeui.ttf";
        if(std::filesystem::exists(font))io.FontDefault=io.Fonts->AddFontFromFileTTF(font.string().c_str(),14.0f);
    }
    std::free(windowsDir);
    applyTheme(host.scale());
    if(!ImGui_ImplSDL3_InitForOpenGL(host.window(),host.context()))throw std::runtime_error("ImGui SDL initialization failed");ui.platform=true;
    if(!ImGui_ImplOpenGL3_Init("#version 450 core"))throw std::runtime_error("ImGui OpenGL initialization failed");ui.renderer=true;
    ViewportRenderer renderer;EditorState editor;PanelState panels;
    const auto gpu=host.gpuDescription();editor.log("INFO","WorldForge Studio " WF_VERSION " / Windows framework");editor.log("INFO",gpu);
    editor.log("INFO","OpenGL viewport ready. Right-drag to orbit; middle-drag to pan.");
    unsigned int frame{};bool running=true,resizeVerified=false,transformVerified=false,contentVerified=false,cameraInputVerified=false;int initialWidth{};float yawBeforeInput{};
    while(running&&!panels.quitRequested){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            ImGui_ImplSDL3_ProcessEvent(&event);
            if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED&&event.window.windowID==SDL_GetWindowID(host.window())))running=false;
            if(event.type==SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)applyTheme(host.scale());
        }
        if(!running)break;
        if(!smokeTest&&(SDL_GetWindowFlags(host.window())&SDL_WINDOW_MINIMIZED)){SDL_Delay(30);continue;}
        if(smokeTest&&frame==8){initialWidth=renderer.width();SDL_SetWindowSize(host.window(),1200,780);}
        if(smokeTest&&frame==15){
            auto id=editor.addCube();Transform t;t.position={1.6f,.5f,0};t.rotationDegrees.y=25;transformVerified=editor.setTransform(id,t);editor.camera.orbit(20,5);editor.camera.zoom(.3f);
        }
        if(smokeTest&&frame==22){resizeVerified=renderer.width()!=initialWidth;SDL_SetWindowSize(host.window(),1440,900);}
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL3_NewFrame();
        if(smokeTest && frame>=25 && frame<=28){
            io.AddFocusEvent(true);
            io.AddMousePosEvent(panels.imageX+200+(frame>=26?30.0f:0.0f),panels.imageY+150);
            if(frame==25){yawBeforeInput=editor.camera.yaw;io.AddMouseButtonEvent(1,true);}
            if(frame==27)io.AddMouseButtonEvent(1,false);
        }
        ImGui::NewFrame();
        drawStudio(editor,renderer,panels,gpu);
        if(smokeTest&&frame==28)cameraInputVerified=editor.camera.yaw!=yawBeforeInput;
        ImGui::Render();
        int width{},height{};SDL_GetWindowSizeInPixels(host.window(),&width,&height);
        glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,width,height);glClearColor(.04f,.05f,.06f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if(smokeTest&&frame==35){
            contentVerified=renderer.hasRenderedContent();screenshot(host.window(),output/"studio.bmp");
            const auto error=glGetError();const bool success=contentVerified&&resizeVerified&&transformVerified&&cameraInputVerified&&panels.panelsDrawn==6&&error==GL_NO_ERROR;
            std::ofstream report(output/"report.json");
            report<<"{\n  \"success\": "<<(success?"true":"false")<<",\n  \"frames\": "<<frame+1<<",\n  \"viewportContent\": "<<(contentVerified?"true":"false")<<",\n  \"resize\": "<<(resizeVerified?"true":"false")<<",\n  \"transform\": "<<(transformVerified?"true":"false")<<",\n  \"cameraInput\": "<<(cameraInputVerified?"true":"false")<<",\n  \"panels\": "<<panels.panelsDrawn<<",\n  \"glError\": "<<error<<"\n}\n";
            return success?0:2;
        }
        host.swap();++frame;
    }
    return 0;
}
}

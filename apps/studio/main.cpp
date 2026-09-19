#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "studio/StudioApp.h"
#include <filesystem>
#include <fstream>
#include <string_view>
#include <exception>

int main(int argc,char** argv){
    const bool smoke=argc>1&&std::string_view(argv[1])=="--smoke-test";
    const auto output=smoke&&argc>2?std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(argv[2]))):std::filesystem::path("artifacts/smoke");
    try { return wf::runStudio(smoke,output); }
    catch(const std::exception& e){
        if(smoke){std::filesystem::create_directories(output);std::ofstream(output/"error.txt")<<e.what();}
        else SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"WorldForge could not start",e.what(),nullptr);
        return 1;
    }
}

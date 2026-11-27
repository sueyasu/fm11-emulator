#pragma once
#ifdef FM11_SETTINGS_GUI
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "ImGuiFileDialog.h"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

// A genuine, standalone SDL window with its own ImGui context, not an
// ImGui child/modal window within the CRT or Settings window. Nonblocking:
// the frontend continues to poll events, render and run guest workers.
class FilePickerGui {
public:
    using SelectionFn=std::function<void(const std::string&)>;
    FilePickerGui() {
        window_=SDL_CreateWindow("FM-11 - Choose file",
            SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1024,740,
            SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
        if(!window_)throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window_,850,620);
        renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_ACCELERATED);
        if(!renderer_)renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_SOFTWARE);
        if(!renderer_)throw std::runtime_error(SDL_GetError());
        context_=ImGui::CreateContext();
        ImGui::SetCurrentContext(context_);
        ImGui::StyleColorsLight();
        auto& io=ImGui::GetIO();
        io.IniFilename=nullptr;
        ImFontConfig font{};
        font.SizePixels=20.0f;
        io.Fonts->AddFontDefault(&font);
        ImGui::GetStyle().ScaleAllSizes(1.2f);
        if(!ImGui_ImplSDL2_InitForSDLRenderer(window_,renderer_) ||
           !ImGui_ImplSDLRenderer2_Init(renderer_))
            throw std::runtime_error("File picker ImGui initialization failed");
    }
    ~FilePickerGui() {
        if(context_) {
            ImGui::SetCurrentContext(context_);
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext(context_);
        }
        if(renderer_)SDL_DestroyRenderer(renderer_);
        if(window_)SDL_DestroyWindow(window_);
    }
    FilePickerGui(const FilePickerGui&)=delete;
    FilePickerGui& operator=(const FilePickerGui&)=delete;
    bool open(const std::string& title,const std::string& selected,
              SelectionFn accept,const char* filters="Disk images{.d88,.raw,.img,.nhd},.*") {
        if(active_) {
            SDL_RaiseWindow(window_);
            return false;
        }
        accept_=std::move(accept);
        active_=true;
        ImGui::SetCurrentContext(context_);
        const std::filesystem::path current(selected);
        IGFD::FileDialogConfig config;
        config.path=current.has_parent_path()?current.parent_path().string():".";
        if(current.has_filename())config.fileName=current.filename().string();
        config.countSelectionMax=1;
        // No ImGui modal flag: the OS-level SDL window is already independent.
        picker_.OpenDialog("FM11IndependentFilePicker",title,filters,config);
        SDL_SetWindowTitle(window_,title.c_str());
        SDL_SetWindowSize(window_,1024,740);
        SDL_ShowWindow(window_);
        SDL_RaiseWindow(window_);
        return true;
    }
    bool processEvent(const SDL_Event& event) {
        if(!active_)return false;
        Uint32 id=0;
        switch(event.type) {
        case SDL_WINDOWEVENT:id=event.window.windowID;break;
        case SDL_KEYDOWN:case SDL_KEYUP:id=event.key.windowID;break;
        case SDL_TEXTINPUT:id=event.text.windowID;break;
        case SDL_TEXTEDITING:id=event.edit.windowID;break;
        case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:id=event.button.windowID;break;
        case SDL_MOUSEMOTION:id=event.motion.windowID;break;
        case SDL_MOUSEWHEEL:id=event.wheel.windowID;break;
        default:break;
        }
        if(id!=SDL_GetWindowID(window_))return false;
        if(event.type==SDL_WINDOWEVENT && event.window.event==SDL_WINDOWEVENT_CLOSE) {
            close();
            return true;
        }
        ImGui::SetCurrentContext(context_);
        ImGui_ImplSDL2_ProcessEvent(&event);
        return true;
    }
    void render() {
        if(!active_)return;
        ImGui::SetCurrentContext(context_);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        int width=0,height=0;
        SDL_GetWindowSize(window_,&width,&height);
        // ImGuiFileDialog creates an ordinary ImGui window. Without an
        // explicit position, ImGui places it with a default inset; size
        // constraints alone do not account for that offset, clipping the
        // chooser's bottom/right controls. The SDL window itself supplies
        // the title bar and resize frame, so fill its client area exactly.
        const ImVec2 clientSize{float(std::max(1,width)),
                                float(std::max(1,height))};
        ImGui::SetNextWindowPos(ImVec2{0.0f,0.0f},ImGuiCond_Always);
        ImGui::SetNextWindowSize(clientSize,ImGuiCond_Always);
        constexpr ImGuiWindowFlags flags=ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse;
        if(picker_.Display("FM11IndependentFilePicker",flags,
                           clientSize,clientSize)) {
            if(picker_.IsOk() && accept_) {
                const auto path=picker_.GetFilePathName();
                auto callback=std::move(accept_);
                close();
                callback(path);
            } else close();
        }
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer_,245,246,248,255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),renderer_);
        SDL_RenderPresent(renderer_);
    }
private:
    void close() {
        if(!active_)return;
        ImGui::SetCurrentContext(context_);
        picker_.Close();
        active_=false;
        accept_={};
        SDL_HideWindow(window_);
    }
    SDL_Window* window_=nullptr;
    SDL_Renderer* renderer_=nullptr;
    ImGuiContext* context_=nullptr;
    ImGuiFileDialog picker_;
    SelectionFn accept_;
    bool active_=false;
};
#endif

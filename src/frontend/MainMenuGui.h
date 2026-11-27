#pragma once
#ifdef FM11_SETTINGS_GUI
#include "SettingsGui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <filesystem>
#include <stdexcept>
#include <string>

// Dear ImGui menu drawn over the CRT window, not a second system window.
// Its ImGui context is independent from the settings window's context.
class MainMenuGui {
public:
    static constexpr int menuHeight=28;
    MainMenuGui(SDL_Window* window,SDL_Renderer* renderer,SDL_GLContext gl)
        :window_(window),renderer_(renderer),gl_(gl) {
        IMGUI_CHECKVERSION();
        context_=ImGui::CreateContext();
        ImGui::SetCurrentContext(context_);
        ImGui::GetIO().IniFilename=nullptr;
        ImGui::StyleColorsLight();
        ImGui::GetStyle().WindowRounding=3.0f;
        if(gl_) {
            if(SDL_GL_MakeCurrent(window_,gl_)!=0)
                throw std::runtime_error(SDL_GetError());
            if(!ImGui_ImplSDL2_InitForOpenGL(window_,gl_))
                throw std::runtime_error("ImGui SDL2 OpenGL menu init failed");
            if(!ImGui_ImplOpenGL3_Init("#version 330"))
                throw std::runtime_error("ImGui OpenGL menu init failed");
        } else {
            if(!ImGui_ImplSDL2_InitForSDLRenderer(window_,renderer_))
                throw std::runtime_error("ImGui SDL2 menu init failed");
            if(!ImGui_ImplSDLRenderer2_Init(renderer_))
                throw std::runtime_error("ImGui SDL renderer menu init failed");
        }
    }
    ~MainMenuGui() {
        if(context_) {
            ImGui::SetCurrentContext(context_);
            if(gl_) {
                SDL_GL_MakeCurrent(window_,gl_);
                ImGui_ImplOpenGL3_Shutdown();
            } else ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext(context_);
        }
    }
    MainMenuGui(const MainMenuGui&)=delete;
    MainMenuGui& operator=(const MainMenuGui&)=delete;

    // Called by the SDL/UI thread before dispatching input to the guest.
    bool processEvent(const SDL_Event& event) {
        if((event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) &&
           event.key.windowID!=SDL_GetWindowID(window_))return false;
        if(event.type==SDL_TEXTINPUT && event.text.windowID!=SDL_GetWindowID(window_))
            return false;
        if((event.type==SDL_MOUSEBUTTONDOWN || event.type==SDL_MOUSEBUTTONUP) &&
           event.button.windowID!=SDL_GetWindowID(window_))return false;
        if(event.type==SDL_MOUSEMOTION &&
           event.motion.windowID!=SDL_GetWindowID(window_))return false;
        if(event.type==SDL_MOUSEWHEEL &&
           event.wheel.windowID!=SDL_GetWindowID(window_))return false;
        ImGui::SetCurrentContext(context_);
        const bool capturing=ImGui::GetIO().WantCaptureKeyboard ||
                             ImGui::GetIO().WantTextInput;
        const bool mouseCapture=ImGui::GetIO().WantCaptureMouse;
        ImGui_ImplSDL2_ProcessEvent(&event);
        if(event.type==SDL_MOUSEBUTTONDOWN || event.type==SDL_MOUSEBUTTONUP)
            return event.button.y<menuHeight || mouseCapture;
        if(event.type==SDL_MOUSEWHEEL || event.type==SDL_MOUSEMOTION)
            return mouseCapture;
        if(event.type==SDL_KEYDOWN || event.type==SDL_KEYUP ||
           event.type==SDL_TEXTINPUT)return capturing;
        return false;
    }

    void render(SettingsGui& settings,bool& paused,bool& running) {
        ImGui::SetCurrentContext(context_);
        if(gl_) {
            if(SDL_GL_MakeCurrent(window_,gl_)!=0)
                throw std::runtime_error(SDL_GetError());
            ImGui_ImplOpenGL3_NewFrame();
        } else ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if(ImGui::BeginMainMenuBar()) {
            if(ImGui::BeginMenu("File")) {
                for(unsigned channel=0;channel<2;++channel) {
                    if(ImGui::BeginMenu(channel?"2HD floppy":"2D floppy")) {
                        for(unsigned drive=0;drive<4;++drive) {
                            ImGui::PushID(int(channel*4+drive));
                            const std::string title="Drive "+std::to_string(drive);
                            if(ImGui::BeginMenu(title.c_str())) {
                                if(ImGui::MenuItem("Choose image..."))
                                    pendingChoose_=int(channel*4+drive);
                                if(ImGui::MenuItem("Eject (edit only)"))
                                    pendingEject_=int(channel*4+drive);
                                ImGui::EndMenu();
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::Separator();
                if(ImGui::MenuItem("Exit","Ctrl+F12"))running=false;
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Settings")) {
                if(ImGui::MenuItem("Configuration...","Ctrl+F10"))
                    settings.toggle();
                if(ImGui::MenuItem(paused?"Resume":"Pause","F2"))
                    paused=!paused;
                ImGui::BeginDisabled();
                ImGui::MenuItem("Reset (Stage 4-C)");
                ImGui::EndDisabled();
                ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("Help")) {
                if(ImGui::MenuItem("Keyboard shortcuts"))showKeys_=true;
                if(ImGui::MenuItem("About"))showAbout_=true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        if(showKeys_)ImGui::OpenPopup("Keyboard shortcuts");
        if(ImGui::BeginPopupModal("Keyboard shortcuts",&showKeys_,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Ctrl+F10: settings   F2: pause/resume");
            ImGui::TextUnformatted("F1: 200/400-line view   Ctrl+F12: exit");
            if(ImGui::Button("Close"))showKeys_=false;
            ImGui::EndPopup();
        }
        if(showAbout_)ImGui::OpenPopup("About FM-11 Emulator");
        if(ImGui::BeginPopupModal("About FM-11 Emulator",&showAbout_,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("FM-11 Emulator");
            ImGui::TextUnformatted("Settings GUI: Dear ImGui + ImGuiFileDialog");
            ImGui::TextDisabled("FDD hot-swap: Stage 4-B   Reset: Stage 4-C");
            if(ImGui::Button("Close"))showAbout_=false;
            ImGui::EndPopup();
        }
        // The File menu owns an ImGuiFileDialog in the *CRT* window,
        // instead of opening the settings window as an intermediate step.
        if(pendingChoose_>=0 && activePicker_==-1) {
            activePicker_=pendingChoose_;
            pendingChoose_=-1;
            const unsigned channel=unsigned(activePicker_)/4;
            const unsigned drive=unsigned(activePicker_)%4;
            const auto& target=(channel?settings.edited().floppy2hd:
                settings.edited().floppy2d)[drive];
            const std::filesystem::path current(target);
            IGFD::FileDialogConfig config;
            config.path=current.has_parent_path()?current.parent_path().string():".";
            if(current.has_filename())config.fileName=current.filename().string();
            config.countSelectionMax=1;
            config.flags=ImGuiFileDialogFlags_Modal;
            chooser_.OpenDialog("FM11MainFilePicker",
                channel?"Choose 2HD floppy image":"Choose 2D floppy image",
                "Disk images{.d88,.raw,.img,.nhd},.*",config);
        }
        if(activePicker_>=0 && chooser_.Display("FM11MainFilePicker")) {
            if(chooser_.IsOk()) {
                settings.stageFddPath(unsigned(activePicker_)/4,
                                      unsigned(activePicker_)%4,
                                      chooser_.GetFilePathName());
            }
            chooser_.Close();
            activePicker_=-1;
        }
        ImGui::Render();
        if(gl_) {
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        } else {
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),renderer_);
        }
        // Eject edits only the saved draft; actual FDD removal is Stage 4-B.
        if(pendingEject_>=0) {
            const unsigned index=static_cast<unsigned>(pendingEject_);
            pendingEject_=-1;
            settings.ejectFdd(index/4,index%4);
        }
    }
private:
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_GLContext gl_;
    ImGuiContext* context_=nullptr;
    bool showAbout_=false,showKeys_=false;
    ImGuiFileDialog chooser_;
    int pendingChoose_=-1,pendingEject_=-1,activePicker_=-1;
};
#endif

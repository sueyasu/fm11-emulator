#pragma once
#ifdef FM11_SETTINGS_GUI
#include "MachineConfig.h"
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "ImGuiFileDialog.h"
#include <filesystem>
#include <array>
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

// Stage 4-A: a separate editor window. Editing or saving does not mutate
// the running Machine. Stage 4-B mounts FDDs live; Stage 4-C applies reset
// dependent changes. The editor is owned and polled by the SDL/UI thread.
class SettingsGui {
public:
    explicit SettingsGui(const MachineConfig& effective)
        :original_(effective),edited_(effective) {
        window_=SDL_CreateWindow("FM-11 Emulator - Settings",
            SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,780,650,
            SDL_WINDOW_HIDDEN|SDL_WINDOW_RESIZABLE);
        if(!window_)throw std::runtime_error(std::string("settings window: ")+SDL_GetError());
        renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_ACCELERATED);
        if(!renderer_)renderer_=SDL_CreateRenderer(window_,-1,SDL_RENDERER_SOFTWARE);
        if(!renderer_)throw std::runtime_error(std::string("settings renderer: ")+SDL_GetError());
        IMGUI_CHECKVERSION();
        context_=ImGui::CreateContext();
        ImGui::SetCurrentContext(context_);
        ImGui::StyleColorsDark();
        ImGui::GetIO().IniFilename=nullptr;
        if(!ImGui_ImplSDL2_InitForSDLRenderer(window_,renderer_))
            throw std::runtime_error("ImGui SDL2 init failed");
        if(!ImGui_ImplSDLRenderer2_Init(renderer_))
            throw std::runtime_error("ImGui SDLRenderer2 init failed");
    }
    ~SettingsGui() {
        if(context_) {
            ImGui::SetCurrentContext(context_);
            ImGui_ImplSDLRenderer2_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext(context_);
        }
        if(renderer_)SDL_DestroyRenderer(renderer_);
        if(window_)SDL_DestroyWindow(window_);
    }
    SettingsGui(const SettingsGui&)=delete;
    SettingsGui& operator=(const SettingsGui&)=delete;

    // File pickers are rendered by ImGui and never block the SDL event loop.
    void chooseFdd(unsigned channel,unsigned drive) {
        if(channel>1 || drive>3)return;
        std::string& target=channel?edited_.floppy2hd[drive]:edited_.floppy2d[drive];
        chooseFile(&target,channel?"Choose 2HD floppy image":"Choose 2D floppy image",
                   "Disk images{.d88,.raw,.img,.nhd},.*");
    }
    void ejectFdd(unsigned channel,unsigned drive) {
        if(channel>1 || drive>3)return;
        (channel?edited_.floppy2hd:edited_.floppy2d)[drive].clear();
        status_="FDD image removed from edited configuration (not mounted until Stage 4-B).";
    }
    bool visible() const {return visible_;}
    void toggle() {
        visible_=!visible_;
        if(visible_) {
            SDL_ShowWindow(window_);
            SDL_RaiseWindow(window_);
        } else SDL_HideWindow(window_);
    }
    // True means the event belongs to this window and must not be forwarded
    // to the emulated keyboard or guest input queue.
    bool processEvent(const SDL_Event& event) {
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
        if(event.type==SDL_WINDOWEVENT &&
           event.window.event==SDL_WINDOWEVENT_CLOSE) {
            toggle();
            return true;
        }
        ImGui::SetCurrentContext(context_);
        ImGui_ImplSDL2_ProcessEvent(&event);
        return true;
    }
    void render() {
        if(!visible_)return;
        ImGui::SetCurrentContext(context_);
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0,0),ImGuiCond_Always);
        int w=0,h=0;
        SDL_GetWindowSize(window_,&w,&h);
        ImGui::SetNextWindowSize(ImVec2(float(w),float(h)),ImGuiCond_Always);
        ImGui::Begin("Settings",nullptr,ImGuiWindowFlags_NoResize|
            ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse|
            ImGuiWindowFlags_NoTitleBar);
        ImGui::TextUnformatted("FM-11 Emulator settings (Stage 4-A)");
        ImGui::TextWrapped("Changes are saved to the user config file. "
            "The running machine is not changed until a future reset.");
        ImGui::Separator();
        if(ImGui::BeginTabBar("configuration")) {
            if(ImGui::BeginTabItem("Disks")) {disks();ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("CPU / RAM")) {cpus();ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("ROM / Display")) {roms();ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("Serial")) {serial();ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("DIP switches")) {dips();ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("Timing")) {timing();ImGui::EndTabItem();}
            ImGui::EndTabBar();
        }
        ImGui::Separator();
        ImGui::TextWrapped("Save destination: %s",userMachineConfigPath().c_str());
        ImGui::TextDisabled("Loaded with --config: it still overrides the saved user file on next start.");
        if(ImGui::Button("Save user configuration")) {
            try {
                saveUserMachineConfig(edited_);
                status_="Saved to "+userMachineConfigPath();
            } catch(const std::exception& e) {
                status_=std::string("Save failed: ")+e.what();
            }
        }
        ImGui::SameLine();
        if(ImGui::Button("Discard edits")) {
            edited_=original_;
            status_="Discarded changes";
        }
        ImGui::SameLine();
        if(ImGui::Button("Close"))toggle();
        ImGui::TextWrapped("%s",status_.c_str());
        ImGui::TextDisabled("FDD live swapping: Stage 4-B | Reset/apply: Stage 4-C");
        ImGui::End();
        // Display once each SDL frame, including after the settings window
        // has been opened from the main CRT menu.
        if(fileTarget_ && ImGuiFileDialog::Instance()->Display("FM11ChooseFile")) {
            if(ImGuiFileDialog::Instance()->IsOk()) {
                *fileTarget_=ImGuiFileDialog::Instance()->GetFilePathName();
                status_="Selected "+*fileTarget_+" (saved only when Save is clicked)";
            }
            ImGuiFileDialog::Instance()->Close();
            fileTarget_=nullptr;
        }
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer_,25,27,32,255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),renderer_);
        SDL_RenderPresent(renderer_);
    }
    const MachineConfig& edited() const {return edited_;}
private:
    void chooseFile(std::string* target,const char* title,const char* filters) {
        if(!visible_)toggle();
        if(fileTarget_)return;
        fileTarget_=target;
        ImGui::SetCurrentContext(context_);
        IGFD::FileDialogConfig config;
        const std::filesystem::path current(*target);
        config.path=current.has_parent_path()?current.parent_path().string():".";
        if(current.has_filename())config.fileName=current.filename().string();
        config.countSelectionMax=1;
        config.flags=ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog("FM11ChooseFile",title,filters,config);
    }
    void fileEntry(const char* label,std::string& value,const char* filter) {
        editText(label,value);
        ImGui::SameLine();
        ImGui::PushID(label);
        if(ImGui::Button("Browse"))chooseFile(&value,label,filter);
        ImGui::PopID();
    }
    static bool editText(const char* label,std::string& value) {
        std::array<char,4096> input{};
        const auto size=std::min(value.size(),input.size()-1);
        std::memcpy(input.data(),value.data(),size);
        if(!ImGui::InputText(label,input.data(),input.size()))return false;
        value=input.data();
        return true;
    }
    static void editClock(const char* label,std::uint64_t& clock) {
        ImGui::InputScalar(label,ImGuiDataType_U64,&clock);
    }
    static void range(const char* label,unsigned& v,unsigned low,unsigned high) {
        int temp=static_cast<int>(v);
        if(ImGui::SliderInt(label,&temp,int(low),int(high)))v=unsigned(temp);
    }
    void disks() {
        ImGui::TextUnformatted("FDD image paths (saved only in 4-A; live mount in 4-B)");
        for(unsigned i=0;i<4;++i) {
            const std::string label="2D FDD "+std::to_string(i);
            fileEntry(label.c_str(),edited_.floppy2d[i],
                      "Disk images{.d88,.raw,.img,.nhd},.*");
        }
        ImGui::Separator();
        for(unsigned i=0;i<4;++i) {
            const std::string label="2HD FDD "+std::to_string(i);
            fileEntry(label.c_str(),edited_.floppy2hd[i],
                      "Disk images{.d88,.raw,.img,.nhd},.*");
        }
        ImGui::Separator();
        fileEntry("HDD image",edited_.hddImage,
                  "Disk images{.d88,.raw,.img,.nhd},.*");
    }
    void cpus() {
        ImGui::Checkbox("6809 card installed",&edited_.main6809Installed);
        int cpu=edited_.main6809Cpu==CpuType::HD6309?1:0;
        if(ImGui::Combo("6809 card CPU",&cpu,"MC6809\0HD6309\0"))
            edited_.main6809Cpu=cpu?CpuType::HD6309:CpuType::MC6809;
        editClock("6809 clock (Hz)",edited_.main6809ClockHz);
        ImGui::Separator();
        ImGui::Checkbox("8088 card installed",&edited_.main8088Installed);
        cpu=edited_.main8088Cpu==CpuType::NECV20?1:0;
        if(ImGui::Combo("8088 card CPU",&cpu,"8088\0NEC V20\0"))
            edited_.main8088Cpu=cpu?CpuType::NECV20:CpuType::I8088;
        editClock("8088 clock (Hz)",edited_.main8088ClockHz);
        ImGui::Separator();
        ImGui::Checkbox("Z80 installed",&edited_.z80Installed);
        editClock("Z80 clock (Hz)",edited_.z80ClockHz);
        int ram=int(edited_.ramKiB);
        if(ImGui::SliderInt("RAM (KiB)",&ram,128,1024,"%d",ImGuiSliderFlags_AlwaysClamp)) {
            edited_.ramKiB=std::size_t(((ram+64)/128)*128);
            edited_.ramKiB=std::min<std::size_t>(1024,edited_.ramKiB);
        }
    }
    void roms() {
        fileEntry("6809 boot ROM",edited_.main6809Rom,"ROM files{.bin,.rom},.*");
        fileEntry("8088 boot ROM",edited_.main8088Rom,"ROM files{.bin,.rom},.*");
        fileEntry("Display CPU ROM",edited_.displayRom,"ROM files{.bin,.rom},.*");
        fileEntry("ANK ROM",edited_.ankRom,"ROM files{.bin,.rom},.*");
        fileEntry("Kanji ROM",edited_.kanjiRom,"ROM files{.bin,.rom},.*");
        fileEntry("Kanji level 2 ROM",edited_.kanji2Rom,"ROM files{.bin,.rom},.*");
        int cpu=edited_.displayCpu==CpuType::HD6309?1:0;
        if(ImGui::Combo("Display CPU",&cpu,"MC6809\0HD6309\0"))
            edited_.displayCpu=cpu?CpuType::HD6309:CpuType::MC6809;
        editClock("Display CPU clock (Hz)",edited_.displayClockHz);
        int height=int(edited_.displayHeight);
        if(ImGui::RadioButton("200-line initial view",height==200))edited_.displayHeight=200;
        ImGui::SameLine();
        if(ImGui::RadioButton("400-line initial view",height==400))edited_.displayHeight=400;
    }
    void serial() {
        editText("Serial0: pty, none or /dev/...",edited_.serial0);
        ImGui::Checkbox("RTS/CTS flow control",&edited_.serialRtsCts);
        ImGui::Checkbox("Clocked serial",&edited_.serialClocked);
    }
    void dips() {
        bool selected6809=edited_.mainSelected==MachineConfig::MainSelection::Cpu6809;
        if(ImGui::RadioButton("6809 boot",selected6809))
            edited_.mainSelected=MachineConfig::MainSelection::Cpu6809;
        ImGui::SameLine();
        if(ImGui::RadioButton("8088 boot",!selected6809))
            edited_.mainSelected=MachineConfig::MainSelection::Cpu8088;
        ImGui::Checkbox("Boot from 2D FDD",&edited_.boot2dFdd);
        ImGui::Checkbox("Boot from 2HD FDD",&edited_.boot2hdFdd);
        ImGui::Checkbox("Boot from HDD",&edited_.bootHdd);
        range("Boot drive",edited_.dipBootDrive,0,3);
        ImGui::Checkbox("Bubble boot",&edited_.dipBootBubble);
        bool cd=edited_.dipSerialCd==MachineConfig::SerialCd::CdAndDsr;
        if(ImGui::Checkbox("Require CD/DSR",&cd))
            edited_.dipSerialCd=cd?MachineConfig::SerialCd::CdAndDsr:MachineConfig::SerialCd::Ignore;
        bool sync=edited_.dipSerialMode==MachineConfig::SerialMode::Sync;
        if(ImGui::Checkbox("Synchronous serial DIP",&sync))
            edited_.dipSerialMode=sync?MachineConfig::SerialMode::Sync:MachineConfig::SerialMode::Async;
        range("Serial DIP baud bits (0..31)",edited_.dipSerialBaudSwitches,0,31);
        if(ImGui::RadioButton("DIP 200 display lines",edited_.dipSystemDisplayLines==200))
            edited_.dipSystemDisplayLines=200;
        ImGui::SameLine();
        if(ImGui::RadioButton("DIP 400 display lines",edited_.dipSystemDisplayLines==400))
            edited_.dipSystemDisplayLines=400;
    }
    void timing() {
        ImGui::Checkbox("Timed FDD operations",&edited_.floppyTimed);
        ImGui::Checkbox("Timed HDD operations",&edited_.hddTimed);
        ImGui::Checkbox("Clocked serial transfers",&edited_.serialClocked);
    }
    SDL_Window* window_=nullptr;
    SDL_Renderer* renderer_=nullptr;
    ImGuiContext* context_=nullptr;
    bool visible_=false;
    MachineConfig original_;
    MachineConfig edited_;
    std::string status_;
    std::string* fileTarget_=nullptr;
};
#endif

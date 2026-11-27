#pragma once
#ifdef FM11_SETTINGS_GUI
#include "MachineConfig.h"
#include <SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "FilePickerGui.h"
#include <filesystem>
#include <iostream>
#include <array>
#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
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
        ImGui::StyleColorsLight();
        auto& io=ImGui::GetIO();
        io.IniFilename=nullptr;
        ImFontConfig font{};
        font.SizePixels=19.0f;
        io.Fonts->AddFontDefault(&font);
        ImGui::GetStyle().ScaleAllSizes(1.2f);
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

    void setFilePicker(FilePickerGui* picker) {filePicker_=picker;}
    struct FddRequest {
        unsigned channel=0,drive=0;
        std::string path;
    };
    void stageFddPath(unsigned channel,unsigned drive,const std::string& path) {
        if(channel>1 || drive>3)return;
        pendingFdd_=FddRequest{channel,drive,path};
    }
    std::optional<std::string> takeFddError() {
        auto error=std::move(fddError_);
        fddError_.reset();
        return error;
    }
    std::optional<FddRequest> takeFddRequest() {
        auto request=std::move(pendingFdd_);
        pendingFdd_.reset();
        return request;
    }
    void finishFddRequest(const FddRequest& request,const std::string& error) {
        if(!error.empty()) {
            status_="FDD exchange failed: "+error;
            fddError_=status_;
            std::cerr<<status_<<"\n";
            return;
        }
        auto& slot=(request.channel?edited_.floppy2hd:edited_.floppy2d)[request.drive];
        slot=request.path;
        (request.channel?original_.floppy2hd:original_.floppy2d)[request.drive]=request.path;
        status_=std::string(request.path.empty()?"Ejected":"Mounted")+
            " FDD"+std::to_string(request.drive)+": "+
            (request.path.empty()?"(empty)":request.path);
    }
    void stageHddPath(const std::string& path) {
        edited_.hddImage=path;
        status_="HDD image path selected (applies after Stage 4-C reset): "+path;
    }
    void ejectFdd(unsigned channel,unsigned drive) {
        stageFddPath(channel,drive,{});
    }
    enum class ResetRequest { None, Reset, ApplyAndReset };
    void requestReset(bool apply) {
        resetRequest_=apply?ResetRequest::ApplyAndReset:ResetRequest::Reset;
    }
    ResetRequest takeResetRequest() {
        const auto request=resetRequest_;
        resetRequest_=ResetRequest::None;
        return request;
    }
    std::optional<std::string> takeResetError() {
        auto result=std::move(resetError_);
        resetError_.reset();
        return result;
    }
    void resetResult(const std::string& error,bool applied) {
        if(!error.empty()) {
            status_="Reset failed: "+error;
            resetError_=status_;
            std::cerr<<status_<<"\n";
            return;
        }
        if(applied)original_=edited_;
        status_=applied?"Applied configuration and reset the machine":
                        "Reset the machine using its current configuration";
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
        if(ImGui::Button("Apply && Reset"))requestReset(true);
        ImGui::SameLine();
        if(ImGui::Button("Reset only"))requestReset(false);
        ImGui::Separator();
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
        ImGui::TextDisabled("Apply && Reset does not save the user configuration file.");
        ImGui::End();
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer_,25,27,32,255);
        SDL_RenderClear(renderer_);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),renderer_);
        SDL_RenderPresent(renderer_);
    }
    const MachineConfig& edited() const {return edited_;}
private:
    void chooseFile(std::string* target,const char* title,const char* filters) {
        if(!filePicker_ || !target)return;
        filePicker_->open(title,*target,
            [this,target](const std::string& path) {
                *target=path;
                status_="Selected "+path+" (save to persist)";
            },filters);
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
    void cpus() {
        ImGui::Checkbox("6809 card installed",&edited_.main6809Installed);
        ImGui::TextUnformatted("6809 card CPU:");
        if(ImGui::RadioButton("MC6809",edited_.main6809Cpu==CpuType::MC6809))
            edited_.main6809Cpu=CpuType::MC6809;
        ImGui::SameLine();
        if(ImGui::RadioButton("HD6309",edited_.main6809Cpu==CpuType::HD6309))
            edited_.main6809Cpu=CpuType::HD6309;
        editClock("6809 clock (Hz)",edited_.main6809ClockHz);
        ImGui::Separator();
        ImGui::Checkbox("8088 card installed",&edited_.main8088Installed);
        ImGui::TextUnformatted("8088 card CPU:");
        if(ImGui::RadioButton("8088",edited_.main8088Cpu==CpuType::I8088))
            edited_.main8088Cpu=CpuType::I8088;
        ImGui::SameLine();
        if(ImGui::RadioButton("NEC V20",edited_.main8088Cpu==CpuType::NECV20))
            edited_.main8088Cpu=CpuType::NECV20;
        editClock("8088 clock (Hz)",edited_.main8088ClockHz);
        ImGui::Separator();
        ImGui::Checkbox("Z80 installed",&edited_.z80Installed);
        editClock("Z80 clock (Hz)",edited_.z80ClockHz);
        ImGui::TextUnformatted("RAM capacity (128 KiB steps):");
        int step=int(edited_.ramKiB/128)-1;
        if(ImGui::Combo("RAM (KiB)",&step,
                        "128 KiB\0" "256 KiB\0" "384 KiB\0" "512 KiB\0"
                        "640 KiB\0" "768 KiB\0" "896 KiB\0" "1024 KiB\0"))
            edited_.ramKiB=std::size_t(step+1)*128;
    }
    void roms() {
        fileEntry("HDD image",edited_.hddImage,"Disk images{.d88,.raw,.img,.nhd},.*");
        ImGui::Separator();
        fileEntry("6809 boot ROM",edited_.main6809Rom,"ROM files{.bin,.rom},.*");
        fileEntry("8088 boot ROM",edited_.main8088Rom,"ROM files{.bin,.rom},.*");
        fileEntry("Display CPU ROM",edited_.displayRom,"ROM files{.bin,.rom},.*");
        fileEntry("ANK ROM",edited_.ankRom,"ROM files{.bin,.rom},.*");
        fileEntry("Kanji ROM",edited_.kanjiRom,"ROM files{.bin,.rom},.*");
        fileEntry("Kanji level 2 ROM",edited_.kanji2Rom,"ROM files{.bin,.rom},.*");
        ImGui::TextUnformatted("Display CPU:");
        if(ImGui::RadioButton("MC6809##display",edited_.displayCpu==CpuType::MC6809))
            edited_.displayCpu=CpuType::MC6809;
        ImGui::SameLine();
        if(ImGui::RadioButton("HD6309##display",edited_.displayCpu==CpuType::HD6309))
            edited_.displayCpu=CpuType::HD6309;
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
    FilePickerGui* filePicker_=nullptr;
    std::optional<FddRequest> pendingFdd_;
    std::optional<std::string> fddError_;
    ResetRequest resetRequest_=ResetRequest::None;
    std::optional<std::string> resetError_;
};
#endif

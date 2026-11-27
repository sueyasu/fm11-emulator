#include "PendingMachineSettings.h"
#include <stdexcept>
#include <string>

// Stage 4-D integration regression for the real SettingsGui state model.
// Requires no display server, keyboard focus or external disk images.
int main() {
    MachineConfig boot;
    boot.ramKiB=256;
    boot.floppy2d[0]="boot-a.d88";
    boot.floppy2hd[2]="hd-old.d88";
    PendingMachineSettings state(boot);
    if(state.hasPendingReset() || state.applied().ramKiB!=256)return 1;

    MachineConfig editor=boot;
    editor.ramKiB=512;
    editor.main6809Cpu=CpuType::HD6309;
    editor.hddImage="disk1.img";
    // Close without Apply must never affect next Reset.
    state.discardEdits(editor);
    if(editor.ramKiB!=256 || editor.main6809Cpu!=CpuType::MC6809 ||
       !editor.hddImage.empty() || state.hasPendingReset())return 2;

    editor.ramKiB=512;
    editor.main6809Cpu=CpuType::HD6309;
    editor.hddImage="disk1.img";
    state.apply(editor);
    if(!state.hasPendingReset() || state.applied().ramKiB!=512 ||
       state.applied().main6809Cpu!=CpuType::HD6309 ||
       state.applied().hddImage!="disk1.img")return 3;
    // Applying changes the next boot only; the independently running
    // configuration is still the original instance.
    if(boot.ramKiB!=256 || boot.main6809Cpu!=CpuType::MC6809)return 4;

    // A second editor session can be closed without erasing the first Apply.
    editor.ramKiB=768;
    editor.hddImage="unsaved.img";
    state.discardEdits(editor);
    if(editor.ramKiB!=512 || editor.hddImage!="disk1.img" ||
       !state.hasPendingReset())return 5;

    // Reject invalid Apply without replacing previously staged settings.
    editor.ramKiB=192;
    try {state.apply(editor);return 6;}
    catch(const std::invalid_argument&) {}
    if(state.applied().ramKiB!=512 || !state.hasPendingReset())return 7;
    state.discardEdits(editor);

    // Reset that fails to boot the staged configuration keeps it for retry.
    state.resetSucceeded(false);
    if(!state.hasPendingReset())return 8;
    state.resetSucceeded(true);
    if(state.hasPendingReset() || state.applied().ramKiB!=512)return 9;

    // Live FDD changes must reach the actual next-reset image, including
    // after Apply has already staged unrelated CPU/RAM changes.
    editor.ramKiB=640;
    state.apply(editor);
    state.setMountedFdd(0,0,"live-b.d88",editor);
    state.setMountedFdd(1,2,"hd-new.d88",editor);
    if(editor.floppy2d[0]!="live-b.d88" ||
       state.applied().floppy2d[0]!="live-b.d88" ||
       state.applied().floppy2hd[2]!="hd-new.d88" ||
       state.applied().ramKiB!=640 ||
       !state.hasPendingReset())return 10;
    editor.ramKiB=1024;
    state.discardEdits(editor);
    if(editor.ramKiB!=640 || editor.floppy2d[0]!="live-b.d88")return 11;

    // Eject takes effect immediately but does not erase staged CPU settings.
    state.setMountedFdd(0,0,"",editor);
    if(!editor.floppy2d[0].empty() ||
       !state.applied().floppy2d[0].empty() ||
       state.applied().ramKiB!=640)return 12;

    try {state.setMountedFdd(2,0,"bad",editor);return 13;}
    catch(const std::out_of_range&) {}
    try {state.setMountedFdd(0,4,"bad",editor);return 14;}
    catch(const std::out_of_range&) {}
    if(!state.hasPendingReset())return 15;
    state.resetSucceeded(true);
    if(state.hasPendingReset())return 16;
    return 0;
}

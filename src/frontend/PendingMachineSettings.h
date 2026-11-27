#pragma once

#include "MachineConfig.h"
#include <stdexcept>
#include <utility>

// Stage 4-D: independent, headless-testable state for the Settings/Reset UI.
// Editing, applying, booting and saving are deliberately different actions.
// This model never manipulates a running Machine or writes a config file.
class PendingMachineSettings {
public:
    explicit PendingMachineSettings(MachineConfig initial)
        : applied_(std::move(initial)) {}

    // Validate first, then atomically stage for the NEXT Reset.
    // Failure must preserve the prior applied configuration.
    void apply(const MachineConfig& edited) {
        (void)serializeMachineConfig(edited);
        MachineConfig next=edited;
        applied_=std::move(next);
        pendingReset_=true;
    }

    const MachineConfig& applied() const noexcept {return applied_;}
    bool hasPendingReset() const noexcept {return pendingReset_;}

    // Cancel/close discards only the uncommitted form contents.
    void discardEdits(MachineConfig& edited) const {edited=applied_;}

    // Only call after successful construction/reset of the new Machine.
    // A failed reset must retain the staged configuration for retry.
    void resetSucceeded(bool usedAppliedSettings) noexcept {
        if(usedAppliedSettings)pendingReset_=false;
    }

    // A live FDD exchange is NOT a Reset or Apply. It is immediately
    // reflected in both the editor and the staged machine configuration;
    // other pending hardware edits are unaffected.
    void setMountedFdd(unsigned channel,unsigned drive,
                       const std::string& path,MachineConfig& edited) {
        if(channel>1 || drive>3)
            throw std::out_of_range("FDD channel or drive outside 0..3");
        auto& staged=channel?applied_.floppy2hd:applied_.floppy2d;
        auto& form=channel?edited.floppy2hd:edited.floppy2d;
        staged[drive]=path;
        form[drive]=path;
    }

private:
    MachineConfig applied_;
    bool pendingReset_=false;
};

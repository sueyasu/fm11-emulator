#pragma once
#include "portable_types.h"
#include <deque>

// Main-board USART model for the headless serial port at FD06/FD07.
// Frontend transport is external; both main CPU cards see the same queues.
class Usart8251 {
public:
    void reset() { input_.clear(); output_.clear(); command_=0; txIrqEnabled_=true; }
    void setTxIrqEnabled(bool enabled) { txIrqEnabled_=enabled; }
    void receive(BYTE value) { input_.push_back(value); }
    bool takeOutput(BYTE& value) {
        if(output_.empty())return false;
        value=output_.front();output_.pop_front();return true;
    }
    BYTE status() const {return BYTE(0x81 | (input_.empty()?0:0x02));}
    BYTE readData() {
        if(input_.empty())return 0;
        const BYTE result=input_.front();input_.pop_front();return result;
    }
    void writeData(BYTE value) {output_.push_back(value);}
    void writeCommand(BYTE value) {command_=value;}
    bool irqRequest() const {
        return (txIrqEnabled_ && (command_&0x01)!=0) ||
               ((command_&0x04)!=0 && !input_.empty());
    }
private:
    std::deque<BYTE> input_,output_;
    BYTE command_=0;
    bool txIrqEnabled_=true;
};

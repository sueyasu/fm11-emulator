#include "Main6809Board.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

// This is a headless CPU/serial runner, not yet a complete FM-11 IPL.
namespace {
volatile std::sig_atomic_t stopping = 0;
void requestStop(int) { stopping = 1; }

class Terminal {
public:
    Terminal() {
        if (!isatty(STDIN_FILENO)) return;
        if (tcgetattr(STDIN_FILENO, &saved_) != 0) return;
        termios raw = saved_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_iflag &= static_cast<tcflag_t>(~(ICRNL | IXON));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0)
            active_ = true;
    }
    ~Terminal() {
        if (active_) tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
    }
private:
    termios saved_{};
    bool active_ = false;
};

unsigned long long number(const std::string& value) {
    std::size_t end = 0;
    const auto n = std::stoull(value, &end, 0);
    if (end != value.size()) throw std::invalid_argument("invalid numeric argument");
    return n;
}

void loadBinary(Main6809Board& board, const std::string& file,
                unsigned long long origin) {
    if (origin > 0xffff) throw std::invalid_argument("load address out of range");
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("cannot open: " + file);
    const auto count = in.tellg();
    if (count < 0 || static_cast<unsigned long long>(count) > 0x10000 - origin)
        throw std::runtime_error("binary exceeds 64 KiB CPU address space");
    in.seekg(0);
    std::vector<char> bytes(static_cast<std::size_t>(count));
    if (!in.read(bytes.data(), count))
        throw std::runtime_error("cannot read: " + file);
    for (std::size_t i = 0; i < bytes.size(); ++i)
        board.writeCpu(static_cast<WORD>(origin + i),
                       static_cast<BYTE>(bytes[i]));
}

void usage() {
    std::cerr << "usage: fm11-headless --binary FILE --load ADDR --pc ADDR"
                 " [--cpu 6809|6309] [--clock HZ] [--instructions COUNT]\n"
                 "  Raw 6809 binaries only; no disk or NitrOS-9 boot loader yet.\n"
                 "  Ctrl-C stops execution; terminal stdin/stdout connect to FD06.\n";
}
}

int main(int argc, char** argv) {
    try {
        std::string file, cpu = "6809";
        unsigned long long load = 0, pc = 0, hz = 2000000, limit = 0;
        bool hasLoad = false, hasPc = false;
        for (int i=1;i<argc;++i) {
            const std::string a=argv[i];
            if (a=="--help") { usage(); return 0; }
            if (i+1>=argc) { usage(); return 2; }
            const std::string v=argv[++i];
            if (a=="--binary") file=v;
            else if (a=="--cpu") cpu=v;
            else if (a=="--load") {load=number(v);hasLoad=true;}
            else if (a=="--pc") {pc=number(v);hasPc=true;}
            else if (a=="--clock") hz=number(v);
            else if (a=="--instructions") limit=number(v);
            else {usage(); return 2;}
        }
        if (file.empty() || !hasLoad || !hasPc || pc>0xffff ||
            (cpu!="6809" && cpu!="6309")) {usage();return 2;}
        Main6809Board board(cpu=="6809" ? CpuType::MC6809 : CpuType::HD6309);
        Main6809Board::BootRomImage rom{};
        rom[0xffe]=static_cast<BYTE>(pc>>8);
        rom[0xfff]=static_cast<BYTE>(pc);
        board.loadBootRom(rom);
        board.reset();
        board.setCpuClockHz(hz);
        loadBinary(board,file,load);
        Terminal terminal;
        std::signal(SIGINT,requestStop);
        std::signal(SIGTERM,requestStop);
        bool inputOpen=true;
        unsigned long long count=0;
        while (!stopping && (limit==0 || count<limit)) {
            // Nonblocking terminal input; EOF disables further polling.
            if (inputOpen) {
                fd_set fds;
                FD_ZERO(&fds); FD_SET(STDIN_FILENO,&fds);
                timeval timeout{};
                if (select(STDIN_FILENO+1,&fds,nullptr,nullptr,&timeout)>0 &&
                    FD_ISSET(STDIN_FILENO,&fds)) {
                    unsigned char buf[128];
                    const ssize_t n=read(STDIN_FILENO,buf,sizeof(buf));
                    if(n==0) inputOpen=false;
                    else if(n>0)
                        for(ssize_t j=0;j<n;++j) board.queueSerialInput(buf[j]);
                    else if(errno!=EINTR && errno!=EAGAIN) inputOpen=false;
                }
            }
            for(unsigned j=0;j<256 && !stopping &&
                (limit==0 || count<limit);++j,++count) {
                board.cpu().executeOne();
                BYTE v;
                while(board.takeSerialOutput(v))
                    if (write(STDOUT_FILENO,&v,1)<0)
                        throw std::runtime_error("terminal output failed");
            }
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "fm11-headless: " << ex.what() << '\n';
        return 1;
    }
}

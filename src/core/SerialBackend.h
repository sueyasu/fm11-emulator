#pragma once
#include "Usart8251.h"
#include "Fm11SerialClock.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/ioctl.h>

// Host-side serial transport, owned by the main CPU worker. It deliberately
// does not advance virtual 8251A bit clocks: Linux UART hardware handles
// physical bit timing, while PTY retains the prior byte-immediate behavior.
class SerialBackend {
public:
    explicit SerialBackend(const std::string& endpoint,unsigned baudDip=0,
                           bool fast=false,bool rtscts=false):endpoint_(endpoint),
                           rtscts_(rtscts) {
        const auto rate=Fm11SerialClock::lookup(baudDip);
        baud_=fast?rate.fastBaud:rate.asyncBaud;
        if(endpoint_=="none")return;
        if(endpoint_=="pty") {
            fd_=posix_openpt(O_RDWR|O_NOCTTY|O_NONBLOCK);
            if(fd_<0 || grantpt(fd_)!=0 || unlockpt(fd_)!=0)
                throw std::runtime_error(error("cannot create serial PTY"));
            const char* name=ptsname(fd_);
            if(!name)throw std::runtime_error(error("cannot resolve serial PTY"));
            label_=name;
            peer_=open(label_.c_str(),O_RDWR|O_NOCTTY|O_NONBLOCK);
            if(peer_<0)throw std::runtime_error(error("cannot open PTY slave"));
            termios raw{};
            if(tcgetattr(peer_,&raw)==0) {
                cfmakeraw(&raw);
                if(tcsetattr(peer_,TCSANOW,&raw)!=0)
                    throw std::runtime_error(error("cannot set PTY raw mode"));
            }
        } else {
            struct stat st{};
            if(stat(endpoint_.c_str(),&st)!=0 || !S_ISCHR(st.st_mode))
                throw std::runtime_error("serial device is not a character device: "+endpoint_);
            fd_=open(endpoint_.c_str(),O_RDWR|O_NOCTTY|O_NONBLOCK);
            if(fd_<0)throw std::runtime_error(error("cannot open serial device"));
            if(tcgetattr(fd_,&original_)!=0)
                throw std::runtime_error(error("serial device does not support termios"));
            savedTermios_=true;
            label_=endpoint_;
            configure(0x4e); // 8N1 before guest supplies a mode word
        }
    }
    SerialBackend(const SerialBackend&)=delete;
    SerialBackend& operator=(const SerialBackend&)=delete;
    ~SerialBackend() {
        if(savedTermios_ && fd_>=0)tcsetattr(fd_,TCSANOW,&original_);
        if(peer_>=0)close(peer_);
        if(fd_>=0)close(fd_);
    }
    const std::string& label() const {return label_;}
    bool enabled() const {return fd_>=0;}
    bool physical() const {return endpoint_!="pty" && endpoint_!="none";}
    // POSIX CS5+CSTOPB means 1.5 stop bits, CS6..CS8+CSTOPB
    // means 2 stop bits. The 8251A encodes 1, 1.5, 2 as 01/10/11.
    static bool stopBitsRepresentable(unsigned mode) {
        const unsigned stops=(mode>>6)&3;
        const unsigned bits=5+((mode>>2)&3);
        return stops==1 || (stops==2 && bits==5) ||
               (stops==3 && bits!=5);
    }
    // Host-independent mode-to-termios translation. PTY drivers may ignore
    // CS5..CS7 and PARENB, so verify these flags without relying on PTY
    // tcgetattr() to echo them back.
    static tcflag_t lineControlFlags(unsigned mode) {
        if((mode&0x03)==0)
            throw std::runtime_error("synchronous USART mode unsupported by termios");
        if(!stopBitsRepresentable(mode))
            throw std::runtime_error("unrepresentable 8251A stop-bit format");
        const unsigned word=5+((mode>>2)&3);
        tcflag_t flags=word==5?CS5:word==6?CS6:word==7?CS7:CS8;
        if(mode&0x10) {
            flags|=PARENB;
            if(!(mode&0x20))flags|=PARODD;
        }
        if(((mode>>6)&3)!=1)flags|=CSTOPB;
        return flags;
    }
    // Called by the CPU owner immediately before servicing host I/O.
    // TIOCMGET supplies live CTS/DSR/CD levels. TIOCMSET changes only the
    // two output bits; hardware input pins must never be overwritten.
    void synchronize(Usart8251& uart) {
        if(!physical())return;
        if(uart.phase()==Usart8251::ControlPhase::Command &&
           static_cast<int>(uart.mode())!=lastMode_)
            configure(uart.mode());
        synchronizeModem(uart);
    }
    template<class Take,class Inject>
    void service(Take take,Inject inject) {
        if(fd_<0)return;
        unsigned char c=0;
        while(take(c))pending_.push_back(c);
        // Hold queued physical UART bytes while CTS is deasserted.
        // The hardware driver remains responsible for bit timing.
        if(!pending_.empty() && (!physical() || ctsAllowed_)) {
            const auto n=write(fd_,pending_.data(),pending_.size());
            if(n>0)pending_.erase(pending_.begin(),pending_.begin()+n);
            else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR &&
                    !(endpoint_=="pty" && errno==EIO))
                throw std::runtime_error(error("serial write failed"));
        }
        unsigned char bytes[256];
        for(;;) {
            const auto n=read(fd_,bytes,sizeof(bytes));
            if(n>0)for(ssize_t i=0;i<n;++i)inject(bytes[i]);
            else if(n<0 && errno==EINTR)continue;
            else if(n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK &&
                    !(endpoint_=="pty" && errno==EIO))
                throw std::runtime_error(error("serial read failed"));
            else break;
        }
    }
private:
    static bool unsupportedModemIoctl(int code) {
        return code==ENOTTY || code==EINVAL || code==ENOSYS ||
               code==EOPNOTSUPP;
    }
    void synchronizeModem(Usart8251& uart) {
        if(!modemSupported_)return;
        int pins=0;
        if(ioctl(fd_,TIOCMGET,&pins)!=0) {
            if(unsupportedModemIoctl(errno)) {
                modemSupported_=false;
                // Leave CTS/DSR asserted as the existing virtual default
                // for adapters without modem-pin support.
                ctsAllowed_=true;
                uart.setModemInputs(true,true,true);
                return;
            }
            throw std::runtime_error(error("cannot query serial modem lines"));
        }
        // Read the physical inputs before making any changes to DTR/RTS.
        // In a three-wire connection CTS may be permanently low despite
        // usable TX/RX. Enable CTS gating only when explicitly requested.
        ctsAllowed_=!rtscts_ || (pins&TIOCM_CTS)!=0;
        uart.setModemInputs(ctsAllowed_,
                            (pins&TIOCM_DSR)!=0,
                            (pins&TIOCM_CAR)!=0);
        const int desired=(uart.dtr()?TIOCM_DTR:0) |
                          (uart.rts()?TIOCM_RTS:0);
        const int existing=pins&(TIOCM_DTR|TIOCM_RTS);
        if(desired==existing)return;
        // TIOCMBIS/TIOCMBIC preserve unrelated modem outputs and inputs.
        int setBits=desired&~existing;
        int clearBits=existing&~desired;
        if(setBits && ioctl(fd_,TIOCMBIS,&setBits)!=0) {
            if(unsupportedModemIoctl(errno)) {
                modemSupported_=false;
                ctsAllowed_=true;
                uart.setModemInputs(true,true,true);
                return;
            }
            throw std::runtime_error(error("cannot assert DTR/RTS"));
        }
        if(clearBits && ioctl(fd_,TIOCMBIC,&clearBits)!=0) {
            if(unsupportedModemIoctl(errno)) {
                modemSupported_=false;
                ctsAllowed_=true;
                uart.setModemInputs(true,true,true);
                return;
            }
            throw std::runtime_error(error("cannot deassert DTR/RTS"));
        }
    }
    std::string error(const std::string& what) const {
        return what+": "+std::strerror(errno);
    }
    static speed_t termiosBaud(unsigned baud) {
        switch(baud) {
        case 75:return B75; case 150:return B150; case 300:return B300;
        case 600:return B600; case 1200:return B1200; case 2400:return B2400;
        case 4800:return B4800; case 9600:return B9600;
        case 19200:return B19200; case 38400:return B38400;
        default:throw std::runtime_error("unsupported physical serial baud: "+std::to_string(baud));
        }
    }
    void configure(unsigned mode) {
        if(!physical())return;
        if(!baud_)throw std::runtime_error("undefined DIP baud for physical serial port");
        const tcflag_t lineFlags=lineControlFlags(mode);
        termios t{};
        if(tcgetattr(fd_,&t)!=0)throw std::runtime_error(error("tcgetattr failed"));
        cfmakeraw(&t);
        t.c_cflag=static_cast<tcflag_t>(t.c_cflag&~(CSIZE|PARENB|PARODD|CSTOPB));
        t.c_cflag|=lineFlags;
        t.c_cflag|=CLOCAL|CREAD;
        t.c_cc[VMIN]=0;t.c_cc[VTIME]=0;
        const auto speed=termiosBaud(baud_);
        if(cfsetispeed(&t,speed)!=0 || cfsetospeed(&t,speed)!=0 ||
           tcsetattr(fd_,TCSANOW,&t)!=0)
            throw std::runtime_error(error("cannot apply serial line parameters"));
        lastMode_=static_cast<int>(mode);
    }
    std::string endpoint_,label_;
    unsigned baud_=0;
    int fd_=-1,peer_=-1,lastMode_=-1;
    bool savedTermios_=false;
    bool modemSupported_=true;
    bool ctsAllowed_=true;
    bool rtscts_=false;
    termios original_{};
    std::vector<unsigned char> pending_;
};

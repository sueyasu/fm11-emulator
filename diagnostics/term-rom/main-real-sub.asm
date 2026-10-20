; FM-11 /Term protocol diagnostic; run with ORIGINAL display subsystem ROM.
; Implements the exact FD05 HALT / FC80-84 / command 12,2D transport
; from NitrOS-9 fm11console.asm, outside of OS-9 kernel/SCF.
; Load this 4 KiB ROM at main $5000; sub CPU ROM MUST remain original.
        org     $5000
PORT    equ     $fd05
MAIL    equ     $fc80
CMDOUT  equ     $12
CMDKEY  equ     $2d
        ; $0700: last request command
        ; $0701: last FD05 read value
        ; $0702: last response FC83
        ; $0703: last response FC84
        ; $0704: completed keyboard polls (low byte)
        ; $0705: count of responses with nonzero FC84
        ; $0706: last failure stage, zero if no failure
        ; $0707: next trace slot 0..15
        ; $0710..$074F: ring of 16 [FC83,FC84,FD05,poll-count] records
start   orcc    #$50
        lds     #$07f0
        ldx     #$0700
init    clr     ,x+
        cmpx    #$0750
        bne     init
        lda     #$03
        sta     $fda0
        ldx     #greeting
        jsr     puts
        ldx     #prompt
        jsr     puts
mainloop
        lda     #$10
        sta     $070a       ; stage: begin INKEY2
        lda     #CMDKEY
        sta     $0700
        jsr     transact
        lbcs    failed
        lda     #$11
        sta     $070a       ; stage: INKEY2 returned
        lda     $fc83
        sta     $0702
        lda     $fc84
        sta     $0703
        inc     $0704
        ; Store each result separately from displayed text.
        pshs    a,b,x
        ldb     $0707
        lslb
        lslb
        ldx     #$0710
        abx
        lda     $0702
        sta     ,x+
        lda     $0703
        sta     ,x+
        lda     $0701
        sta     ,x+
        lda     $0704
        sta     ,x
        inc     $0707
        lda     $0707
        anda    #$0f
        sta     $0707
        puls    a,b,x
        ; Same KPAscii result selection as the OS driver:
        ; FC83==0 -> FC84 is ASCII; nonzero is extended status.
        tst     $0702
        bne     idlepoll
        lda     $0703
        beq     idlepoll  ; No key: back off before next INKEY2 request.
        inc     $0705
        cmpa    #$0d
        beq     enter
        cmpa    #$08
        beq     backspace
        cmpa    #$20
        blo     mainloop
        cmpa    #$7e
        bhi     mainloop
        jsr     putchar
        lbra    mainloop
enter   jsr     putchar
        lda     #$0a
        jsr     putchar
        ldx     #prompt
        jsr     puts
        lbra    mainloop
backspace
        jsr     putchar
        lbra    mainloop

; Standalone ROM has no OS-9 F$Sleep. Limit empty polling to roughly
; a few 60 Hz ticks by spending simulated main-CPU cycles without
; HALTing the sub CPU. This is a busy delay, not a host OS sleep.
idlepoll
        pshs    x,b
        ldb     #3
idleouter
        ldx     #$1000
idleinner
        leax    -1,x
        bne     idleinner
        decb
        bne     idleouter
        puls    x,b
        lbra    mainloop
failed  lda     #$21
        ; Stop rather than silently repeating an invalid command.
failspin bra    failspin

puts    lda     ,x+
        beq     putsret
        jsr     putchar
        bcs     putsret
        bra     puts
putsret rts

; A=character; command 12 via exactly the same HALT/READY transport.
putchar pshs    a,b
        sta     $0708
        lda     #CMDOUT
        sta     $0700
        lda     $0708
        sta     $0709
        jsr     transact
        puls    a,b,pc

; Entry command number FC90, char payload FC99 for command 12.
; Sequence mirrors KeyboardPoll/ScreenPutC in fm11console.asm.
transact
        pshs    a,b,x
        ldx     #$ffff
readylp lda     #$21
        sta     $070a
        lda     PORT
        sta     $0701
        bpl     haltit
        leax    -1,x
        bne     readylp
        lda     #1
        bra     fail
haltit  lda     #$80
        sta     PORT
        ldx     #$ffff
haltlp  lda     #$22
        sta     $070a
        lda     PORT
        sta     $0701
        bmi     install
        leax    -1,x
        bne     haltlp
        clr     PORT
        lda     #2
        bra     fail
install clr     MAIL
        clr     MAIL+1
        lda     $0700
        sta     MAIL+2
        cmpa    #CMDOUT
        bne     inputinit
        lda     $0709
        sta     MAIL+3
        bra     release
inputinit
        clr     MAIL+3
        clr     MAIL+4
release clr     PORT
        ldx     #$ffff
donelp  lda     #$23
        sta     $070a
        lda     PORT
        sta     $0701
        bpl     done
        leax    -1,x
        bne     donelp
        lda     #3
        bra     fail
done    clra
        sta     $0706
        andcc   #$fe
        puls    a,b,x,pc
fail    sta     $0706
        orcc    #1
        puls    a,b,x,pc

greeting fcc    "FM11 ORIGINAL SUB ROM TEST"
        fcb     $0d,$0a,0
prompt  fcc     "TEST> "
        fcb     0
        fill    $00,$5ffe-*
        fdb     start

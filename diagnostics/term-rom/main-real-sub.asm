; FM-11 /Term protocol diagnostic; run with ORIGINAL display subsystem ROM.
; Implements the exact FD05 HALT / FC80-84 / command 12,2D transport
; from NitrOS-9 fm11console.asm, outside of OS-9 kernel/SCF.
; Load this 4 KiB ROM at main $5000; sub CPU ROM MUST remain original.
        org     $5000
PORT    equ     $fd05
MAIL    equ     $fc80
CMDOUT  equ     $12
CMDKEY  equ     $2d
        ; FC90: last request command
        ; FC91: last FD05 read value
        ; FC92: last response FC83
        ; FC93: last response FC84
        ; FC94: completed keyboard polls (low byte)
        ; FC95: count of responses with nonzero FC84
        ; FC96: last failure stage, zero if no failure
        ; FC97: next trace slot 0..15
        ; FCA0..FCDF: ring of 16 [FC83,FC84,FD05,FC94] records
start   orcc    #$50
        lds     #$07f0
        ldx     #$fc90
init    clr     ,x+
        cmpx    #$fce0
        bne     init
        lda     #$03
        sta     $fda0
        ldx     #greeting
        jsr     puts
        ldx     #prompt
        jsr     puts
mainloop
        lda     #CMDKEY
        sta     $fc90
        jsr     transact
        lbcs    failed
        lda     $fc83
        sta     $fc92
        lda     $fc84
        sta     $fc93
        inc     $fc94
        ; Store each result separately from displayed text.
        pshs    a,b,x
        ldb     $fc97
        lslb
        lslb
        ldx     #$fca0
        abx
        lda     $fc92
        sta     ,x+
        lda     $fc93
        sta     ,x+
        lda     $fc91
        sta     ,x+
        lda     $fc94
        sta     ,x
        inc     $fc97
        lda     $fc97
        anda    #$0f
        sta     $fc97
        puls    a,b,x
        ; Same KPAscii result selection as the OS driver:
        ; FC83==0 -> FC84 is ASCII; nonzero is extended status.
        tst     $fc92
        lbne    mainloop
        lda     $fc93
        beq     mainloop  ; Diagnostic guards no-key; trace still logs it.
        inc     $fc95
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
        ldx     #prompt
        jsr     puts
        lbra    mainloop
backspace
        jsr     putchar
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
        sta     $fc98
        lda     #CMDOUT
        sta     $fc90
        lda     $fc98
        sta     $fc99
        jsr     transact
        puls    a,b,pc

; Entry command number FC90, char payload FC99 for command 12.
; Sequence mirrors KeyboardPoll/ScreenPutC in fm11console.asm.
transact
        pshs    a,b,x
        ldx     #$ffff
readylp lda     PORT
        sta     $fc91
        bpl     haltit
        leax    -1,x
        bne     readylp
        lda     #1
        bra     fail
haltit  lda     #$80
        sta     PORT
        ldx     #$ffff
haltlp  lda     PORT
        sta     $fc91
        bmi     install
        leax    -1,x
        bne     haltlp
        clr     PORT
        lda     #2
        bra     fail
install clr     MAIL
        clr     MAIL+1
        lda     $fc90
        sta     MAIL+2
        cmpa    #CMDOUT
        bne     inputinit
        lda     $fc99
        sta     MAIL+3
        bra     release
inputinit
        clr     MAIL+3
        clr     MAIL+4
release clr     PORT
        ldx     #$ffff
donelp  lda     PORT
        sta     $fc91
        bpl     done
        leax    -1,x
        bne     donelp
        lda     #3
        bra     fail
done    clra
        sta     $fc96
        andcc   #$fe
        puls    a,b,x,pc
fail    sta     $fc96
        orcc    #1
        puls    a,b,x,pc

greeting fcc    "FM11 ORIGINAL SUB ROM TEST"
        fcb     $0d,0
prompt  fcc     "TEST> "
        fcb     0
        fill    $00,$5ffe-*
        fdb     start

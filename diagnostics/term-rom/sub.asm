; Standalone sub-CPU ROM mapped at $B000-$FFFF.
; CRTC + text RAM + FC80/9F80 mailbox, deliberately no key synthesis.
        org     $b000
start   orcc    #$50
        lds     #$9ef0
        ldx     #$8000
        lda     #$20
        ldb     #$07
clear   sta     ,x+
        stb     ,x+
        cmpx    #$8fa0
        bne     clear
        ldx     #$8000
        stx     $9f10

        lda     #$01
        sta     $afee
        lda     #$28
        sta     $afef
        lda     #$06
        sta     $afee
        lda     #$19
        sta     $afef
        lda     #$0c
        sta     $afee
        clr     $afef
        lda     #$0d
        sta     $afee
        clr     $afef
        lda     #$01
        sta     $afe0       ; READY handshake to main FD05

poll    lda     $9f80
        beq     poll
        lda     $9f81
        cmpa    #13
        beq     newline
        cmpa    #8
        beq     backspace
        ldx     $9f10
        sta     ,x+
        lda     #$07
        sta     ,x+
        stx     $9f10
        bra     done
newline ldx     $9f10
        ; Advance to the start of the next 80-column row.
        ; 80 cells = 160 bytes. Compute row by repeated subtraction.
        tfr     x,d
        subd    #$8000
        ldx     #$8000
rowfind cmpd    #$00a0
        blo     rowfound
        subd    #$00a0
        leax    $00a0,x
        bra     rowfind
rowfound
        leax    $00a0,x
        cmpx    #$8fa0
        blo     storepos
        ldx     #$8000
storepos stx    $9f10
        bra     done
backspace
        ldx     $9f10
        cmpx    #$8000
        beq     done
        leax    -2,x
        lda     #$20
        sta     ,x
        lda     #$07
        sta     1,x
        stx     $9f10
done    clr     $9f80       ; acknowledge
        bra     poll

; Writing AFE0 enables periodic sub-CPU NMI in the emulator. All ROM
; interrupt vectors must be valid or execution jumps into video RAM after
; the first NMI, making the banner appear only momentarily.
irqret  rti
        fill    $00,$fff6-*
        fdb     irqret      ; $FFF6 FIRQ
        fdb     irqret      ; $FFF8 IRQ
        fdb     irqret      ; $FFFA SWI
        fdb     irqret      ; $FFFC NMI
        fdb     start       ; $FFFE RESET

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
        ldx     $9f10
        sta     ,x+
        lda     #$07
        sta     ,x+
        stx     $9f10
        bra     done
newline ldx     #$80a0
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

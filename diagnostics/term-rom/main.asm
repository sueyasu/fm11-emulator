; Standalone main-CPU boot ROM, mapped at $5000-$5FFF.
; This is an isolated hardware-path test, not yet the NitrOS-9 /Term driver.
        org     $5000
start   orcc    #$50
        lds     #$07f0
        clr     $fc80
        clr     $fc81
waitsub lda     $fd05
        bmi     waitsub
        lda     #$03
        sta     $fda0
        ldx     #banner
nextch  lda     ,x+
        beq     printed
        bsr     putchar
        bra     nextch
printed ldx     #subtitle
next2   lda     ,x+
        beq     ready
        bsr     putchar
        bra     next2
ready   lda     #$77
        sta     $fc84
; With FD02 bit 7, the main CPU receives keyboard events in FD00/FD01.
; Do not read or echo any input until the diagnostic stage requests it.
        lda     #$80
        sta     $fd02
poll    lda     $fd03
        bita    #$40
        beq     poll
        lda     $fd00
        sta     $fc90
        lda     $fd01
        sta     $fc91
        inc     $fc92
        bra     poll

; Transmit one ASCII character in A over FC80/FC81, then wait for ACK.
putchar sta     $fc81
        ldb     #$01
        stb     $fc80
ack     ldb     $fc80
        bne     ack
        rts

banner  fcc     "FM-11 DISPLAY / INPUT ISOLATION"
        fcb     13,0
subtitle fcc    "NO KEY SHOULD GENERATE EVENTS"
        fcb     0
        fill    $00,$5ffe-*
        fdb     start

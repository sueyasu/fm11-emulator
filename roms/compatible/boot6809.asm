* FM-11 MC6809 / HD6309 compatible reset ROM, independent MIT implementation.
* ROM $5000-$5FFF; final word is copied to reset vector $FFFE.
* Boot DIP FD23 active low: HDD (7), 2HD (6), 2D (5).
* Load exactly 512 bytes to $0400 and enter in native 6809 mode.
* No memory test or bubble memory boot.
        org     $5000
start   orcc    #$50
        lds     #$3F00
        clra
        tfr     a,dp
        clr     >$FD93
        clr     >$FDA0
        lbsr    init_display
        bcs     display_done
        lda     #3
        sta     >$FDA0
display_done
        lda     >$FD23
        bita    #$80
        bne     no_hdd
        pshs    a
        lbsr    boot_hdd
        puls    a
        lbcc    enter_ipl
no_hdd  bita    #$40
        bne     no_2hd
        pshs    a
        lbsr    boot_2hd
        puls    a
        lbcc    enter_ipl
no_2hd  bita    #$20
        bne     stop
        lbsr    boot_2d
        bcc     enter_ipl
stop    bra     stop
enter_ipl
        jmp     $0400

* Native CHARACTER OUT: default attribute, clear and home.
init_display
        ldb     #$1B
        bsr     display_char
        bcs     display_return
        ldb     #$47
        bsr     display_char
        bcs     display_return
        ldb     #7
        bsr     display_char
        bcs     display_return
        ldb     #$16
        bsr     display_char
        bcs     display_return
        ldb     #$0B
        bsr     display_char
display_return
        rts
display_char
        ldx     #$FFFF
disp_ready
        lda     >$FD05
        bita    #$80
        beq     disp_halt
        leax    -1,x
        bne     disp_ready
        bra     disp_fail
disp_halt
        lda     #$80
        sta     >$FD05
        ldx     #$FFFF
disp_wait_halt
        lda     >$FD05
        bita    #$80
        bne     disp_install
        leax    -1,x
        bne     disp_wait_halt
        clr     >$FD05
        bra     disp_fail
disp_install
        clr     >$FC80
        clr     >$FC81
        lda     #$12
        sta     >$FC82
        stb     >$FC83
        clr     >$FC84
        clr     >$FD05
        ldx     #$FFFF
disp_complete
        lda     >$FD05
        bita    #$80
        beq     disp_ok
        leax    -1,x
        bne     disp_complete
disp_fail
        orcc    #1
        rts
disp_ok andcc   #$FE
        rts

* Type I RESTORE; Y=FDC command/status address.
restore
        lda     #$0B
        sta     ,y
        ldu     #$FFFF
restore_wait
        lda     ,y
        bita    #1
        beq     restore_done
        leau    -1,u
        bne     restore_wait
        bra     restore_fail
restore_done
        bita    #$90
        bne     restore_fail
        andcc   #$FE
        rts
restore_fail
        orcc    #1
        rts

boot_2d
        lda     #$80
        sta     >$FD1D
        clr     >$FD1C
        ldy     #$FD18
        lbsr    restore
        lbcs    restore_fail
        clr     >$FD94
        ldx     #$0400
        lda     #1
        sta     >$FD1A
read_2d
        clr     >$FD50
        stx     >$FD40
        ldd     #256
        std     >$FD42
        lda     #4
        sta     >$FD50
        lda     #$80
        sta     ,y
        ldu     #$FFFF
wait_2d
        lda     >$FD50
        bita    #$40
        bne     fail_2d
        bita    #$80
        bne     done_2d
        lda     ,y
        bita    #$9C
        bne     fail_2d
        leau    -1,u
        bne     wait_2d
        bra     fail_2d
done_2d
        clr     >$FD50
        lda     ,y
        bita    #$9C
        bne     fail_2d
        leax    256,x
        cmpx    #$0600
        beq     ok_2d
        inc     >$FD1A
        bra     read_2d
ok_2d
        andcc   #$FE
        rts
fail_2d
        clr     >$FD50
        lda     #$D0
        sta     ,y
        orcc    #1
        rts

boot_2hd
        lda     >$FD36
        bita    #1
        lbeq    restore_fail
        clr     >$FD35
        clr     >$FD34
        ldy     #$FD30
        lbsr    restore
        lbcs    restore_fail
        clr     >$FD95
        ldx     #$0400
        lda     #1
        sta     >$FD32
read_2hd
        clr     >$FD51
        stx     >$FD44
        ldd     #128
        std     >$FD46
        lda     #4
        sta     >$FD51
        lda     #$80
        sta     ,y
        ldu     #$FFFF
wait_2hd
        lda     >$FD51
        bita    #$40
        bne     fail_2hd
        bita    #$80
        bne     done_2hd
        lda     ,y
        bita    #$9C
        bne     fail_2hd
        leau    -1,u
        bne     wait_2hd
        bra     fail_2hd
done_2hd
        clr     >$FD51
        lda     ,y
        bita    #$9C
        bne     fail_2hd
        leax    128,x
        cmpx    #$0600
        beq     ok_2hd
        inc     >$FD32
        bra     read_2hd
ok_2hd
        andcc   #$FE
        rts
fail_2hd
        clr     >$FD51
        lda     #$D0
        sta     ,y
        orcc    #1
        rts

* MDC binary LBA, HD6844 channel 2.
boot_hdd
        lda     #$20
        sta     >$FDC2
        lda     #$11
        sta     >$FDC0
        lda     #$70
        sta     >$FDC1
        clr     >$FDC1
        lbsr    hdd_result
        bcs     hdd_fail
        clr     >$FD96
        ldx     #$0400
hdd_next
        clr     >$FD52
        stx     >$FD48
        ldd     #256
        std     >$FD4A
        lda     #4
        sta     >$FD52
        lda     #$22
        sta     >$FDC0
        clr     >$FDC1
        clr     >$FDC1
        clr     >$FDC1
        clr     >$FDC1
        clr     >$FDC1
        tfr     x,d
        suba    #4
        sta     >$FDC1
        lda     #1
        sta     >$FDC1
        clr     >$FDC1
        lbsr    hdd_result
        bcs     hdd_fail
        lda     >$FD52
        anda    #$C0
        cmpa    #$80
        bne     hdd_fail
        clr     >$FD52
        leax    256,x
        cmpx    #$0600
        bne     hdd_next
        andcc   #$FE
        rts
hdd_fail
        clr     >$FD52
        orcc    #1
        rts
hdd_result
        ldu     #$FFFF
hdd_wait
        lda     >$FDC0
        bita    #$40
        bne     hdd_ready
        leau    -1,u
        bne     hdd_wait
        orcc    #1
        rts
hdd_ready
        lda     >$FDC1
        pshs    a
        ldb     #9
hdd_drain
        lda     >$FDC1
        decb
        bne     hdd_drain
        puls    a
        bita    #$80
        bne     hdd_fail
        andcc   #$FE
        rts

        fill    $FF,$5FFE-*
        fdb     start
        end     start

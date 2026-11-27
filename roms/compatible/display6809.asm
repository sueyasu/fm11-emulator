* Independent minimal FM-11 Display Sub System firmware.
* Public mailbox ABI only; private state is deliberately different.
        org     $b000
ctl     equ     $afe0
firqreg equ     $afe4
keyreg  equ     $afec
crtc    equ     $afee
mail    equ     $9f80
col     equ     $9000
row     equ     $9001
attr    equ     $9002
state   equ     $9003
start   equ     $9004
cursor  equ     $9006
tmpx    equ     $9008
head    equ     $9009
tail    equ     $900a
shadow  equ     $900b
fifo    equ     $9100

reset
        orcc    #$50
        lds     #$9f60
        lda     #$90
        tfr     a,dp
        ldx     #$9000
        ldd     #0
reset_ram
        std     ,x++
        cmpx    #$9f80
        blo     reset_ram
        std     mail
        clr     mail+2
        lda     #7
        sta     attr
        sta     $afe1
        clr     $afe2
        clr     $afe3
        clr     $afe8
        clr     $afe9
        clr     $afea
        clr     $afeb
        lda     #$47
        sta     $afe6
        lda     #$02
        sta     shadow
        sta     ctl
* Clear all three graphics planes with one write per address.
        ldx     #0
        ldd     #0
reset_gfx
        std     ,x++
        cmpx    #$8000
        blo     reset_gfx
        ldx     #crtc_init
        clra
reset_crtc
        sta     crtc
        ldb     ,x+
        stb     crtc+1
        inca
        cmpa    #16
        blo     reset_crtc
        lbsr    clear_all
* The initial keyboard self-test produces a P event on this hardware.
* Consume only the startup event before READY, not subsequent user keys.
        lda     firqreg
        bita    #1
        beq     reset_ready
        ldd     keyreg
        cmpd    #$0050
        beq     reset_ready
        lbsr    queue_key
reset_ready
        andcc   #$af
ready
        lda     shadow
        ora     #1
        sta     shadow
        sta     ctl
wait_cmd
        lda     mail+2
        beq     wait_cmd
        ldb     shadow
        andb    #$fe
        stb     shadow
        stb     ctl
        clr     mail
        clr     mail+1
        cmpa    #$12
        beq     cmd_out
        cmpa    #$2d
        beq     cmd_key
        lda     #$46
        sta     mail
        bra     done
cmd_out
        lda     mail+3
        lbsr    output
        lbsr    update_cursor
        bra     done
cmd_key
        orcc    #$40
        lda     head
        cmpa    tail
        beq     no_key
        ldx     #fifo
        lsla
        leax    a,x
        ldd     ,x
        std     mail+3
        inc     head
        lda     head
        anda    #$3f
        sta     head
        bra     key_done
no_key
        ldd     #$8000
        std     mail+3
key_done
        andcc   #$bf
done
        clr     mail+2
        lbra     ready

* Parser state 0 normal, 1 ESC, 2 attribute, 3 X, 4 Y.
output
        ldb     state
        beq     normal
        clr     state
        cmpb    #1
        beq     escape
        cmpb    #2
        beq     set_attr
        cmpb    #3
        beq     set_x
        cmpa    #25
        lbhs     output_ret
        ldb     tmpx
        cmpb    #80
        lbhs     output_ret
        sta     row
        stb     col
        rts
set_x
        sta     tmpx
        lda     #4
        sta     state
        rts
set_attr
        anda    #$3f
        sta     attr
output_ret
        rts
escape
        cmpa    #'G
        bne     esc_t
        lda     #2
        sta     state
        rts
esc_t
        cmpa    #'T
        lbeq    erase_line
        cmpa    #'Y
        lbeq    erase_end
        cmpa    #'*
        lbeq    clear_all
        rts
normal
        cmpa    #$20
        lbhs     printable
        cmpa    #$08
        lbeq     left
        cmpa    #$09
        lbeq     tab
        cmpa    #$0a
        lbeq    linefeed
        cmpa    #$0b
        lbeq     home
        cmpa    #$0c
        lbeq    clear_all
        cmpa    #$0d
        lbeq     carriage
        cmpa    #$12
        lbeq     position
        cmpa    #$16
        lbeq    clear_all
        cmpa    #$1b
        lbeq     begin_esc
        cmpa    #$1c
        lbeq     right
        cmpa    #$1d
        lbeq     left
        cmpa    #$1e
        lbeq     up
        cmpa    #$1f
        lbeq     down
        rts
printable
        pshs    a
        lbsr    cell_address
        puls    a
        ldb     attr
        std     ,x
        inc     col
        lda     col
        cmpa    #80
        lblo     output_ret
        clr     col
        lbra    linefeed
position
        lda     #3
        sta     state
        rts
begin_esc
        lda     #1
        sta     state
        rts
home
        clr     row
carriage
        clr     col
        rts
left
        dec     col
        bpl     move_ret
        lda     #79
        sta     col
        rts
right
        inc     col
        lda     col
        cmpa    #80
        blo     move_ret
        clr     col
        rts
up
        dec     row
        bpl     move_ret
        lda     #24
        sta     row
        rts
down
        inc     row
        lda     row
        cmpa    #25
        blo     move_ret
        clr     row
move_ret
        rts
tab
        lda     col
        adda    #8
        anda    #$f8
        cmpa    #80
        blo     tab_store
        clra
tab_store
        sta     col
        rts

* CRTC addresses are TWO text cells per unit, not one.
* start is a byte offset into the 4 KiB text ring.
cell_address
        lda     row
        ldb     #160
        mul
        addd    start
        addb    col
        adca    #0
        addb    col
        adca    #0
        anda    #$0f
        ora     #$80
        tfr     d,x
        rts
update_cursor
        bsr     cell_address
        tfr     x,d
        anda    #$0f
        bitb    #2
        beq     even_cursor
        pshs    d
        lda     shadow
        ora     #$20
        bra     cursor_bit
even_cursor
        pshs    d
        lda     shadow
        anda    #$df
cursor_bit
        sta     shadow
        sta     ctl
        puls    d
        lsra
        rorb
        lsra
        rorb
        pshs    b
* A is high address byte; write index then data.
        tfr     a,b
        lda     #14
        std     crtc
        puls    b
        lda     #15
        std     crtc
        rts
update_start
        ldd     start
        lsra
        rorb
        lsra
        rorb
        pshs    b
        tfr     a,b
        lda     #12
        std     crtc
        puls    b
        lda     #13
        std     crtc
        rts
linefeed
        lda     row
        cmpa    #24
        bhs     scroll
        inc     row
        rts
scroll
        ldd     start
        addd    #160
        anda    #$0f
        std     start
        pshs    a
        lda     col
        pshs    a
        clr     col
        lbsr    erase_line
        puls    a
        sta     col
        puls    a
        bra     update_start

clear_all
        clr     row
        clr     col
        ldd     #0
        std     start
        ldx     #$8000
        ldy     #2048
        bsr     blank_cells
        bra     update_start
erase_line
        lbsr    cell_address
        ldb     #80
        subb    col
        clra
        tfr     d,y
        bra     blank_cells
erase_end
        lbsr    cell_address
        pshs    x
        lda     #25
        suba    row
        ldb     #80
        mul
        subb    col
        sbca    #0
        tfr     d,y
        puls    x
blank_cells
        clra
        ldb     attr
blank_loop
        std     ,x++
        cmpx    #$9000
        blo     blank_next
        ldx     #$8000
blank_next
        leay    -1,y
        bne     blank_loop
        rts

* FIRQ hardware latch bit 0 is keyboard. Preserve full registers used.
firq
        pshs    d,x
        lda     firqreg
        bita    #1
        beq     firq_done
        ldd     keyreg
        bita    #2
        bne     firq_done
        bsr     queue_key
firq_done
        puls    d,x
        rti
queue_key
        pshs    d
        lda     tail
        inca
        anda    #$3f
        cmpa    head
        beq     queue_full
        ldx     #fifo
        ldb     tail
        lslb
        leax    b,x
        sta     tail
        puls    d
        std     ,x
        rts
queue_full
        puls    d,pc
irq
        tst     $afe5
        rti
nmi
        rti
unused
        rti
crtc_init
        fcb     $35,$28,$2c,$04,$1f,$06,$19,$19,$00,$07
        fcb     $40,$07,0,0,0,0
        fill    $ff,$fff2-*
        fdb     unused,unused,firq,irq,unused,nmi,reset

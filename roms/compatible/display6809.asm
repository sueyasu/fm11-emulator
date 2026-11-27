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
gstart  equ     $900c
kcount  equ     $900e
kcode   equ     $9010
kmap    equ     $9200       * 2000 visible cells, 0/1/2 = ANK/left/right
kpos    equ     $9012
kaddr   equ     $9014
kleft   equ     $9016
kright  equ     $9017
krow    equ     $9018
kcol    equ     $9019
sjmode  equ     $901a       * FFh enables Shift-JIS interpretation
sjlead  equ     $901b       * pending lead byte, zero when idle

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
        lbeq    cmd_out
        cmpa    #$34
        lbeq    cmd_kanji
        cmpa    #$37
        lbeq    cmd_sjis
        cmpa    #$2d
        lbeq    cmd_key
        lda     #$46
        sta     mail
        lbra    done
cmd_out
        lda     mail+3
        lbsr    output
        lbsr    update_cursor
        lbra    done
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
        lbeq    stream_byte
        clr     state
        cmpb    #1
        lbeq    escape
        cmpb    #2
        lbeq    set_attr
        cmpb    #3
        lbeq    set_x
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
* Only normal stream bytes are candidates for Shift-JIS.
* Native ESC and position argument states take precedence.
stream_byte
        tst     sjmode
        lbeq    normal
        ldb     sjlead
        lbne    sj_trail
sj_new
        cmpa    #$81
        lblo    normal
        cmpa    #$9f
        lbls    sj_save
        cmpa    #$e0
        lblo    normal
        cmpa    #$ef
        lbhi    normal
sj_save
        sta     sjlead
        rts
sj_trail
        clr     sjlead
        cmpa    #$40
        lblo    sj_new
        cmpa    #$fc
        lbhi    sj_new
        cmpa    #$7f
        lbeq    sj_new
* A=trail, B=lead. Convert to JIS row and column.
        exg     a,b
        cmpa    #$e0
        blo     sj_low_lead
        suba    #$c1
        bra     sj_row
sj_low_lead
        suba    #$81
sj_row
        lsla
        adda    #$21
        cmpb    #$9f
        blo     sj_odd_row
        inca
        subb    #$7e
        lbra    sj_draw
sj_odd_row
        cmpb    #$7f
        blo     sj_odd_low
        decb
sj_odd_low
        subb    #$1f
sj_draw
        lbsr    jis_selector
        lbcs    output_ret
        std     kcode
* Wrap before drawing when only one column remains.
        lda     col
        cmpa    #79
        blo     sj_fits
        clr     col
        lbsr    linefeed
sj_fits
        lbsr    draw_kanji
        lda     col
        adda    #2
        cmpa    #80
        lblo    sj_advance
        clr     col
        lbra    linefeed
sj_advance
        sta     col
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
        ldd     kcount
        beq     print_clean
        lbsr    erase_at_cursor
        lbsr    cell_address
print_clean
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
        lbsr    scroll_kanji
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
        lbsr    clear_kanji
        clr     row
        clr     col
        ldd     #0
        std     start
        ldx     #$8000
        ldy     #2048
        bsr     blank_cells
        bra     update_start
erase_line
        ldd     kcount
        beq     erase_line_text
        pshs    a,b
        lda     row
        sta     krow
        lda     col
        sta     kcol
        ldb     #80
        subb    col
        clra
        lbsr    erase_range
        puls    a,b
erase_line_text
        lbsr    cell_address
        ldb     #80
        subb    col
        clra
        tfr     d,y
        bra     blank_cells
erase_end
        ldd     kcount
        beq     erase_end_text
        lda     row
        sta     krow
        lda     col
        sta     kcol
        lda     #25
        suba    row
        ldb     #80
        mul
        subb    col
        sbca    #0
        lbsr    erase_range
erase_end_text
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


* K-1: JIS78 KANJI OUT, 640x400 graphics plane.
* The main CPU owns FDA0 layer gating; never change it here.
cmd_kanji
        lda     col
        cmpa    #79
        lbhs    done
        ldd     mail+3
        lbsr    jis_selector
        lbcs    done
        std     kcode
        lbsr    draw_kanji
        lbra    done

* Shared glyph renderer: kcode is selector, cursor stays unchanged.
draw_kanji
        lbsr    erase_at_cursor
        inc     col
        lbsr    erase_at_cursor
        dec     col
        lbsr    cell_address
        clra
        clrb
        std     ,x++
        cmpx    #$9000
        blo     kanji_text_next
        ldx     #$8000
kanji_text_next
        std     ,x
        lda     row
        sta     krow
        lda     col
        sta     kcol
        lbsr    map_address
        lda     #1
        sta     ,x+
        inca
        sta     ,x
        ldd     kcount
        addd    #1
        std     kcount
        ldd     kcode
        std     $aff4
        lda     attr
        anda    #7
        sta     $afe1
        lbsr    graphics_address
        ldy     #16
kanji_raster
        ldd     $aff6
        pshs    d
        lda     attr
        bita    #8
        puls    d
        beq     kanji_store
        coma
        comb
kanji_store
        sta     ,x+
        cmpx    #$8000
        blo     kanji_right
        ldx     #0
kanji_right
        stb     ,x
        leax    79,x
        tfr     x,d
        anda    #$7f
        tfr     d,x
        leay    -1,y
        bne     kanji_raster
        rts

* Supported $37 mode values used by the stock-ROM kanjitest.
* Auxiliary arguments are reserved for future compatibility work.
cmd_sjis
        clr     sjlead
        lda     mail+3
        cmpa    #$ff
        beq     sj_mode_store
        clra
sj_mode_store
        sta     sjmode
        lbra    done

* Return selector D, carry set for unsupported JIS codes.
* Table groups follow the compatible font generator, including A16.
jis_selector
        cmpa    #$21
        lblo    jis_bad
        cmpa    #$73
        lbhi    jis_bad
        cmpb    #$21
        lblo    jis_bad
        cmpb    #$7e
        lbhi    jis_bad
        suba    #$20
        subb    #$20
        cmpa    #8
        lblo    jis_symbols
        cmpa    #16
        lblo    jis_bad
        cmpa    #48
        lblo    jis_first
* Second level: group = 3*((ku-48)/16) + ten/32.
        pshs    d
        suba    #48
        lsra
        lsra
        lsra
        lsra
        ldb     #3
        mul
        lda     1,s
        lsra
        lsra
        lsra
        lsra
        lsra
        pshs    b
        adda    ,s+
        lsla
        bita    #$10
        beq     jis_second_normal
        lda     #$1d
        bra     jis_second_base
jis_second_normal
        anda    #$0e
        ora     #$10
jis_second_base
        clrb
        bra     jis_add_offset
jis_first
        pshs    d
        lda     1,s
        lsra
        lsra
        lsra
        lsra
        lsra
        lsla
        adda    #4
        ldb     ,s
        cmpb    #32
        blo     jis_first_base
        adda    #6
jis_first_base
        clrb
        bra     jis_add_offset
jis_symbols
        pshs    d
        lda     1,s
        lsra
        lsra
        lsra
        lsra
        lsra
        ldx     #symbol_bases
        lda     a,x
        clrb
jis_add_offset
        pshs    d
        lda     2,s
        anda    #15
        ldb     #32
        mul
        addd    ,s++
        pshs    d
        ldb     3,s
        andb    #31
        clra
        addd    ,s++
        leas    2,s
        andcc   #$fe
        rts
jis_bad
        orcc    #1
        rts
symbol_bases
        fcb     0,2,1

* Map uses logical screen cells, not physical text offsets. The two
* hardware rings have different lengths. Only metadata is moved on scroll.
map_address
        lda     krow
        ldb     #80
        mul
        addb    kcol
        adca    #0
        addd    #kmap
        tfr     d,x
        rts
graphics_address
        lda     krow
        ldb     #5
        mul
        tfr     b,a
        clrb
        addd    gstart
        addb    kcol
        adca    #0
        anda    #$7f
        tfr     d,x
        rts
erase_at_cursor
        lda     row
        sta     krow
        lda     col
        sta     kcol
        bra     erase_one
* Clear both halves of any Kanji touched by a single-cell operation.
erase_one
        pshs    d,x,y,u
        lbsr    map_address
        lda     ,x
        beq     erase_one_done
        cmpa    #2
        bne     erase_pair
        dec     kcol
        leax    -1,x
erase_pair
        clr     ,x
        clr     1,x
        ldd     kcount
        subd    #1
        std     kcount
        lbsr    graphics_address
        lda     #7
        sta     $afe1
        ldy     #16
erase_raster
        clra
        clrb
        sta     ,x+
        cmpx    #$8000
        blo     erase_right
        ldx     #0
erase_right
        stb     ,x
        leax    79,x
        tfr     x,d
        anda    #$7f
        tfr     d,x
        leay    -1,y
        bne     erase_raster
* Both text cells were transparent blank cells; no stale glyph remains.
erase_one_done
        puls    d,x,y,u,pc
erase_range
        tfr     d,u
range_loop
        cmpu    #0
        beq     range_done
        lda     kcol
        pshs    a
        lbsr    erase_one
        puls    a
        inca
        cmpa    #80
        blo     range_next
        clra
        inc     krow
range_next
        sta     kcol
        leau    -1,u
        bra     range_loop
range_done
        rts

scroll_kanji
        ldd     kcount
        beq     scroll_gstart
* Remove old top-row glyphs before moving metadata, while old origin
* still points to their graphics bytes.
        clr     krow
        clr     kcol
        ldd     #80
        lbsr    erase_range
        ldx     #kmap+80
        ldy     #kmap
scroll_map
        ldd     ,x++
        std     ,y++
        cmpx    #kmap+2000
        blo     scroll_map
        ldd     #0
scroll_map_blank
        std     ,y++
        cmpy    #kmap+2000
        blo     scroll_map_blank
scroll_gstart
        ldd     gstart
        addd    #1280
        anda    #$7f
        std     gstart
        std     $afea
        rts
clear_kanji
        ldd     kcount
        beq     clear_gstart
        clr     krow
        clr     kcol
        ldd     #2000
        lbsr    erase_range
clear_gstart
        ldd     #0
        std     gstart
        std     $afea
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
* Full-height blinking cursor (16 rasters per 80x25 character).
        fcb     $40,$0f,0,0,0,0
        fill    $ff,$fff2-*
        fdb     unused,unused,firq,irq,unused,nmi,reset

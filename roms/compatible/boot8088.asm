; FM-11 8088 / NEC V20 compatible reset ROM, independent implementation.
; MIT license; no Fujitsu firmware bytes or decompiled code used.
;
; Linked at physical FF000h-FFFFFh (CS=FF00h). Reset vector FFFF0h.
; Boot DIP FD23h: bit 5=0 enables 2D, bit 6=0 enables 2HD,
;                 bit 7=0 enables MDC HDD. The first successful device wins.
; 2D IPL: T0/H0/S1..S2 (2 x 256 bytes), physical 07C00h.
; 2HD IPL: T0/H0/S1..S4 (4 x 128 bytes), physical 07C00h.
; HDD IPL: LBA0..1 (2 x 256 bytes), physical 07C00h.
; All IPLs enter at CS:IP = 07C0:0000.
; No bubble-memory support and no destructive RAM test.
;
; Depends only on the normal FM-11 I/O registers and DMA, NOT emulator HLE.
cpu 8086
bits 16
org 0

%define BOOT_SEG   0x07C0
%define BOOT_ADDR  0x7C00
%define DIPSW      0xFD23
%define DMA_FF     0x1C
%define DMA_ADDR   0x10
%define DMA_TC     0x18
%define DMA_MASK   0x1A
%define DMA_MODE   0x1B
%define DMA_BANK   0x20

start:
    cli
    xor ax,ax
    mov ss,ax
    mov sp,0x7000
    mov ds,ax
    mov es,ax
    cld
    ; Standard firmware enables host text video through main-board FDA0.
    ; The display sub-CPU may execute and acknowledge every mailbox command
    ; while the SDL frontend still suppresses ALL characters when FDA0[1:0]
    ; remains zero after reset. Enable text before transferring to the IPL.
    mov dx,0xFDA0
    mov al,0x03                 ; text display enabled (both text control bits)
    out dx,al
    mov dx,DIPSW
    in al,dx
    mov bl,al                    ; preserve active-low boot DIP
    test bl,0x20
    jnz .no2d
    push bx                      ; boot2d uses BX as DMA destination
    call boot2d
    pop bx                       ; POP does not modify success CF
    jnc boot_ipl
.no2d:
    test bl,0x40
    jnz .no2hd
    push bx                      ; boot2hd also uses BX as DMA destination
    call boot2hd
    pop bx
    jnc boot_ipl
.no2hd:
    test bl,0x80
    jnz boot_failure
    call boothdd
    jnc boot_ipl
boot_failure:
    ; All enabled devices absent or failed. Halt instead of running garbage.
    cli
.halt:
    hlt
    jmp .halt
boot_ipl:
    jmp BOOT_SEG:0

; Common 8237 setup, one sector per command.
; IN: DX=DMA address port (10h for 2D,12h for 2HD,14h for HDD)
;     BX=destination offset, CX=sector length (128 or 256)
;     AL=channel (0,1,2)
; OUT: selected channel unmasked, count programmed, terminal latch clear.
; Clobbers AX,DX,SI. Caller restores FDC port after this routine.
dma_read_setup:
    push bp
    mov bp,dx                    ; address port
    mov si,ax                    ; DMA channel number
    mov dx,DMA_MASK
    mov ax,si
    or al,4                     ; mask chosen channel
    out dx,al
    mov dx,DMA_FF
    xor al,al
    out dx,al
    mov dx,bp
    mov ax,bx
    out dx,al
    mov al,ah
    out dx,al
    inc dx                       ; count port
    mov ax,cx
    dec ax
    out dx,al
    mov al,ah
    out dx,al
    mov dx,DMA_BANK
    add dx,si
    xor al,al                    ; DMA bank 0
    out dx,al
    mov dx,DMA_MODE
    mov ax,si
    or al,0x44                  ; single mode + device->memory
    out dx,al
    mov dx,DMA_TC
    in al,dx                     ; clear previous TC latch
    mov dx,DMA_MASK
    mov ax,si                    ; unmask selected channel
    out dx,al
    pop bp
    ret

; FDC Type-I RESTORE, followed by bounded wait.
; IN DX = command/status port (FD18h or FD30h).
; OUT CF=0 when track0 is ready.
fdc_restore:
    mov al,0x0B
    out dx,al
    mov cx,0xFFFF
.wait:
    in al,dx
    cmp al,0xFF                  ; floating bus: no controller/media
    je .error
    test al,1
    jz .done
    loop .wait
    stc
    ret
.done:
    test al,0x90                 ; not ready / seek error
    jnz .error
    clc
    ret
.error:
    stc
    ret

; Bounded FDC READ status completion. DX=command/status port.
; The fast FDC implementation completes DMA synchronously; timed mode
; completes asynchronously and is polled. CF=1 for error or timeout.
fdc_read_wait:
    mov cx,0xFFFF
.wait:
    in al,dx
    test al,0x10                 ; record-not-found
    jnz .bad
    test al,3                    ; BUSY/DRQ
    jz .done
    loop .wait
.bad:
    stc
    ret
.done:
    or al,al
    jnz .bad
    clc
    ret

; 2D drive 0. The ROM does not need a full disk geometry: the IPL
; handles all remaining sectors itself.
boot2d:
    mov dx,0xFD1D
    xor al,al
    out dx,al                    ; drive 0 / motor
    mov dx,0xFD1C
    out dx,al                    ; head 0
    mov dx,0xFD18
    call fdc_restore
    jc .error
    mov bp,1                     ; physical sector 1 and 2
    mov bx,BOOT_ADDR
.next:
    mov dx,DMA_ADDR
    xor ax,ax                    ; channel 0
    mov cx,256
    call dma_read_setup
    mov dx,0xFD1A
    mov ax,bp
    out dx,al                    ; sector
    mov dx,0xFD18
    mov al,0x80                  ; READ SECTOR
    out dx,al
    call fdc_read_wait
    jc .error
    mov dx,DMA_TC
    in al,dx
    test al,1
    jz .error
    add bx,256
    inc bp
    cmp bp,3
    jb .next
    clc
    ret
.error:
    stc
    ret

; 2HD drive 0 special boot track: four 128-byte FM sectors.
boot2hd:
    mov dx,0xFD36
    in al,dx
    test al,1                    ; 2HD drive 0 present
    jz .error
    mov dx,0xFD35
    xor al,al
    out dx,al
    mov dx,0xFD34
    out dx,al
    mov dx,0xFD30
    call fdc_restore
    jc .error
    mov bp,1
    mov bx,BOOT_ADDR
.next:
    mov dx,DMA_ADDR+2
    mov ax,1                     ; channel 1
    mov cx,128
    call dma_read_setup
    mov dx,0xFD32
    mov ax,bp
    out dx,al
    mov dx,0xFD30
    mov al,0x80
    out dx,al
    call fdc_read_wait
    jc .error
    mov dx,DMA_TC
    in al,dx
    test al,2
    jz .error
    add bx,128
    inc bp
    cmp bp,5
    jb .next
    clc
    ret
.error:
    stc
    ret

; MDC HDD0. Binary LBA mode, DMA channel 2.
; Read LBA0 and 1 into contiguous IPL buffer.
boothdd:
    mov dx,0xFDC2
    mov al,0x20
    out dx,al
    mov dx,0xFDC0
    mov al,0x11                  ; SET CMASK
    out dx,al
    mov dx,0xFDC1
    mov al,0x70                  ; binary LBA addressing
    out dx,al
    xor al,al
    out dx,al
    call hdd_wait
    jc .error
    mov bp,0
    mov bx,BOOT_ADDR
.next:
    mov dx,DMA_ADDR+4
    mov ax,2
    mov cx,256
    call dma_read_setup
    mov dx,0xFDC2
    mov al,0x20
    out dx,al
    mov dx,0xFDC0
    mov al,0x22                  ; READ
    out dx,al
    mov dx,0xFDC1
    xor al,al
    out dx,al                    ; device 0
    out dx,al                    ; reserved
    out dx,al                    ; LBA[31:24]
    out dx,al                    ; LBA[23:16]
    out dx,al                    ; LBA[15:8]
    mov ax,bp
    out dx,al                    ; LBA[7:0]
    mov al,1
    out dx,al                    ; 0100h transfer bytes
    xor al,al
    out dx,al
    call hdd_wait
    jc .error
    mov dx,DMA_TC
    in al,dx
    test al,4
    jz .error
    add bx,256
    inc bp
    cmp bp,2
    jb .next
    clc
    ret
.error:
    stc
    ret

; Read 10-byte MDC result and check result[0] bit7.
; CF=0 success; CF=1 hardware error/timeout.
hdd_wait:
    push bx
    push cx
    push dx
    mov dx,0xFDC0
    mov cx,0xFFFF
.poll:
    in al,dx
    test al,0x40
    jnz .ready
    loop .poll
    stc
    jmp .done
.ready:
    mov dx,0xFDC1
    in al,dx
    mov ah,al
    mov cx,9
.drain:
    in al,dx
    loop .drain
    test ah,0x80
    jnz .fail
    clc
    jmp .done
.fail:
    stc
.done:
    pop dx
    pop cx
    pop bx
    ret

; Hardware reset execution starts at FFFF0h, not at the beginning of ROM.
times 0xFF0-($-$$) db 0xFF
reset_vector:
    jmp 0xFF00:start
times 0x1000-($-$$) db 0xFF

;
; a minimal dummy PBR for NEC PC-9801/9821 (for non-system partition)
;
; License: Public Domain
;          (You can use/modify/re-distribute it freely BUT NO WARRANTY)
;

    BITS 16
    CPU 8086
    ORG 0


%macro _org 1
  times (%1 - ($-top)) db 0
%endmacro

segment TEXT

top:
    jmp short start
    db 90h            ; nop
    db 'BTNPART '     ; OEM name

    ; extended BPB stub
    dw 512            ; bytes per (logical) sector
    db 0              ; sectors per cluster
    dw 1              ; reserved sectors (1...512bytes/sct, 4...256bytes/sct)
    db 2              ; number of FATs (2 as typical)
    dw 0              ; number of root directory entries
    dw 0              ; total sectors (less than 65536)
    db 0f8h           ; media desctiptor (F8h as non-removable medium)
    dw 0              ; sectors per a FAT
    dw 0              ; sectors per track
    dw 0              ; heads
    dd 0              ; hidden sectors  (= LBA offset of the partition)
    dd 0              ; total sectors (more than 65535)
    db 80h            ; drive number
    db 0              ;
    db 29h            ; signature for extended partition
    dd 0              ; serial no.
    db 'NO NAME    '  ; volume label
    db 'FAT16   '     ; file system

    dd 0              ; (NEC DOS5+ HD loader) LBA offset of the partition
    dw 0              ; (NEC DOS5+ HD loader) system (IO.SYS) offset
    dw 0              ; (NEC DOS5+ HD loader) bytes per (physical) sector
    db 0              ; (NEC DOS5+ HD loader) reserved?
    dd 0              ; (freedos98 loader) rootdir offset (by logical sector)
    dw 0              ; (freedos98 loader) rootdir count by logical sector
    dd 0              ; (freedos98 loader) fat offset (by logical sector)
    dd 0              ; (freedos98 loader) data offset (by logical sector)
    dd 0              ; (freedos98 loader) load address (0060:0000)


start:
    mov ax, 0a000h
    test byte [0501h], 8   ; 0000:0501 bit3 : hi-reso flag
    jz .l1
    mov ah, 0e0h
  .l1:
    mov es, ax
    push cs
    pop ds
    mov ah, 0ch       ; GDC start
    int 18h
    mov ah, 12h       ; hide cursor
    int 18h
    xor ax, ax
    xor di, di
    mov si, msg_nosystem
  .lp:
    lodsb
    or al, al
    jz .l2
    stosw
    jmp short .lp
  .l2:
    
  .waitlp:
    sti
    nop
    hlt               ; halt cpu until an interruption.
    mov ah, 05h
    int 18h
    or bh, bh
    jz .waitlp
  .reboot:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov dx, [0486h]   ; for a proof: restore DX (initial value after reset)
    jmp 0f000h:0fff0h ; 0ffffh:0000h for old 8086/186
    

msg_nosystem  db 'NO SYSTEM DISK. PRESS ANY KEY TO REBOOT.', 0

    _org 01feh

    db 55h, 0aah


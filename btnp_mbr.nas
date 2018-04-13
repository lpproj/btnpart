;
; a minimal HD MBR for NEC PC-9801/9821
; (just chain to boot sector in the boot partition)
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
    jmp short startup_zero
    db 90h, 90h
    db 'IPL1'
bootmenu_entry:     ; load & start address for bootmenu
    dw 0
    dw 1e00h

startup_zero:
    push ds
    push cs
    pop ds
    xor si, si
    mov es, [bootmenu_entry + 2]
    xor di, di
    mov cx, 512
    cld
    rep movsw
    mov si, startup_real
    pop ds
    push es       ; jump new entry
    push si
    retf

startup_real:
    mov al, [0584h] ; 0000:0584 DISK_BOOT
    mov ah, 8eh     ; wake up, HDD
    int 1bh
    mov ah, 84h     ; New Sense (to get bytes per sector)
    mov bx, 256
    int 1bh
    mov si, [cs: bx - 5]  ; get autoboot_part
    and si, 1fh
    mov cl, 5
    shl si, cl
    add si, bx
    
    mov ah, 06h           ; load PBR (IPL in the partition)
    mov bp, 1f80h
    mov es, bp
    xor bp, bp
    mov dx, [cs: si + 4]  ; ipl h, s
    mov cx, [cs: si + 6]  ; ipl c
    mov bx, 1024
    int 1bh
    xor ah, ah    ; is it needed?
    push es       ; jump to PBR
    push bp
    retf
    



    ; boot signeture (for 256bytes per sector)
    _org 0fah

autoboot:
    db 00h        ; 80h: boot without interactive bootmenu
autoboot_part:
    db 0          ; index of boot partition for autoboot
bootmenu_version:
    db 0          ; version of bootmenu
    db 0
    db  55h, 0aah ; PC-9801 `Extended' partitioning disk

    ; ditto (for 512bytes per sector)
    _org 1fah

;autoboot:
    db 00h        ; 80h: boot without interactive bootmenu
;autoboot_part:
    db 0          ; index of boot partition for autoboot
;bootmenu_version:
    db 0          ; version of bootmenu
    db 0
    db  55h, 0aah


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

%define STACK_SIZE_PARA		20h


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

msg_not98:
    db 'This disk is formatted for NEC PC-98 series.', 0

startup_zero:
    cld
    push ds
;.check_nec98_or_ibmpc:
    mov ax, 0ffffh
    mov ds, ax
    cmp word [0003h], 0fd80h
    push cs
    pop ds
    je .nec98
    mov ah, 0fh
    int 10h
    cmp ah, 0fh
    je .nec98
;.ibmpc:
    call .ibmpc_l0
.ibmpc_l0:
    pop si              ; mov si, .ibmpc_l0
    sub si, .ibmpc_l0 - msg_not98
    xor bx, bx
.ibmpc_lp:
    lodsb
    test al, al
    jz .ibmpc_hlt
    mov ah, 0eh
    int 10h
    jmp short .ibmpc_lp
.ibmpc_hlt:
    hlt
    jmp short .ibmpc_hlt

.nec98:
    xor si, si
    mov es, [bootmenu_entry + 2]
    xor di, di
    mov cx, 512
    rep movsw
    mov si, startup_real
    pop ds
    push es       ; jump new entry
    push si
    retf

startup_real:
    mov [cs: org_sp], sp
    mov [cs: org_ss], ss
    push cs
    pop ax
    sub ax, STACK_SIZE_PARA
    mov ss, ax
    mov sp, (STACK_SIZE_PARA) * 16
    
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
    mov ss, [cs: org_ss]
    mov sp, [cs: org_sp]
    push es       ; jump to PBR
    push bp
    retf
    

org_sp  dw 0
org_ss  dw 0

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


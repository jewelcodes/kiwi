;
; kiwi - general-purpose high-performance operating system
;
; Copyright (c) 2025 Omar Elghoul
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in all
; copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
; SOFTWARE.
;

[bits 16]
[org 0]

%define SEGMENT                         0x4000
%define STACK_SEGMENT                   0x5000

%define STAGE2_SEGMENT                  0x0000
%define STAGE2_OFFSET                   0x0500

%define SECTOR_SIZE                     512
%define SUPERBLOCK_BLOCK_SIZE           24
%define SUPERBLOCK_BLOCKS_PER_GROUP     32
%define SUPERBLOCK_INODES_PER_GROUP     40
%define SUPERBLOCK_SIGNATURE_OFFSET     56
%define SUPERBLOCK_SIGNATURE            0xEF53

%define SUPERBLOCK_VERSION_MAJOR        76
%define SUPERBLOCK_VERSION_MINOR        62
%define SUPERBLOCK_INODE_SIZE           88

%define BGDT_INODE_TABLE                8

%define INODE_TYPE                      0
%define INODE_TYPE_MASK                 0xF000
%define INODE_TYPE_DIRECTORY            0x4000
%define INODE_TYPE_REGULAR              0x8000

%define INODE_DIRECT_PTR_0              40
%define INODE_DIRECT_PTR_1              44
%define INODE_DIRECT_PTR_2              48
%define INODE_DIRECT_PTR_3              52
%define INODE_DIRECT_PTR_4              56
%define INODE_DIRECT_PTR_5              60
%define INODE_DIRECT_PTR_6              64
%define INODE_DIRECT_PTR_7              68
%define INODE_DIRECT_PTR_8              72
%define INODE_DIRECT_PTR_9              76
%define INODE_DIRECT_PTR_10             80
%define INODE_DIRECT_PTR_11             84
%define INODE_DIRECT_PTR_COUNT          12

%define INODE_SINGLY_INDIRECT_PTR       88
%define INODE_DOUBLY_INDIRECT_PTR       92
%define INODE_TRIPLY_INDIRECT_PTR       96

%define DIRECTORY_INODE                 0
%define DIRECTORY_ENTRY_LENGTH          4
%define DIRECTORY_NAME_LENGTH           6
%define DIRECTORY_NAME                  8

_start:
    cli
    cld

    mov ax, STACK_SEGMENT
    mov ss, ax
    mov sp, 0xFFF0

    sti

    xor ax, ax
    mov es, ax

    ; check if we were passed a partition table by the MBR
    mov di, boot_partition + 0x7C00
    mov cx, 16 / 2
    cmp byte [si], 0x80
    jnz .no_partition

    rep movsw           ; copy the partition table

    jmp .relocate

.no_partition:
    rep stosw           ; zero out the partition table

    sti

.relocate:
    ; relocate to higher memory
    xor ax, ax
    mov di, ax          ; di = 0
    mov ds, ax          ; ds = 0
    mov ax, SEGMENT
    mov es, ax
    mov si, 0x7C00
    mov cx, _end - _start
    rep movsb

    jmp SEGMENT:_main

_main:
    mov ds, ax
    mov [bootdisk], dl

    sti

    ; load the second half of the boot sector
    mov eax, 1
    add eax, [boot_partition.start_lba]
    mov cx, 1
    mov di, 0x200
    call read_sectors
    jc error.disk

    ; superblock
    mov eax, 2
    add eax, [boot_partition.start_lba]
    mov cx, 2
    mov di, superblock
    call read_sectors
    jc error.disk

    ; verify the superblock
    mov ax, [superblock + SUPERBLOCK_SIGNATURE_OFFSET]
    cmp ax, SUPERBLOCK_SIGNATURE
    jnz error.bad_superblock

    mov al, [superblock + SUPERBLOCK_BLOCK_SIZE]
    mov cl, al
    mov bx, 2
    shl bx, cl
    mov [sectors_per_block], bx

    and al, al
    jz .bgdt_1024

.bgdt_other:
    mov dword [bgdt_block], 1   ; starts at block 1 for any non-1024 block size
    jmp .bgdt_done

.bgdt_1024:
    mov dword [bgdt_block], 2

.bgdt_done:
    ; detect inode size
    mov ax, [superblock + SUPERBLOCK_VERSION_MAJOR]
    and ax, ax
    jz .default

    mov ax, [superblock + SUPERBLOCK_INODE_SIZE]
    mov word [inode_size], ax
    jmp .inode_size_done

.default:
    mov word [inode_size], 128

.inode_size_done:
    ; load the root inode (inode 2)
    mov eax, 2
    mov di, block_buffer
    call read_inode
    jc error.disk

    ; find the directory
    mov si, block_buffer
    xor ax, ax

.dir_loop:
    add si, ax
    mov ax, [si + DIRECTORY_ENTRY_LENGTH]
    and ax, ax
    jz error.not_found

    cmp byte [si + DIRECTORY_NAME_LENGTH], dir_name_size
    jnz .dir_loop

    mov cx, dir_name_size
    mov di, dir_name
    push si
    add si, DIRECTORY_NAME
    rep cmpsb
    pop si
    jnz .dir_loop

    ; found the directory, load this inode
    mov eax, [si + DIRECTORY_INODE]
    mov di, block_buffer
    call read_inode
    jc error.disk

    ; now look for the file
    mov si, block_buffer
    xor ax, ax

.file_loop:
    add si, ax
    mov ax, [si + DIRECTORY_ENTRY_LENGTH]
    and ax, ax
    jz error.not_found

    cmp byte [si + DIRECTORY_NAME_LENGTH], file_name_size
    jnz .file_loop

    mov cx, file_name_size
    mov di, file_name
    push si
    add si, DIRECTORY_NAME
    rep cmpsb
    pop si
    jnz .file_loop

    ; found the file, load this inode
    mov eax, [si + DIRECTORY_INODE]
    mov dx, STAGE2_SEGMENT
    mov es, dx
    mov di, STAGE2_OFFSET
    call read_inode
    jc error.disk

    mov dl, [bootdisk]
    mov si, boot_partition
    jmp STAGE2_SEGMENT:STAGE2_OFFSET

; print_string:
; @brief: prints a string to the screen
; @input: ds:si = string pointer
; @output: none

print_string:
    pusha

.loop:
    lodsb
    and al, al
    jz .done
    mov ah, 0x0E
    int 0x10
    jmp .loop

.done:
    popa
    ret

; print_number:
; @brief: prints a decimal number
; @input: ax = number
; @output: none

print_number:
    pusha

    mov cx, 0
    mov bx, 10

.get_digits:
    xor dx, dx
    div bx
    push dx
    inc cx
    and ax, ax
    jnz .get_digits

.print:
    pop ax
    add al, '0'
    mov ah, 0x0E
    push cx
    int 0x10
    pop cx
    loop .print

.done:
    popa
    ret

; read_sectors:
; @brief: reads sectors from disk using int 0x13 extensions
; @input: eax = lba address
;         cx = count
;         es:di = buffer
; @output: cf == 1 on error

read_sectors:
    mov byte [disk_address_packet.size], 0x10
    mov byte [disk_address_packet.reserved], 0
    mov [disk_address_packet.count], cx
    mov [disk_address_packet.offset], di
    mov [disk_address_packet.segment], es
    mov [disk_address_packet.lba], eax

    clc
    mov ah, 0x42
    mov dl, [bootdisk]
    mov si, disk_address_packet
    int 0x13

    ret

; read_block:
; @brief: reads a filesystem block
; @input: eax = block number
;         es:di = buffer
; @output: cf == 1 on error

read_block:
    movzx ebx, word [sectors_per_block]
    mul ebx
    movzx ecx, word [sectors_per_block]
    add eax, [boot_partition.start_lba]
    call read_sectors

    ret

error:

.disk:
    mov si, disk_error
    call print_string
    jmp .halt

.bad_superblock:
    mov si, superblock_error
    call print_string
    jmp .halt

.not_found:
    mov si, not_found_error
    call print_string

.halt:
    sti
    hlt
    jmp .halt

newline:                            db 13, 10, 0
disk_error:                         db "disk I/O error", 0
superblock_error:                   db "bad superblock", 0
not_found_error:                    db "/boot/bootman.bin not found", 0

padding1:                           times 510 - ($ - $$) db 0
boot_signature:                     dw 0xAA55

; --- second half of the boot sector begins here ---

; read_inode:
; @brief: reads an inode from the filesystem
; @input: eax = inode number
;         es:di = buffer
; @output: cf == 1 on error
;          al == non-zero if directory

read_inode:
    mov dx, es
    mov [.segment], dx
    mov [.offset], di

    mov dx, SEGMENT
    mov es, dx

    ; calculate block group
    dec eax
    mov ebx, [superblock + SUPERBLOCK_INODES_PER_GROUP]
    xor edx, edx
    div ebx

    ; eax = block group number
    ; edx = index within block group

    mov [.block_group_number], eax
    mov [.index_within_group], edx

    ; 1024 bytes of BGDT fits 32 block groups
    ; so determine which block of the BGDT to read

    shl eax, 5                  ; eax = total offset into BGDT, block group * 32
    mov ebx, eax
    shr ebx, 10                 ; ebx = 1 KB relative block number within BGDT
    mov cl, [superblock + SUPERBLOCK_BLOCK_SIZE]
    shr ebx, cl                 ; ebx = relative BGDT block number
    add ebx, [bgdt_block]       ; ebx = absolute BGDT block number
    mov [.bgdt_block_number], ebx
    mov eax, ebx
    mov di, bgdt
    call read_block
    jc error.disk

    mov si, bgdt
    mov eax, [.block_group_number]
    and al, 0x1F
    shl ax, 5
    add si, ax
    mov eax, [si + BGDT_INODE_TABLE]
    mov [.inode_table_base], eax

    mov eax, [.index_within_group]
    movzx ebx, word [inode_size]
    mul ebx                     ; eax = byte offset within inode table
    mov ebx, 1024
    mov cl, [superblock + SUPERBLOCK_BLOCK_SIZE]
    shl ebx, cl                 ; ebx = number of sectors per block
    xor edx, edx
    div ebx

    add eax, [.inode_table_base]
    mov [.inode_block], eax
    mov [.inode_offset], edx
    mov di, inode_table
    call read_block
    jc error.disk

    mov ebx, [.inode_offset]
    mov si, inode_table
    add si, bx
    mov [.inode_ptr], si

    mov ax, [si + INODE_TYPE]
    and ax, INODE_TYPE_MASK
    cmp ax, INODE_TYPE_DIRECTORY
    jz .is_directory

    xor al, al
    jmp .read_begin

.is_directory:
    mov al, 1

.read_begin:
    mov [.type], al

    ; now we can begin to read the inode data
    mov ax, [.segment]
    mov es, ax

    mov si, [.inode_ptr]
    add si, INODE_DIRECT_PTR_0
    mov cx, INODE_DIRECT_PTR_COUNT

.direct_loop:
    lodsd
    and eax, eax
    jz .check_singly

    push cx

    push si
    mov di, [.offset]
    call read_block
    pop si
    jc error.disk

    mov cl, [superblock + SUPERBLOCK_BLOCK_SIZE]
    mov bx, 1024
    shl bx, cl
    add [.offset], bx
    pop cx
    loop .direct_loop

.check_singly:
    ; check if we need to read singly-linked indirect blocks
    mov si, [.inode_ptr]
    add si, INODE_SINGLY_INDIRECT_PTR
    mov eax, [si]
    and eax, eax
    jz .done

    mov dx, SEGMENT
    mov es, dx
    mov di, singly_block
    call read_block
    jc error.disk

    mov ax, [.segment]
    mov es, ax

    mov bx, 1024 / 4
    mov cl, [superblock + SUPERBLOCK_BLOCK_SIZE]
    shl bx, cl          ; bx = number of block pointers per block
    mov cx, bx
    mov si, singly_block

.singly_loop:
    lodsd
    and eax, eax
    jz .done

    push cx

    push si
    mov di, [.offset]
    call read_block
    pop si
    jc error.disk

    mov cl, [superblock + SUPERBLOCK_BLOCK_SIZE]
    mov bx, 1024
    shl bx, cl
    add [.offset], bx
    pop cx
    loop .singly_loop

.done:
    ret

    align 4
    .block_group_number:            resb 4
    .index_within_group:            resb 4
    .bgdt_block_number:             resb 4
    .inode_table_base:              resb 4
    .inode_block:                   resb 4
    .inode_offset:                  resb 4
    .inode_ptr:                     resb 4
    .segment:                       resb 2
    .offset:                        resb 2
    .type:                          resb 1

    dir_name:                       db "boot"
    dir_name_size:                  equ $ - dir_name
    file_name:                      db "bootman.bin"
    file_name_size:                 equ $ - file_name

padding2:                           times 1024 - ($ - $$) db 0
bootdisk:                           resb 1
sectors_per_block:                  resb 2
bgdt_block:                         resb 4
inode_size:                         resb 2

align 4
disk_address_packet:
    .size                           resb 1
    .reserved                       resb 1
    .count                          resb 2
    .offset                         resb 2
    .segment                        resb 2
    .lba                            resb 8

boot_partition:
    .bootable:                      resb 1
    .start_chs:                     resb 3
    .type:                          resb 1
    .end_chs:                       resb 3
    .start_lba:                     resb 4
    .size:                          resb 4

superblock:                         resb 4096
inode_table:                        resb 4096
bgdt:                               resb 4096
singly_block:                       resb 4096
block_buffer:                       resb 4096

_end:

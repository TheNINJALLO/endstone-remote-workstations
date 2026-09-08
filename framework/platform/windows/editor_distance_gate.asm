; Exact Windows BDS call gate. The patch uses CALL (not JMP), so this is a
; normal function frame with unwind metadata and a real BDS return address.
; The original branch continuation replaces that return address before RET.
option casemap:none
EXTERN rw_command_distance_allowed:PROC
EXTERN rw_command_accept:QWORD
EXTERN rw_command_reject:QWORD
EXTERN rw_command_entity_accept:QWORD
EXTERN rw_command_entity_reject:QWORD
EXTERN rw_structure_distance_allowed:PROC
EXTERN rw_structure_accept:QWORD
EXTERN rw_structure_reject:QWORD
.code
COMMAND_GATE MACRO gate_name, accept_address, reject_address, policy
LOCAL admit, deny, restore
PUBLIC gate_name
gate_name PROC FRAME
    sub rsp,0e8h
    .allocstack 0e8h
    .endprolog
    mov qword ptr [rsp+80h],rax
    mov qword ptr [rsp+88h],rcx
    mov qword ptr [rsp+90h],rdx
    mov qword ptr [rsp+98h],r8
    mov qword ptr [rsp+0a0h],r9
    mov qword ptr [rsp+0a8h],r10
    mov qword ptr [rsp+0b0h],r11
    movdqu xmmword ptr [rsp+20h],xmm0
    movdqu xmmword ptr [rsp+30h],xmm1
    movdqu xmmword ptr [rsp+40h],xmm2
    movdqu xmmword ptr [rsp+50h],xmm3
    movdqu xmmword ptr [rsp+60h],xmm4
    movdqu xmmword ptr [rsp+70h],xmm5
    ucomiss xmm0,xmm1
    lahf
    mov byte ptr [rsp+0d8h],ah
    mov rcx,rsi ; Actual ServerPlayer, verified a64f3b basic block.
    mov rdx,rdi ; Actual CommandBlockUpdatePacket const&.
    call policy
    cmp eax,1
    je admit
    cmp eax,2
    je deny
    test byte ptr [rsp+0d8h],1
    jnz deny
admit:
    and byte ptr [rsp+0d8h],0feh
    mov rax,qword ptr [accept_address]
    jmp restore
deny:
    or byte ptr [rsp+0d8h],1
    mov rax,qword ptr [reject_address]
restore:
    mov qword ptr [rsp+0e8h],rax
    movdqu xmm0,xmmword ptr [rsp+20h]
    movdqu xmm1,xmmword ptr [rsp+30h]
    movdqu xmm2,xmmword ptr [rsp+40h]
    movdqu xmm3,xmmword ptr [rsp+50h]
    movdqu xmm4,xmmword ptr [rsp+60h]
    movdqu xmm5,xmmword ptr [rsp+70h]
    mov rcx,qword ptr [rsp+88h]
    mov rdx,qword ptr [rsp+90h]
    mov r8,qword ptr [rsp+98h]
    mov r9,qword ptr [rsp+0a0h]
    mov r10,qword ptr [rsp+0a8h]
    mov r11,qword ptr [rsp+0b0h]
    xor eax,eax ; UCOMISS cleared OF; restore its other status flags below.
    mov ah,byte ptr [rsp+0d8h]
    sahf
    mov rax,qword ptr [rsp+80h]
    lea rsp,[rsp+0e8h]
    ret
gate_name ENDP
ENDM
COMMAND_GATE rw_command_gate, rw_command_accept, rw_command_reject, rw_command_distance_allowed
COMMAND_GATE rw_command_entity_gate, rw_command_entity_accept, rw_command_entity_reject, rw_command_distance_allowed
COMMAND_GATE rw_structure_gate, rw_structure_accept, rw_structure_reject, rw_structure_distance_allowed
END

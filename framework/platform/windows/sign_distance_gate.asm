; BDS 1.26.45.1 Windows only. Both verified basic blocks enter with
; RSP 16-byte aligned, RSI=ServerPlayer, R15=BlockActorDataPacket.
; Preserve all volatile GPRs, XMM0..5 and the native comparison flags.
; The noexcept policy returns 0=original distance, 1=admit owned save,
; 2=reject a retired owned save, including when the player moved nearby.
option casemap:none
EXTERN rw_sign_distance_allowed:PROC
EXTERN rw_sign_accept_0:QWORD
EXTERN rw_sign_reject_0:QWORD
EXTERN rw_sign_accept_1:QWORD
EXTERN rw_sign_reject_1:QWORD
.code
SIGN_GATE MACRO name:req, accepted:req, rejected:req, stage:req
LOCAL restore, pass
PUBLIC name
name PROC
    ucomiss xmm0,xmm1
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp,80h
    movdqu xmmword ptr [rsp+20h],xmm0
    movdqu xmmword ptr [rsp+30h],xmm1
    movdqu xmmword ptr [rsp+40h],xmm2
    movdqu xmmword ptr [rsp+50h],xmm3
    movdqu xmmword ptr [rsp+60h],xmm4
    movdqu xmmword ptr [rsp+70h],xmm5
    mov rcx,rsi
    mov rdx,r15
    mov r8d,stage
IF stage EQ 0
    mov r9,qword ptr [rbp+1f8h]
ELSE
    lea r9,[rdi+0b8h]
ENDIF
    call rw_sign_distance_allowed
    cmp al,1
    jne @F
    and qword ptr [rsp+0b8h],-2
    jmp restore
@@:
    cmp al,2
    jne restore
    or qword ptr [rsp+0b8h],1
restore:
    movdqu xmm0,xmmword ptr [rsp+20h]
    movdqu xmm1,xmmword ptr [rsp+30h]
    movdqu xmm2,xmmword ptr [rsp+40h]
    movdqu xmm3,xmmword ptr [rsp+50h]
    movdqu xmm4,xmmword ptr [rsp+60h]
    movdqu xmm5,xmmword ptr [rsp+70h]
    add rsp,80h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    jnc pass
    jmp qword ptr [rejected]
pass:
    jmp qword ptr [accepted]
name ENDP
ENDM
SIGN_GATE rw_sign_gate_0,rw_sign_accept_0,rw_sign_reject_0,0
SIGN_GATE rw_sign_gate_1,rw_sign_accept_1,rw_sign_reject_1,1
END

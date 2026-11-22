@ Battery reply for Keychron's official Q6 Ultra 8K ANSI firmware v1.0.4-ZK (build Feb 10 2026 09:47:25).
@ Adds raw HID command A7 31 -> reply [A7][31][battery %][charging], the layout Keychron's own Nape Pro uses.
@ Reference source for the bytes in q6_ultra_8k_patch.py; file offset 0 of the image is address 0x0402D000.
@
@ .hook replaces "cmp r3,#13 ; bhi.n exit" in kc_user_cmd (0x0404A73E) with a jump to the cave.
@ .cave replaces single_tone_test_start (0x04050DD0, factory radio carrier test, one caller), which becomes a no-op.
@ TBB_SITE (0x0404A742) is the instruction after the hook, EXIT_SITE (0x0404A768) the handler's epilogue.
@ Function and variable names are from Keychron's source (github.com/Keychron/zmk, branch rtl8762g).
    .syntax unified
    .cpu cortex-m33
    .thumb

    .section .hook, "ax", %progbits
    b.w     cave_entry

    .section .cave, "ax", %progbits
    bx      lr                      @ single_tone_test_start: do nothing
cave_entry:                         @ r3 = sub-command - 1, r4 = command_data (byte 0 = sub-command)
    cmp     r3, #13
    bhi.n   1f
    b.w     TBB_SITE                @ sub-commands 1..14: original jump table, r3 untouched
1:  cmp     r3, #0x30               @ sub-command 0x31
    bne.n   2f
    ldr     r3, bat_level_ptr
    ldrb    r3, [r3]
    strb    r3, [r4, #1]            @ reply byte 2: battery percent
    ldr     r3, chg_state_ptr
    ldrb    r3, [r3]
    strb    r3, [r4, #2]            @ reply byte 3: charging flag
2:  b.w     EXIT_SITE               @ add sp,#16 ; pop {r4,r5,r6,pc}
    .balign 4
bat_level_ptr:
    .word   0x0012E981              @ last_state_of_charge (battery.c)
chg_state_ptr:
    .word   0x0010944D              @ bat_charge_state (behavior_keychron.c)

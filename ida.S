#define LED_MATRIX_0_BASE (0xf0000000)
#define LED_MATRIX_0_WIDTH (0x08)
#define LED_MATRIX_0_HEIGHT (0x06)

.equ RENDER, 1
.equ FRAME_DELAY, 10000

.equ PERMUTATIONS, 5040
.equ ORIENTATIONS, 729
.equ MAX_SOLUTION, 11
.equ MAX_MOVE, 9

.section .rodata

move_names:
    .word .Lmove_R, .Lmove_R2, .Lmove_Rprime
    .word .Lmove_B, .Lmove_B2, .Lmove_Bprime
    .word .Lmove_D, .Lmove_D2, .Lmove_Dprime

.Lmove_R:      .string "R "
.Lmove_R2:     .string "R2 "
.Lmove_Rprime: .string "R' "
.Lmove_B:      .string "B "
.Lmove_B2:     .string "B2 "
.Lmove_Bprime: .string "B' "
.Lmove_D:      .string "D "
.Lmove_D2:     .string "D2 "
.Lmove_Dprime: .string "D' "

move_faces:
    .byte 0, 0, 0, 1, 1, 1, 2, 2, 2
face_end_moves:
    .byte 3, 3, 3, 6, 6, 6, 9, 9, 9
turn_indices:
    .byte 0, 1, 2, 0, 1, 2, 0, 1, 2
.balign 4
face_tables:
    .word permutation, orientation
    .word permutation + 2*PERMUTATIONS, orientation + 2*ORIENTATIONS
    .word permutation + 4*PERMUTATIONS, orientation + 4*ORIENTATIONS
newline: .string "\n"

.extern permutation
.extern orientation
.extern table
.extern table_prefix

.balign 4
.type popcount, @object
popcount:
    .byte 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
    .byte 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5
    .byte 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5
    .byte 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6
.size popcount, .-popcount

.balign 4
input:
    .string "21345671111111"

# Private search workspace and call stack for the LED renderer.
.section .bss
.balign 4
state: .zero 14
.balign 4
stack: .zero 8*(MAX_SOLUTION + 1)
solution: .zero MAX_SOLUTION + 1
.balign 16
vis_call_stack: .zero 16
vis_call_stack_top:

.section .text
.globl main
.type main, @function
main:
    la sp, vis_call_stack_top
    la a0, input
    la a1, state
    jal parse_state
.if RENDER
    la a0, input
    la a1, vis_state
    jal parse_state
    jal vis_draw
.endif
    la a0, state
    jal rank_state
    jal ida_star
    jal append_table_solution
    jal print
    li a7, 10
    ecall
.size main, .-main

.globl parse_state
.type parse_state, @function
parse_state:
    li t0, 14
.Lparse_digit:
    lbu t1, 0(a0)
    addi t1, t1, -49
    sb t1, 0(a1)
    addi a0, a0, 1
    addi a1, a1, 1
    addi t0, t0, -1
    bnez t0, .Lparse_digit
    ret
.size parse_state, .-parse_state

.macro mul2 rd, rs
    slli \rd, \rs, 1
.endm

.macro mul3 rd, rs
    slli t6, \rs, 1
    add  \rd, t6, \rs
.endm

.macro mul4 rd, rs
    slli \rd, \rs, 2
.endm

.macro mul5 rd, rs
    slli t6, \rs, 2
    add  \rd, t6, \rs
.endm

.macro mul6 rd, rs
    slli t6, \rs, 1
    add  t6, t6, \rs
    slli \rd, t6, 1
.endm

.macro rank_digit offset, factor
    lbu t5, \offset(a0)
    sll t4, a2, t5
    addi t3, t4, -1
    and t3, t3, t0
    .if \factor != 2
        xor t0, t0, t4
    .endif
    add t3, t2, t3
    lbu t3, 0(t3)
    mul\factor t1, t1
    add t1, t1, t3
.endm

.macro rank_orientation_digit offset
    mul3 a1, a1
    lbu t5, \offset(a0)
    add a1, a1, t5
.endm

# p at a0, o at a1
.globl rank_state
.type rank_state, @function
rank_state:
.rank_permutation:
    li a2, 1
    la t2, popcount
    lbu t1, 0(a0) 
    sll t4, a2, t1
    li t0, 127
    xor t0, t0, t4
    rank_digit 1, 6
    rank_digit 2, 5
    rank_digit 3, 4
    rank_digit 4, 3
    rank_digit 5, 2
.rank_orientation:
    lbu a1, 7(a0)
    rank_orientation_digit 8
    rank_orientation_digit 9
    rank_orientation_digit 10
    rank_orientation_digit 11
    rank_orientation_digit 12
    mv a0, t1
    ret
.size rank_state, .-rank_state

# dense_key: ((p + o*5040) << 8);
.macro dense_key rd, p, o
    slli t5, \o, 2
    add t5, t5, \o
    slli \rd, t5, 6
    sub \rd, \rd, t5
    slli \rd, \rd, 4
    add \rd, \rd, \p
    slli \rd, \rd, 8
.endm

.macro search_table entry, p, o
    dense_key t6, \p, \o
    srli t5, t6, 22
    slli t5, t5, 1
    add t5, s11, t5
    # lo at t4, hi at t3
    lhu t4, 0(t5)
    lhu t3, 2(t5)
    slli t4, t4, 2
    slli t3, t3, 2
    add t4, s10, t4
    add t3, s10, t3
    mv t5, t3
.sloop\@:
    bgeu t4, t3, .scheck\@
    sub t2, t3, t4
    srli t2, t2, 1
    andi t2, t2, -4
    add t2, t4, t2
    lw t1, 0(t2)
    bltu t1, t6, .sright\@
    mv t3, t2
    j .sloop\@
.sright\@:
    addi t4, t2, 4
    j .sloop\@
.scheck\@:
    beq t4, t5, .smiss\@
    lw t1, 0(t4)
    srli t1, t1, 8
    srli t6, t6, 8
    bne t1, t6, .smiss\@
    mv \entry, t4
    j .send\@
.smiss\@:
    li \entry, 0
.send\@:
.endm

# stack[depth] at s0, threshold at s1, next_threshold at s2, MAX_SOLUTION at s3, depth at s4, solution[depth] at s5
# return stop_p at a0, stop_o at a1, solution[depth] at a2
.globl ida_star
.type ida_star, @function
ida_star:
    la s10, table
    la s11, table_prefix
    la s5, solution
    search_table t0, a0, a1
    bnez t0, .find
    li s1, 6
    li s3, MAX_SOLUTION
    
.ida_loop:
    li s2, MAX_SOLUTION+1
    la s0, stack
    la s5, solution
    li s4, 0

    sh a0, 0(s0)
    sh a1, 2(s0)
    li t0, 0x03010000
    sw t0, 4(s0)

.inner_loop:
    lhu a0, 0(s0)
    lhu a1, 2(s0)
    lbu a2, 4(s0)
    lbu a3, 5(s0)
    lbu a4, 6(s0)
    lbu a5, 7(s0)

    bnez a3, .not_first
    search_table t0, a0, a1
    li t1, 6
    beqz t0, .heuristic_ready
    lbu t1, 0(t0)
    srli t1, t1, 4
.heuristic_ready:
    add t1, s4, t1
    bltu s1, t1, .update_threshold
    bnez t0, .find
    j .not_first

.update_threshold:
    bgeu t1, s2, .cut
    mv s2, t1
.cut:
    beqz s4, .next_threshold
    addi s4, s4, -1
    addi s0, s0, -8
    addi s5, s5, -1
    j .inner_loop
.not_first:
    bgeu s4, s3, .cut
    li t0, MAX_MOVE
    bgeu a2, t0, .cut
    
    bne a3, a5, .no_skip_face
    addi a2, a2, 3
    addi a3, a3, 1
    sb a2, 4(s0)
    sb a3, 5(s0)
    j .inner_loop
.no_skip_face:
    addi t0, a2, 1
    sb t0, 4(s0)
    addi t0, a4, 1
    li t1, 4
    bne t0, t1, .store_turns
    li t0, 1
    addi t1, a3, 1
    sb t1, 5(s0)
.store_turns:
    sb t0, 6(s0)
    sb a2, 0(s5)

    la t0, face_tables
    slli t1, a3, 3
    add t0, t0, t1
    lw t2, 0(t0)
    lw t3, 4(t0)
    beqz a4, .child_ready
.child_turn:
    slli t0, a0, 1
    add t0, t2, t0
    lhu a0, 0(t0)
    slli t0, a1, 1
    add t0, t3, t0
    lhu a1, 0(t0)
    addi a4, a4, -1
    bnez a4, .child_turn
.child_ready:
    addi s4, s4, 1
    addi s0, s0, 8
    addi s5, s5, 1
    sh a0, 0(s0)
    sh a1, 2(s0)
    sh x0, 4(s0)
    li t0, 1
    sb t0, 6(s0)
    sb a3, 7(s0)

    j .inner_loop

.next_threshold:
    mv s1, s2
    bgeu s3, s1, .ida_loop
.find:
    mv a2, s5
    ret
.size ida_star, .-ida_star


# stop_p at a0, stop_o at a1, solution[depth] at a2.
.globl append_table_solution
.type append_table_solution, @function
append_table_solution:
    la s10, table
    la s11, table_prefix
.alookup:
    search_table t0, a0, a1
    lw t0, 0(t0)
    andi t1, t0, 240
    beqz t1, .aend
    andi a3, t0, 15
    sb a3, 0(a2)
    addi a2, a2, 1

    la t0, move_faces
    add t0, t0, a3
    lbu t1, 0(t0)
    la t0, face_tables
    slli t1, t1, 3
    add t0, t0, t1
    lw a4, 0(t0)
    lw a5, 4(t0)
    la t0, turn_indices
    add t0, t0, a3
    lbu a3, 0(t0)
    addi a3, a3, 1
.aturn:
    slli t0, a0, 1
    add t0, a4, t0
    lhu a0, 0(t0)
    slli t0, a1, 1
    add t0, a5, t0
    lhu a1, 0(t0)
    addi a3, a3, -1
    bnez a3, .aturn
    j .alookup
.aend:
    li t0, 255
    sb t0, 0(a2)
    la a0, solution
    ret
.size append_table_solution, .-append_table_solution

.globl print
.type print, @function
# solution at s0, move string at t4
print:
.if RENDER
    addi sp, sp, -16
    sw ra, 0(sp)
.endif
    mv s0, a0
.print_loop:
    li t0, 255
    la t1, move_names
    lbu t2, 0(s0)
    beq t2, t0, .print_end
    slli t3, t2, 2
    add t3, t3, t1
    lw t4, 0(t3)
    # Print one move.
.print_char:
    lbu a0, 0(t4)
    beqz a0, .print_draw
    li a7, 11
    ecall
    addi t4, t4, 1
    j .print_char
.print_draw:
.if RENDER
    # Apply the move and update LEDs.
    lbu a0, 0(s0)
    jal vis_move
    jal vis_draw
.endif
    addi s0, s0, 1
    j .print_loop
.print_end:
.if RENDER
    lw ra, 0(sp)
    addi sp, sp, 16
.endif
    li a0, 10
    li a7, 11
    ecall
    ret
.size print, .-print


.if RENDER
.section .rodata
# Faces: U, L, F, R, B, D.
vis_corner_faces:
    .byte 0,1,2, 0,2,3, 5,3,2, 5,2,1
    .byte 0,3,4, 5,4,3, 5,1,4, 0,4,1
# Corner positions for each face, row-major from outside.
vis_face_corners:
    .byte 7,4,0,1, 7,0,6,3, 0,1,3,2
    .byte 1,4,2,5, 4,7,5,6, 3,2,6,5
vis_origins:
    .byte 2,0, 0,2, 2,2, 4,2, 6,2, 2,4
.balign 4
vis_colors:
    .word 0xffffff, 0xff8000, 0x00cc44, 0xff2020, 0x2050ff, 0xffff00
vis_sources:
    .byte 1,4,2,0,3,5,6, 0,1,2,4,5,6,3, 0,2,5,3,1,4,6
vis_twists:
    .byte 1,2,0,2,1,0,0, 0,0,0,1,2,1,2, 0,0,0,0,0,0,0
.section .bss
vis_state: .space 14
vis_next: .space 14
.section .text
# move at a0, turns at a0, source at a1, twist at a2
.type vis_move, @function
vis_move:
    la t0, move_faces
    add t0, t0, a0
    lbu t1, 0(t0)
    slli t0, t1, 3
    sub t1, t0, t1
    la t0, turn_indices
    add t0, t0, a0
    lbu a0, 0(t0)
    addi a0, a0, 1
    la a1, vis_sources
    add a1, a1, t1
    la a2, vis_twists
    add a2, a2, t1
.vis_quarter:
    la a3, vis_state
    la a4, vis_next
    li t0, 0
.vis_cubie:
    add t1, a1, t0
    lbu t1, 0(t1)
    add t1, a3, t1
    lbu t2, 0(t1)
    lbu t3, 7(t1)
    add t1, a2, t0
    lbu t1, 0(t1)
    add t3, t3, t1
    li t1, 3
    bltu t3, t1, .vis_twisted
    addi t3, t3, -3
.vis_twisted:
    add t1, a4, t0
    sb t2, 0(t1)
    sb t3, 7(t1)
    addi t0, t0, 1
    li t1, 7
    bltu t0, t1, .vis_cubie
    li t0, 14
.vis_copy:
    lbu t1, 0(a4)
    sb t1, 0(a3)
    addi a4, a4, 1
    addi a3, a3, 1
    addi t0, t0, -1
    bnez t0, .vis_copy
    addi a0, a0, -1
    bnez a0, .vis_quarter
    ret

.size vis_move, .-vis_move

# width at a6, height at a7, LED base at a5
# 8x6 net: U / L F R B / D; address = base + 4*(y*width + x).
.type vis_draw, @function
vis_draw:
    li a6, LED_MATRIX_0_WIDTH
    li a7, LED_MATRIX_0_HEIGHT
    li a5, LED_MATRIX_0_BASE
    li t0, 0
.vis_clear_y:
    li t1, 0
.vis_clear_x:
    sw zero, 0(a5)
    addi a5, a5, 4
    addi t1, t1, 1
    bltu t1, a6, .vis_clear_x
    addi t0, t0, 1
    bltu t0, a7, .vis_clear_y
    li a0, 0
.vis_sticker:
    # sticker at a0, face at a1, slot at a2
    srli a1, a0, 2
    andi a2, a0, 3
    la t0, vis_face_corners
    add t0, t0, a0
    # position at t1, cubie at t2, orientation at t3
    lbu t1, 0(t0)
    li t2, 0
    li t3, 0
    beqz t1, .vis_anchor
    la t0, vis_state
    add t0, t0, t1
    lbu t2, -1(t0)
    addi t2, t2, 1
    lbu t3, 6(t0)
.vis_anchor:
    slli t0, t1, 1
    add t0, t0, t1
    la t4, vis_corner_faces
    add t0, t0, t4
    li t1, 0
.vis_slot:
    lbu t5, 0(t0)
    beq t5, a1, .vis_slot_found
    addi t0, t0, 1
    addi t1, t1, 1
    j .vis_slot
.vis_slot_found:
    # source slot = (slot - orientation) % 3
    sub t1, t1, t3
    bgez t1, .vis_slot_positive
    addi t1, t1, 3
.vis_slot_positive:
    slli t0, t2, 1
    add t0, t0, t2
    add t0, t0, t1
    add t0, t0, t4
    lbu t0, 0(t0)
    slli t0, t0, 2
    la t1, vis_colors
    add t0, t0, t1
    lw a3, 0(t0)
    slli t0, a1, 1
    la t1, vis_origins
    add t0, t0, t1
    # x at t1, y at t2
    lbu t1, 0(t0)
    lbu t2, 1(t0)
    andi t0, a2, 1
    add t1, t1, t0
    srli t0, a2, 1
    add t2, t2, t0
    li t0, 0
.vis_row_offset:
    beqz t2, .vis_address
    add t0, t0, a6
    addi t2, t2, -1
    j .vis_row_offset
.vis_address:
    add t0, t0, t1
    slli t0, t0, 2
    li a4, LED_MATRIX_0_BASE
    add a4, a4, t0
    sw a3, 0(a4)
    addi a0, a0, 1
    li t0, 24
    bltu a0, t0, .vis_sticker
    li t0, FRAME_DELAY
.vis_delay:
    addi t0, t0, -1
    bnez t0, .vis_delay
    ret
.size vis_draw, .-vis_draw
.endif

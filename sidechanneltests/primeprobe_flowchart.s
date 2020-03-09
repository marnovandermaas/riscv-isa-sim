rxentry:

00 auipc a0, 0x0
01 lw a1, 0(a0)
02 add t0, zero, zero
03 addi t1, zero, 1
04 slli t1, t1, 32


rxloop:

05 srli t1, t1, 1
06 beq t1, zero, rxend
07 csrrs t2, 0x415, zero
08 nop
09 nop
0A lw a1, 0(a0)


rxloopend:

0B csrrs t3, 0x415, zero
0C sub t4, t3, t2
0D beq t4, zero, rxbitunset
0E or t0, t0, t1
0F j rxloop


rxbitunset:

2E nop
2F j rxloop


txentry:

10 auipc a0, 0x0
11 lui t0, 0x55555
12 addi t0, t0, 0x555
13 addi t1, zero, 1
14 slli t1, t1, 32


txloop:

15 srli t1, t1, 1
16 beq t1, zero, txend
17 and t2, t0, t1
18 beq t2, zero, txbitunset
19 lw a1, 0(a0)
1A j txloopend


txbitunset:

39 nop
3A nop


txloopend:

1B nop
1C nop
1D nop
1E nop
1F j txloop

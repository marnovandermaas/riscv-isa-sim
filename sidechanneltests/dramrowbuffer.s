.section .text.init
rxentry:
    auipc a0, 0x0
    lw a1, 0(a0)              #Prime the cache by evicting the sender's line
    add t0, zero, zero        #set t0 to zero
    addi t1, zero, 1          #set our mask to 1
    slli t1, t1, 32           #prepare our mask with just 33rd bit set

rxloop:
    srli t1, t1, 1            #shifting our mask right
    beq t1, zero, rxend       #We've gone through the loop 32 times now
    csrrs t2, 0x415, zero     #Read the initial miss count
    nop                       #filler to match txloop
    nop                       #filler to match txloop
    lw a1, 0(a0)
    csrrs t3, 0x415, zero     #Read out final miss count
    sub t4, t3, t2            #Calculate difference
    beq t4, zero, rxbitunset  #t4 is zero if no eviction occured
    or t0, t0, t1             #setting the (31-n)th bit where n is the loop iteration count
    j rxloop

rxbitunset:
    nop #don't set bit in t0
    j rxloop


rxend:
    csrrw zero, 0x405, zero   #This will cause the simulator to exit
    j rxend                   #Loop when finished if there is no environment to return to.

.section .data.init
txentry:
    auipc a0, 0x0
    lui t0, 0x55555           #loading upper 20 bits of secret into t0
    addi t0, t0, 0x555        #loading lower 12 bits of secret into t0
    addi t1, zero, 1          #set our mask to 1
    slli t1, t1, 32           #prepare our mask with just the 32nd bit set

txloop:
    srli t1, t1, 1            #Shift our mask right by 1
    beq t1, zero, txend
    and t2, t0, t1            #Mask one bit of the secret
    beq t2, zero, txbitunset  #check whether bit is set or not
    lw a1, 0(a0)              #evict line if bit is set
    j txloopend

txbitunset:
    nop                       #don't evict line if bit is unset
    nop                       #filler for j txloopend

txloopend:
    nop                       #filler to match rxloop
    nop                       #filler to match rxloop
    nop                       #filler to match rxloop
    nop                       #filler to match rxloop
    j txloop


txend:
    csrrw zero, 0x405, zero   #This will cause the simulator to exit
    j txend                   #Loop when finished if there is no environment to return to.

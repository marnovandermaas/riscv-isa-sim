# RISC-V baremetal assembly

.section .reader.asm

reader: #Reader runs in normal world and is receiving the bits via prime and probe attack
#    lw collision_address    #Prime the cache by evicting the sender's line
    csrrw t0, 0x415, zero   #Read the initial miss count
#   lw collision_address
    csrrw t1, 0x415, zero   #Read out final miss count
    sub t2, t1, t0          #Calculate difference
    csrrw t0, 0x415, zero   #Read the initial miss count
#    lw collision_address
    csrrw t1, 0x415, zero   #Read out final miss count
    sub t3, t1, t0          #Calculate difference

rend:
    csrrw zero, 0x405, zero #This will cause the simulator to exit
    j rend                   #Loop when finished if there is no environment to return to.

.section .sender.asm
sender: #Sender runs in enclave and is transmitting bits
    auipc a0, 0x0
    nop
    nop
    nop
    nop
#    lw [0]a0
    nop
    nop

send:
    csrrw zero, 0x405, zero #This will cause the simulator to exit
    j send                   #Loop when finished if there is no environment to return to.

# RISC-V baremetal init.s
# This code is executed first.

.section .text.init
entry:
    la    t0, __mem         #Load base address into t0
    la    t1, __stack       #Load stack offset into t1
    csrrs a0, 0xF14, zero   #Read hartID into a0
    addiw a0, a0, 0x1       #Add one to a0, which contains the hartID in spike
    mul   t1, t1, a0        #Multiply the stack offset with a0 so that each thread has its own stack
    add   sp, t0, t1        #Add the memory base and the stack offset and set it as the stack pointer.
    call  main              #Call the main function

end:
    csrrw zero, 0x405, zero #This will cause the simulator to exit
    j end                   #Loop when finished if there is no environment to return to.

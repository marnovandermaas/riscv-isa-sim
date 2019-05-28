# RISC-V baremetal init.s
# This code is executed first.

.section .text.init
entry:
    auipc t0, 0x5           #t0 is pc plus 5000 which is the stack offset
    csrrs a0, 0xF14, zero   #Read hartID into a0
    add   sp, t0, zero      #Add the memory base and the stack offset and set it as the stack pointer.
    call  main              #Call the main function

end:
    csrrw zero, 0x405, zero #This will cause the simulator to exit
    j end                   #Loop when finished if there is no environment to return to.

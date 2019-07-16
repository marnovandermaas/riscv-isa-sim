# RISC-V baremetal init.s
# This code is executed first.

.section .text.init
entry:
    #TODO make the stack offset dependent on a variable in the linker script
    auipc sp, 0x20000       #sp is pc plus 0x2000 0000 which is the stack location
    csrrs a0, 0xF14, zero   #Read hartID into a0
    call  main              #Call the main function

end:
    csrrw zero, 0x405, zero #This will cause the simulator to exit
    j end                   #Loop when finished if there is no environment to return to.

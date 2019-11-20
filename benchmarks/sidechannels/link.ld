OUTPUT_ARCH( "riscv" )

__reader   = 0x80000000;
__sender   = 0x80004000;

SECTIONS
{
    . = __reader;
    .reader : {
        *(.reader.asm) /* add the instruction from primeprobe.s*/
    }

    . = __sender;
    .sender : {
        *(.sender.asm) /* add the instruction from primeprobe.s*/
    }
}

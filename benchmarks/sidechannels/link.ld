OUTPUT_ARCH( "riscv" )

/*
 * The stack starts at the top of memory, defined by __sp, and grows down.
 * Unfortunately this means we must hardcode the size of memory.
 * Here we choose 64K. Change this value to change the working memory size.
 */
/*__mem should be set to the beginning of the memory as specified by the device tree, for spike run: "spike --dump-dts a.out"*/
__mem   = 0x80000000;
__emem  = 0x80004000;

SECTIONS
{
  . = __mem;
  .text : {
      *(.text.init)
  }

  . = __emem;
  .data : {
      *(.data.init)
  }
}

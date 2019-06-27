OUTPUT_ARCH( "riscv" )

/*
 * The stack starts at the top of memory, defined by __sp, and grows down.
 * Unfortunately this means we must hardcode the size of memory.
 * Here we choose 64K. Change this value to change the working memory size.
 */
/*__mem should be set to the beginning of the memory as specified by the device tree, for spike run: "spike --dump-dts a.out"*/ 
__mem   = 0x80000000; 


SECTIONS
{
    . = __mem;
    .text : {
        *(.text.init) /* add the instruction from init.s*/
        *(.text) /* add the instructions compiled from hello.c*/
    }

   /* data segment */
  .data : { *(.data) }

  .sdata : {
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2)
    *(.srodata*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  }

  /* bss segment */
  __bss_start = .;
  .sbss : {
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon)
  }
  .bss : { *(.bss) }
  __bss_end = .;

  __malloc_start = .;
  . = . + 512;

  /* End of uninitalized data segement */
  _end = .;
}



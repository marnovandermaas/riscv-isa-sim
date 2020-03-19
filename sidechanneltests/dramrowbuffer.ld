OUTPUT_ARCH( "riscv" )

/*
 * The stack starts at the top of memory, defined by __sp, and grows down.
 * Unfortunately this means we must hardcode the size of memory.
 * Here we choose 64K. Change this value to change the working memory size.
 */
__mem   = 0x80000000;
__emem  = 0x80020000; /* This is row 1 in bank 0 assuming 2^13 Bytes in a row, 2^14 rows in a bank, 2^31 Bytes in memory and addresses in form row:bank:Byte*/

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

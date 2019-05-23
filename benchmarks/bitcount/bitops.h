/* +++Date last modified: 05-Jul-1997 */

/*
**  Macros and prototypes for bit operations
**
**  public domain for SNIPPETS by:
**    Scott Dudley
**    Auke Reitsma
**    Ratko Tomic
**    Aare Tali
**    J. Blauth
**    Bruce Wedding
**    Bob Stout
*/

#ifndef BITOPS__H
#define BITOPS__H

#include "stdint.h"
/*
**  BITCNT_1.C
*/

int  bit_count(uint32_t x);

/*
**  BITCNT_2.C
*/

int  bitcount(uint32_t i);

/*
**  BITCNT_3.C
*/

int  ntbl_bitcount(uint32_t x);
int  BW_btbl_bitcount(uint32_t x);
int  AR_btbl_bitcount(uint32_t x);

/*
**  BITCNT_4.C
*/

int  ntbl_bitcnt(uint32_t);
int  btbl_bitcnt(long x);

#endif /*  BITOPS__H */

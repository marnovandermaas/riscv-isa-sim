/* +++Date last modified: 05-Jul-1997 */

/*
 **  BITCNTS.C - Test program for bit counting functions
 **
 **  public domain by Bob Stout & Auke Reitsma
 */

#include "stdint.h"
#include "bitops.h"
#include "praesidio.h"
#include "stdio.h"

#define FUNCS  7
#define ITERATIONS 10000 //500000
#define BITCOUNT_ITER 1

static int bit_shifter(uint32_t x);

int main_hongyan()
{
    uint32_t i, j, n, seed;
    uint32_t iterations;
    static int (* pBitCntFunc[FUNCS])(uint32_t) = {
        bit_count,
        bitcount,
        ntbl_bitcnt,
        ntbl_bitcount,
        /*            btbl_bitcnt, DOESNT WORK*/
        BW_btbl_bitcount,
        AR_btbl_bitcount,
        bit_shifter
    };
    static const char *text[FUNCS] = {
        "Optimized 1 bit/loop counter",
        "Ratko's mystery algorithm",
        "Recursive bit count by nybbles",
        "Non-recursive bit count by nybbles",
        /*            "Recursive bit count by bytes",*/
        "Non-recursive bit count by bytes (BW)",
        "Non-recursive bit count by bytes (AR)",
        "Shift and count bits"
    };

    iterations=ITERATIONS;

    printf("Bit counter algorithm benchmark\n");

    for(int iter=0; iter<BITCOUNT_ITER; iter++) {
        for(i = 0; i < FUNCS; i++) {
            for(j = n = 0, seed = 0; j < iterations; j++, seed += 13)
                n += pBitCntFunc[i](seed);
            // output_string("Counting algorithm ");
            // output_char('0'+i);
            // output_string(" counts:\n    ");
            // output_char('0'+(n%8));
            // output_char('\n');
            printf("Counting algorithm %s counts:\n    %d.\n", text[i], n);
        }
    }
    return 0;
}

static int bit_shifter(uint32_t x)
{
    int i, n;

    for (i = n = 0; x && (i < (int)(sizeof(uint32_t) * CHAR_BIT)); ++i, x >>= 1)
        n += (int)(x & 1L);
    return n;
}

int main() {
  int id = getCurrentEnclaveID();

  if(id == ENCLAVE_DEFAULT_ID) {
    main_hongyan();
  } else {
    //Do nothing
  }
}

#ifndef _RISCV_ENCLAVE_H
#define _RISCV_ENCLAVE_H

#include "decode.h"
#include "../praesidio-software/lib/enclaveLibrary.h"

#define NUM_OF_ENCLAVE_PAGES 3

struct page_owner_t
{
  enclave_id_t owner = ENCLAVE_DEFAULT_ID;
  enclave_id_t reader = ENCLAVE_INVALID_ID;
};

#endif //_RISCV_ENCLAVE_H

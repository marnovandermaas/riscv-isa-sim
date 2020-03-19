#ifndef _RISCV_ENCLAVE_H
#define _RISCV_ENCLAVE_H

#include "decode.h"
typedef uint64_t enclave_id_t;
#define ENCLAVE_DEFAULT_ID    (0)
#define ENCLAVE_INVALID_ID    (0xFFFFFFFFFFFFFFFF)

struct page_owner_t
{
  enclave_id_t owner = ENCLAVE_DEFAULT_ID;
  enclave_id_t reader = ENCLAVE_INVALID_ID;
};

#endif //_RISCV_ENCLAVE_H

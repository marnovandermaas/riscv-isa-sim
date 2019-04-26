#ifndef _RISCV_ENCLAVE_H
#define _RISCV_ENCLAVE_H

#include "decode.h"

typedef uint64_t enclave_id_t;

#define ENCLAVE_MANAGEMENT_ID (0xFFFFFFFFFFFFFFFF)
#define ENCLAVE_DEFAULT_ID    (0)
#define ENCLAVE_INVALID_ID    (0xFFFFFFFFFFFFFFFE)

struct Message_t {
  enclave_id_t source;
  enclave_id_t destination;
  reg_t content; //Assuming that a reg_t can fit an Address_t
};

struct page_owner_t
{
  enclave_id_t owner = ENCLAVE_DEFAULT_ID;
  enclave_id_t reader = ENCLAVE_INVALID_ID;
};

#endif //_RISCV_ENCLAVE_H

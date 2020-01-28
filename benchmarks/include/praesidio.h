#ifndef PRAESIDIO_HEADER
#define PRAESIDIO_HEADER

#include "management.h"
#include "instructions.h"
#include "../riscv/encoding.h"

#define NUMBER_OF_ENCLAVE_PAGES (64)
#define NUMBER_OF_STACK_PAGES (4)
#define NUMBER_OF_COMMUNICATION_PAGES (1)

#define PAGE_TO_POINTER(NUM) (((NUM * (1 << 12)) | DRAM_BASE))

#endif //PRAESIDIO_HEADER

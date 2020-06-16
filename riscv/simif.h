// See LICENSE for license details.

#ifndef _RISCV_SIMIF_H
#define _RISCV_SIMIF_H

#include "enclave.h"
#include "decode.h"

// this is the interface to the simulator used by the processors and memory
class simif_t
{
public:
  // should return NULL for MMIO addresses
  virtual char* addr_to_mem(reg_t addr) = 0;
  // used for MMIO addresses
  virtual bool mmio_load(reg_t addr, size_t len, uint8_t* bytes) = 0;
  virtual bool mmio_store(reg_t addr, size_t len, const uint8_t* bytes) = 0;
  // Callback for processors to let the simulation know they were reset.
  virtual void proc_reset(unsigned id) = 0;
  // Praesidio specific calls
  virtual void request_halt(uint32_t id) = 0;
  virtual void output_stats(reg_t label=0) = 0;
  virtual void process_enclave_read_access(reg_t paddr, enclave_id_t writer_id, enclave_id_t reader_id) = 0;
};

#endif

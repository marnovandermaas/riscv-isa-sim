// See LICENSE for license details.

#ifndef _RISCV_CACHE_SIM_H
#define _RISCV_CACHE_SIM_H

#include "memtracer.h"
#include <cstring>
#include <string>
#include <map>
#include <cstdint>

typedef size_t slot_id_t;
typedef uint64_t enclave_id_t;

//Linear-feedback shift register
class lfsr_t
{
 public:
  lfsr_t() : reg(1) {}
  lfsr_t(const lfsr_t& lfsr) : reg(lfsr.reg) {}
  uint32_t next() { return reg = (reg>>1)^(-(reg&1) & 0xd0000001); }
 private:
  uint32_t reg;
};

//Base cache simulator class
class cache_sim_t
{
 public:
  //Caches have "sets" amount of sets. Each set consists of "ways" amount of
  //cache blocks. And each block has "linesz" amount of Bytes
  cache_sim_t(size_t sets, size_t ways, size_t linesz, const char* name);
  cache_sim_t(const cache_sim_t& rhs);
  virtual ~cache_sim_t();

  void access(uint64_t addr, size_t bytes, bool store);
  void print_stats();
  void set_miss_handler(cache_sim_t* mh) { miss_handler = mh; }

  static cache_sim_t* construct(const char* config, const char* name);

 protected:
  static const uint64_t VALID = 1ULL << 63;
  static const uint64_t DIRTY = 1ULL << 62;

  virtual uint64_t* check_tag(uint64_t addr);
  virtual uint64_t victimize(uint64_t addr);

  lfsr_t lfsr;
  cache_sim_t* miss_handler;

  size_t sets;
  size_t ways;
  size_t linesz;
  size_t idx_shift;

  uint64_t* tags;
  
  uint64_t read_accesses;
  uint64_t read_misses;
  uint64_t bytes_read;
  uint64_t write_accesses;
  uint64_t write_misses;
  uint64_t bytes_written;
  uint64_t writebacks;

  std::string name;

  void init();
};

class remapping_table_t
{
  public:
    remapping_table_t(size_t sets);
};

//Wrapper class for LLC that is partitioned per enclave.
class partitioned_cache_sim_t : public cache_sim_t
{
//Just use the static construct function from the cache_sim_t
//Partitioned cache has an owner identifier per set.
  public:
    partitioned_cache_sim_t(size_t sets, size_t ways, size_t linesz, const char* name, slot_id_t num_of_slots);
    slot_id_t add_enclave(enclave_id_t id);
    void access(uint64_t addr, size_t bytes, bool store, enclave_id_t id);
    //void print_stats();
  protected:
    uint64_t* check_tag(uint64_t addr, enclave_id_t id);
    uint64_t victimize(uint64_t addr);
    remapping_table_t** rmts;
    size_t max_enclaves;
};

//This is a fully associative cache, which only has one set of cache blocks.
class fa_cache_sim_t : public cache_sim_t
{
 public:
  fa_cache_sim_t(size_t ways, size_t linesz, const char* name);
  uint64_t* check_tag(uint64_t addr);
  uint64_t victimize(uint64_t addr);
 private:
  static bool cmp(uint64_t a, uint64_t b);
  std::map<uint64_t, uint64_t> tags;
};

//Memtracer wrapper around cache class, which is used to be able to hook it to the MMU.
class cache_memtracer_t : public memtracer_t
{
 public:
  cache_memtracer_t(const char* config, const char* name)
  {
    cache = cache_sim_t::construct(config, name);
  }
  ~cache_memtracer_t()
  {
    delete cache;
  }
  void set_miss_handler(cache_sim_t* mh)
  {
    cache->set_miss_handler(mh);
  }

  void print_stats() {
    cache->print_stats();
  }

 protected:
  cache_sim_t* cache;
};

//instruction cache subclass of memory tracer
class icache_sim_t : public cache_memtracer_t
{
 public:
  icache_sim_t(const char* config) : cache_memtracer_t(config, "I$") {}
  bool interested_in_range(uint64_t begin, uint64_t end, access_type type)
  {
    return type == FETCH;
  }
  void trace(uint64_t addr, size_t bytes, access_type type)
  {
    if (type == FETCH) cache->access(addr, bytes, false);
  }
};

//data cache subclass of memory tracer
class dcache_sim_t : public cache_memtracer_t
{
 public:
  dcache_sim_t(const char* config) : cache_memtracer_t(config, "D$") {}
  bool interested_in_range(uint64_t begin, uint64_t end, access_type type)
  {
    return type == LOAD || type == STORE;
  }
  void trace(uint64_t addr, size_t bytes, access_type type)
  {
    if (type == LOAD || type == STORE) cache->access(addr, bytes, type == STORE);
  }
};

#endif

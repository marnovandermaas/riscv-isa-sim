// See LICENSE for license details.

#ifndef _RISCV_CACHE_SIM_H
#define _RISCV_CACHE_SIM_H

#include "memtracer.h"
#include "enclave.h"
#include <cstring>
#include <string>
#include <map>
#include <cstdint>

typedef size_t slot_id_t;

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

  virtual void access(uint64_t addr, size_t bytes, bool store);
  void print_stats(bool csv_style=false);
  void set_miss_handler(cache_sim_t* mh);

  static cache_sim_t* construct(const char* config, const char* name);
  static void parse_config_string(const char* config, size_t *sets, size_t *ways, size_t *linesz); //Extracts sets ways and linesz from config string.

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

//Wrapper class for LLC that is partitioned per enclave.
class partitioned_cache_sim_t
{
//Just use the static construct function from the cache_sim_t
//Partitioned cache has an owner identifier per set.
  public:
    partitioned_cache_sim_t(size_t slots); //Set/Way mapping done in remapping table
    bool access(size_t slot, uint64_t addr, enclave_id_t id);
    size_t victimize(uint64_t addr, size_t slot, enclave_id_t id); //Returns a random new slot to replace.
  protected:
    uint64_t* check_tag(uint64_t addr, enclave_id_t id);
  private:
    uint64_t *addresses;
    enclave_id_t *identifiers;
    size_t cache_size;
};

class remapping_table_t : public cache_sim_t
{
  public:
    remapping_table_t(size_t sets, size_t ways, size_t linesz, const char* name, partitioned_cache_sim_t* l2, enclave_id_t id); //Assuming direct mapped for now (way = 1)
    void print_stats(bool csv_style=false);
    virtual void access(uint64_t addr, size_t bytes, bool store);
  private:
    partitioned_cache_sim_t *llc;
    enclave_id_t enclave_id;
    size_t *slots;
    uint64_t victimize(uint64_t addr, size_t index);
  protected:
    virtual uint64_t* check_tag(uint64_t addr, size_t *slot);
    virtual uint64_t victimize(uint64_t addr);
    uint64_t llc_read_misses;
    uint64_t llc_write_misses;
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
  cache_memtracer_t(size_t sets, size_t ways, size_t linesz, const char* name)
  {
    cache = new cache_sim_t(sets, ways, linesz, name);
  }
  ~cache_memtracer_t()
  {
    delete cache;
  }
  void set_miss_handler(cache_sim_t* mh)
  {
    cache->set_miss_handler(mh);
  }
  void set_miss_handler(cache_memtracer_t* mh)
  {
    cache->set_miss_handler(mh->cache);
  }
  void print_stats(bool csv_style=false) {
    cache->print_stats(csv_style);
  }

 protected:
  cache_sim_t* cache;
};

//l2 cache subclass of memory tracer
class l2cache_sim_t : public cache_memtracer_t
{
 public:
  l2cache_sim_t(const char* config, const char* name) : cache_memtracer_t(config, name) {}
  l2cache_sim_t(size_t sets, size_t ways, size_t linesz, const char* name) : cache_memtracer_t(sets, ways, linesz, name) {}
  l2cache_sim_t(size_t sets, size_t ways, size_t linesz, const char* name, partitioned_cache_sim_t* l2, enclave_id_t id) : cache_memtracer_t(sets, ways, linesz, name) {
    delete cache;
    cache = new remapping_table_t(sets, ways, linesz, name, l2, id);
  }
  bool interested_in_range(uint64_t begin, uint64_t end, access_type type)
  {
    return true;
  }
  void trace(uint64_t addr, size_t bytes, access_type type)
  {
    cache->access(addr, bytes, type == STORE);
  }
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

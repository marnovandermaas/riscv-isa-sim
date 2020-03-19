// See LICENSE for license details.

#ifndef _RISCV_CACHE_SIM_H
#define _RISCV_CACHE_SIM_H

#include "memtracer.h"
#include "enclave.h"
#include <cstring>
#include <string>
#include <map>
#include <cstdint>
#include "debug.h"

typedef size_t slot_id_t;

enum cache_result {
  CACHE_HIT,
  CACHE_MISS,
  CACHE_MISS_HIT,
  CACHE_MISS_MISS,
};

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

  virtual cache_result access(uint64_t addr, size_t bytes, bool store);
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
    virtual cache_result access(uint64_t addr, size_t bytes, bool store);
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

class dram_bank_t : public memtracer_t
{
public:
  dram_bank_t(uint64_t address_bits, uint64_t row_bits, uint64_t byte_bits, size_t bank_number)
  {
    uint64_t bank_bits = address_bits - row_bits - byte_bits;
    _row_offset = address_bits - row_bits;
    _row_mask = ((1 << row_bits) - 1) << (_row_offset);
    _bank_mask = ((1 << (bank_bits)) << (byte_bits));
    _bank_offset = byte_bits;
    _bank_number = bank_number;
    _open_row_number = 0;
  }
  ~dram_bank_t() {}

  bool interested_in_range(uint64_t begin, uint64_t end, access_type type)
  {
    uint64_t begin_bank_number = (begin & _bank_mask) >> _bank_offset;
    uint64_t end_bank_number = (end & _bank_mask) >> _bank_offset;
    return ((type != FETCH) && ((begin_bank_number == _bank_number) || (end_bank_number == _bank_number)));
  }
  trace_result trace(uint64_t addr, size_t bytes, access_type type)
  {
    uint64_t row_number = (addr & _row_mask) >> _row_offset;
    if(row_number == _open_row_number)
    {
      return LLC_HIT;
    }
    else
    {
      _open_row_number = row_number; //update open row number
      return LLC_MISS;
    }
  }
private:
  uint64_t _row_mask;
  uint64_t _row_offset;
  uint64_t _bank_mask;
  uint64_t _bank_offset;
  uint64_t _bank_number;
  uint64_t _open_row_number;
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
    return (type != FETCH);
  }
  trace_result trace(uint64_t addr, size_t bytes, access_type type)
  {
    switch(cache->access(addr, bytes, type == STORE)) {
      case CACHE_MISS:
#ifdef PRAESIDIO_DEBUG
        fprintf(stderr, "cachesim.h: l2 cache miss for address 0x%016lx, size %lu\n", addr, bytes);
#endif //PRAESIDIO_DEBUG
        return LLC_MISS;
      case CACHE_HIT:
        return LLC_HIT;
      default:
        fprintf(stderr, "cachesim.h: WARNING this case should not happen in l2cache_sim_t unless there is a third level cache\n");
        return NO_LLC_INTERACTION;
    }
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
  trace_result trace(uint64_t addr, size_t bytes, access_type type)
  {
    if (type == FETCH)
    {
      switch(cache->access(addr, bytes, false)) {
        case CACHE_MISS_MISS:
          return LLC_MISS;
        case CACHE_MISS_HIT:
          return LLC_HIT;
        default:
          return NO_LLC_INTERACTION;
      }
    }
    return NO_LLC_INTERACTION;
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
  trace_result trace(uint64_t addr, size_t bytes, access_type type)
  {
    if (type == LOAD || type == STORE)
    {
      switch(cache->access(addr, bytes, type == STORE)) {
        case CACHE_MISS_MISS:
          return LLC_MISS;
        case CACHE_MISS_HIT:
          return LLC_HIT;
        default:
          return NO_LLC_INTERACTION;
      }
    }
    return NO_LLC_INTERACTION;
  }
};

#endif

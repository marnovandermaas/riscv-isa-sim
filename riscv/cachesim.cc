// See LICENSE for license details.

#include "cachesim.h"
#include "common.h"
#include <cstdlib>
#include <iostream>
#include <iomanip>

cache_sim_t::cache_sim_t(size_t _sets, size_t _ways, size_t _linesz, const char* _name)
 : sets(_sets), ways(_ways), linesz(_linesz), name(_name)
{
  init();
}

static void help()
{
  std::cerr << "Cache configurations must be of the form" << std::endl;
  std::cerr << "  sets:ways:blocksize" << std::endl;
  std::cerr << "where sets, ways, and blocksize are positive integers, with" << std::endl;
  std::cerr << "sets and blocksize both powers of two and blocksize at least 8." << std::endl;
  exit(1);
}

cache_sim_t* cache_sim_t::construct(const char* config, const char* name)
{
  size_t sets, ways, linesz;
  parse_config_string(config, &sets, &ways, &linesz);

  if (ways > 4 /* empirical */ && sets == 1)
    return new fa_cache_sim_t(ways, linesz, name);
  return new cache_sim_t(sets, ways, linesz, name);
}

void cache_sim_t::parse_config_string(const char* config, size_t *sets, size_t *ways, size_t *linesz)
{
  const char* wp = strchr(config, ':');
  if (!wp++) help();
  const char* bp = strchr(wp, ':');
  if (!bp++) help();

  *sets = atoi(std::string(config, wp).c_str());
  *ways = atoi(std::string(wp, bp).c_str());
  *linesz = atoi(bp);
}

void cache_sim_t::init()
{
  if(sets == 0 || (sets & (sets-1)))
    help();
  if(linesz < 8 || (linesz & (linesz-1)))
    help();

  idx_shift = 0;
  for (size_t x = linesz; x>1; x >>= 1)
    idx_shift++;

  tags = new uint64_t[sets*ways]();
  read_accesses = 0;
  read_misses = 0;
  bytes_read = 0;
  write_accesses = 0;
  write_misses = 0;
  bytes_written = 0;
  writebacks = 0;

  miss_handler = NULL;
}

cache_sim_t::cache_sim_t(const cache_sim_t& rhs)
 : sets(rhs.sets), ways(rhs.ways), linesz(rhs.linesz),
   idx_shift(rhs.idx_shift), name(rhs.name)
{
  tags = new uint64_t[sets*ways];
  memcpy(tags, rhs.tags, sets*ways*sizeof(uint64_t));
}

cache_sim_t::~cache_sim_t()
{
  print_stats();
  delete [] tags;
}

void cache_sim_t::print_stats()
{
  if(read_accesses + write_accesses == 0) {
    std::cout << name << " ";
    std::cout << "No stats recorded" << std::endl;
    return;
  }

  float mr = 100.0f*(read_misses+write_misses)/(read_accesses+write_accesses);

  std::cout << std::setprecision(3) << std::fixed;
  std::cout << name << " ";
  std::cout << "Bytes Read:            " << bytes_read << std::endl;
  std::cout << name << " ";
  std::cout << "Bytes Written:         " << bytes_written << std::endl;
  std::cout << name << " ";
  std::cout << "Read Accesses:         " << read_accesses << std::endl;
  std::cout << name << " ";
  std::cout << "Write Accesses:        " << write_accesses << std::endl;
  std::cout << name << " ";
  std::cout << "Read Misses:           " << read_misses << std::endl;
  std::cout << name << " ";
  std::cout << "Write Misses:          " << write_misses << std::endl;
  std::cout << name << " ";
  std::cout << "Writebacks:            " << writebacks << std::endl;
  std::cout << name << " ";
  std::cout << "Miss Rate:             " << mr << '%' << std::endl;
}

uint64_t* cache_sim_t::check_tag(uint64_t addr)
{
  size_t idx = (addr >> idx_shift) & (sets-1);
  size_t tag = (addr >> idx_shift) | VALID;

  for (size_t i = 0; i < ways; i++)
    if (tag == (tags[idx*ways + i] & ~DIRTY))
      return &tags[idx*ways + i];

  return NULL;
}

uint64_t cache_sim_t::victimize(uint64_t addr)
{
  size_t idx = (addr >> idx_shift) & (sets-1);
  size_t way = lfsr.next() % ways;
  uint64_t victim = tags[idx*ways + way];
  tags[idx*ways + way] = (addr >> idx_shift) | VALID;
  return victim;
}

void cache_sim_t::access(uint64_t addr, size_t bytes, bool store)
{
  //fprintf(stderr, "cache_sim_t accessing cache %s with addres %lu, bytes %lu and store %d.\n", name.c_str(), addr, bytes, store);
  store ? write_accesses++ : read_accesses++;
  (store ? bytes_written : bytes_read) += bytes;

  uint64_t* hit_way = check_tag(addr);
  if (likely(hit_way != NULL))
  {
    if (store)
      *hit_way |= DIRTY;
    return;
  }

  store ? write_misses++ : read_misses++;

  uint64_t victim = victimize(addr);

  if ((victim & (VALID | DIRTY)) == (VALID | DIRTY))
  {
    uint64_t dirty_addr = (victim & ~(VALID | DIRTY)) << idx_shift;
    if (miss_handler)
      miss_handler->access(dirty_addr, linesz, true);
    writebacks++;
  }

  if (miss_handler)
    miss_handler->access(addr & ~(linesz-1), linesz, false);

  if (store)
    *check_tag(addr) |= DIRTY;
}

void cache_sim_t::set_miss_handler(cache_sim_t* mh)
{
  if (miss_handler) {
    fprintf(stderr, "cache_sim_t ERROR: Attempting to set miss handler again.\n");
    exit(-1);
  }
  miss_handler = mh;
}

remapping_table_t::remapping_table_t(size_t _sets, size_t _ways, size_t _linesz, const char* _name, partitioned_cache_sim_t* _l2, enclave_id_t _id) :
  cache_sim_t(_sets, _ways, _linesz, _name), llc(_l2), enclave_id(_id)
{
  //fprintf(stderr, "remapping_table_t: constructing RMT %lu.\n", _id);
  llc_read_misses = 0;
  llc_write_misses = 0;
  slots = new size_t[sets*ways]();
}

void remapping_table_t::access(uint64_t addr, size_t bytes, bool store)
{
    //fprintf(stderr, "remapping_table_t: Accessing remapping table with address %lu, bytes %lu and store %d.\n", addr, bytes, store);
    store ? write_accesses++ : read_accesses++;
    (store ? bytes_written : bytes_read) += bytes;
    size_t index;
    uint64_t* hit_way = check_tag(addr, &index);
    bool soft_miss = false;
    if (hit_way != NULL)
    {
      //This means we have had a hit in the RMT.
      if(llc == NULL) {
        fprintf(stderr, "remappint_table_t: ERROR LLC is null, aborting.\n");
        exit(-1);
      }
      bool hit = llc->access(slots[index], addr, enclave_id);
      if(!hit) { //This means a hit in RMT, but miss in LLC.
        soft_miss = true;
      } else {
        if (store) {
          *hit_way |= DIRTY;
        }
        return;
      }
    }

    uint64_t victim;
    if(soft_miss) {
      store ? llc_write_misses++ : llc_read_misses++; //This keeps track of the LLC specific misses.
      victim = victimize(addr, index);
    } else {
      //This means there was a miss in the RMT or a missin the LLC
      store ? write_misses++ : read_misses++;

      victim = victimize(addr);
    }

    if ((victim & (VALID | DIRTY)) == (VALID | DIRTY))
    {
      uint64_t dirty_addr = (victim & ~(VALID | DIRTY)) << idx_shift;
      if (miss_handler)
        miss_handler->access(dirty_addr, linesz, true);
      writebacks++;
    }

    if (miss_handler)
      miss_handler->access(addr & ~(linesz-1), linesz, false);

    if (store)
      *check_tag(addr, &index) |= DIRTY;
}

uint64_t* remapping_table_t::check_tag(uint64_t addr, size_t *index)
{
    size_t idx = (addr >> idx_shift) & (sets-1);
    size_t tag = (addr >> idx_shift) | VALID;

    //fprintf(stderr, "remapping_table_t: checking tag\n");
    for (size_t i = 0; i < ways; i++) {
      if (tag == (tags[idx*ways + i] & ~DIRTY)) {
        *index = idx*ways + i;
        return &tags[idx*ways + i];
      }
    }

    return NULL;
}

uint64_t remapping_table_t::victimize(uint64_t addr)
{
    //fprintf(stderr, "remapping_table_t: victimize\n");
    size_t idx = (addr >> idx_shift) & (sets-1);
    size_t way = lfsr.next() % ways;
    return victimize(addr, idx*ways + way);
}

uint64_t remapping_table_t::victimize(uint64_t addr, size_t index)
{
    uint64_t victim = tags[index];
    tags[index] = (addr >> idx_shift) | VALID;
    slots[index] = llc->victimize(addr, slots[index], enclave_id);
    return victim;
}

void remapping_table_t::print_stats()
{
  cache_sim_t::print_stats();
  if(read_accesses + write_accesses == 0) {
    std::cout << name << " ";
    std::cout << "No stats recorded" << std::endl;
    return;
  }

  std::cout << name << " ";
  std::cout << "Read Misses in LLC:          " << llc_read_misses << std::endl;
  std::cout << name << " ";
  std::cout << "Write Misses in LLC:            " << llc_write_misses << std::endl;
}

partitioned_cache_sim_t::partitioned_cache_sim_t(size_t slots)
{
  cache_size = slots;
  addresses = new uint64_t[slots]();
  identifiers = new enclave_id_t[slots];
  for (size_t i = 0; i < slots; i++) {
    identifiers[i] = ENCLAVE_INVALID_ID;
  }
}

bool partitioned_cache_sim_t::access(size_t slot, uint64_t addr, enclave_id_t id) //returns whether it hit.
{
  if(identifiers[slot] == ENCLAVE_INVALID_ID) {
    return false;
  }
  if(addr == addresses[slot]) {
    if(id == identifiers[slot]) {
      return true;
    }
  }
  return false;
}

size_t partitioned_cache_sim_t::victimize(uint64_t addr, size_t slot, enclave_id_t id) //Returns new random slot to replace.
{
  //fprintf(stderr, "partitioned_cache_sim_t: victimizing address %lu, slot %lu, for id %lu. identifiers %d, addresses %d, cache_size %lu\n", addr, slot, id, identifiers, addresses, cache_size);
  identifiers[slot] = ENCLAVE_INVALID_ID;
  //fprintf(stderr, "1\n");
  size_t random_slot = rand() % cache_size;
  //fprintf(stderr, "partitioned_cache_sim_t, generated random slot: %lu\n", random_slot);
  addresses[random_slot] = addr;
  //fprintf(stderr, "1\n");
  identifiers[random_slot] = id;
  //fprintf(stderr, "partitioned_cache_sim_t: ending victimize.\n");
  return random_slot;
}

fa_cache_sim_t::fa_cache_sim_t(size_t ways, size_t linesz, const char* name)
  : cache_sim_t(1, ways, linesz, name)
{
}

uint64_t* fa_cache_sim_t::check_tag(uint64_t addr)
{
  auto it = tags.find(addr >> idx_shift);
  return it == tags.end() ? NULL : &it->second;
}

uint64_t fa_cache_sim_t::victimize(uint64_t addr)
{
  uint64_t old_tag = 0;
  if (tags.size() == ways)
  {
    auto it = tags.begin();
    std::advance(it, lfsr.next() % ways);
    old_tag = it->second;
    tags.erase(it);
  }
  tags[addr >> idx_shift] = (addr >> idx_shift) | VALID;
  return old_tag;
}

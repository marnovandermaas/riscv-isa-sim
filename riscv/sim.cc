// See LICENSE for license details.

#include "sim.h"
#include "mmu.h"
#include "dts.h"
#include "remote_bitbang.h"
#include "encoding.h"
#include <map>
#include <iostream>
#include <sstream>
#include <climits>
#include <cstdlib>
#include <cassert>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

volatile bool ctrlc_pressed = false;
static void handle_signal(int sig)
{
  if (ctrlc_pressed)
    exit(-1);
  ctrlc_pressed = true;
  signal(sig, &handle_signal);
}

void sim_t::request_halt(uint32_t id) {
  static bool procRequests[64] = {false};
  if(id >= 64) exit(-1);
  procRequests[id] = true;
  for(unsigned int i = 0; i < procs.size(); i++) {
    if(!procRequests[i]) return;
  }
  for(size_t i = 0; i < nenclaves + 1; i++) {
    if(ics[i] || dcs[i]) fprintf(stderr, "\nCache stats for enclave %lu:\n", i);
    if(ics[i]) ics[i]->print_stats();
    if(dcs[i]) dcs[i]->print_stats();
    if(rmts[i]) rmts[i]->print_stats();
  }
  if(l2 != NULL) {
    fprintf(stderr, "\nShared Cache:\n");
    l2->print_stats();
  }
  exit(0);
}

sim_t::sim_t(const char* isa, size_t nprocs, size_t nenclaves, bool halted, reg_t start_pc,
             std::vector<std::pair<reg_t, mem_t*>> mems,
             const std::vector<std::string>& args,
             std::vector<int> const hartids, unsigned progsize,
             unsigned max_bus_master_bits, bool require_authentication, icache_sim_t **ics, dcache_sim_t **dcs, cache_sim_t *l2, remapping_table_t **rmts, reg_t num_of_pages)
  : htif_t(args), mems(mems), procs(std::max(nprocs, size_t(1))), nenclaves(nenclaves),
    start_pc(start_pc), current_step(0), current_proc(0), debug(false),
    histogram_enabled(false), dtb_enabled(true), remote_bitbang(NULL),
    debug_module(this, progsize, max_bus_master_bits, require_authentication), ics(ics), dcs(dcs), l2(l2), rmts(rmts)
{
  signal(SIGINT, &handle_signal);

  page_owners = new enclave_id_t[num_of_pages];
  for(reg_t i = 0; i < num_of_pages; i++) {
    page_owners[i] = ENCLAVE_DEFAULT_ID;
  }

  for (auto& x : mems)
    bus.add_device(x.first, x.second);

  debug_module.add_device(&bus);

  debug_mmu = new mmu_t(this, NULL, page_owners, num_of_pages);

  if (hartids.size() == 0) {
    for (size_t i = 0; i < procs.size() - nenclaves; i++) {
      procs[i] = new processor_t(isa, this, i, ENCLAVE_DEFAULT_ID, page_owners, num_of_pages, halted);
    }
    enclave_id_t current_id = 1;
    for (size_t i = procs.size() - nenclaves; i < procs.size(); i++) {
      procs[i] = new processor_t(isa, this, i, current_id++, page_owners, num_of_pages, halted);
    }
  }
  else {
    if (hartids.size() != procs.size()) {
      std::cerr << "Number of specified hartids doesn't match number of processors or you specified both hardids and enclaves" << strerror(errno) << std::endl;
      exit(1);
    }
    for (size_t i = 0; i < procs.size(); i++) {
      procs[i] = new processor_t(isa, this, ENCLAVE_DEFAULT_ID, hartids[i], page_owners, num_of_pages, halted);
    }
  }

  clint.reset(new clint_t(procs));
  bus.add_device(CLINT_BASE, clint.get());
}

sim_t::~sim_t()
{
  for (size_t i = 0; i < procs.size(); i++)
    delete procs[i];
  delete debug_mmu;
}

void sim_t::make_enclave_pages() {
  addr_t base_addr = DRAM_BASE;
  size_t len = 8;
  char data[8];
  for(addr_t addr = base_addr; addr < base_addr + PGSIZE; addr += len) {
    read_chunk(addr, len, data);
    //printf("Address %10x with data 0x%02x %02x %02x %02x %02x %02x %02x %02x \n", addr, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    for (size_t i = 1; i <= nenclaves; i++) {
      write_chunk(addr+i*NUM_OF_ENCLAVE_PAGES*PGSIZE, len, data);
    }
  }
  for (size_t i = 1; i <= nenclaves; i++) {
    for (size_t j = 0; j < NUM_OF_ENCLAVE_PAGES; j++) {
      size_t code_page = NUM_OF_ENCLAVE_PAGES*i + j;
      size_t stack_page = STACK_PAGE_OFFSET*(procs.size() + i - 1) + NUM_OF_ENCLAVE_PAGES*(i+1) + j; 
      page_owners[code_page] = i; //code/data pages
      page_owners[stack_page] = i; //stack pages
      fprintf(stderr, "Setting page %d and %d to enclave %d.\n", code_page, stack_page, i);
    }
  }
}

void sim_thread_main(void* arg)
{
  ((sim_t*)arg)->main();
}

void sim_t::main()
{
  if (!debug && log)
    set_procs_debug(true);

  while (!done())
  {
    if (debug || ctrlc_pressed)
      interactive();
    else
      step(INTERLEAVE);
    if (remote_bitbang) {
      remote_bitbang->tick();
    }
  }
}

int sim_t::run()
{
  host = context_t::current();
  target.init(sim_thread_main, this);
  fprintf(stderr, "sim.cc: running htif.\n");
  return htif_t::run();
}

void sim_t::step(size_t n)
{
  for (size_t i = 0, steps = 0; i < n; i += steps)
  {
    steps = std::min(n - i, INTERLEAVE - current_step);
    if (current_proc < procs.size()) {
      procs[current_proc]->step(steps);
    }

    current_step += steps;
    if (current_step == INTERLEAVE)
    {
      current_step = 0;
      procs[current_proc]->get_mmu()->yield_load_reservation();

      if (++current_proc == procs.size()) {
        current_proc = 0;
        clint->increment(INTERLEAVE / INSNS_PER_RTC_TICK);
      }

      host->switch_to();
    }
  }
}

void sim_t::set_debug(bool value)
{
  debug = value;
}

void sim_t::set_log(bool value)
{
  log = value;
}

void sim_t::set_histogram(bool value)
{
  histogram_enabled = value;
  for (size_t i = 0; i < procs.size(); i++) {
    procs[i]->set_histogram(histogram_enabled);
  }
}

void sim_t::set_procs_debug(bool value)
{
  for (size_t i = 0; i < procs.size(); i++) {
    procs[i]->set_debug(value);
  }
}

bool sim_t::mmio_load(reg_t addr, size_t len, uint8_t* bytes)
{
  if (addr + len < addr)
    return false;
  return bus.load(addr, len, bytes);
}

bool sim_t::mmio_store(reg_t addr, size_t len, const uint8_t* bytes)
{
  if (addr + len < addr)
    return false;
  return bus.store(addr, len, bytes);
}

void sim_t::make_dtb()
{
  const int reset_vec_size = 8;

  start_pc = start_pc == reg_t(-1) ? get_entry_point() : start_pc;

  uint32_t reset_vec[reset_vec_size] = {
    0x297,                                      // auipc  t0,0x0
    0x28593 + (reset_vec_size * 4 << 20),       // addi   a1, t0, &dtb
    0xf1402573,                                 // csrr   a0, mhartid
    get_core(0)->get_xlen() == 32 ?
      0x0182a283u :                             // lw     t0,24(t0)
      0x0182b283u,                              // ld     t0,24(t0)
    0x28067,                                    // jr     t0
    0,
    (uint32_t) (start_pc & 0xffffffff),
    (uint32_t) (start_pc >> 32)
  };

  std::vector<char> rom((char*)reset_vec, (char*)reset_vec + sizeof(reset_vec));

  dts = make_dts(INSNS_PER_RTC_TICK, CPU_HZ, procs, mems);
  std::string dtb = dts_compile(dts);

  rom.insert(rom.end(), dtb.begin(), dtb.end());
  const int align = 0x1000;
  rom.resize((rom.size() + align - 1) / align * align);

  boot_rom.reset(new rom_device_t(rom));
  bus.add_device(DEFAULT_RSTVEC, boot_rom.get());
}

char* sim_t::addr_to_mem(reg_t addr) {
  auto desc = bus.find_device(addr);
  if (auto mem = dynamic_cast<mem_t*>(desc.second))
    if (addr - desc.first < mem->size())
      return mem->contents() + (addr - desc.first);
  return NULL;
}

// htif

void sim_t::reset()
{
  fprintf(stderr, "sim.cc: resetting.\n");
  make_enclave_pages();
  if (dtb_enabled)
    make_dtb();
}

void sim_t::idle()
{
  target.switch_to();
}

void sim_t::read_chunk(addr_t taddr, size_t len, void* dst)
{
  assert(len == 8);
  auto data = debug_mmu->load_uint64(taddr);
  memcpy(dst, &data, sizeof data);
}

void sim_t::write_chunk(addr_t taddr, size_t len, const void* src)
{
  assert(len == 8);
  uint64_t data;
  memcpy(&data, src, sizeof data);
  debug_mmu->store_uint64(taddr, data);
}

void sim_t::proc_reset(unsigned id)
{
  fprintf(stderr, "sim.cc: processor reset.\n");
  debug_module.proc_reset(id);
}

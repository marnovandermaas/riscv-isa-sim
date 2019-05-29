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
#include "debug.h"

volatile bool ctrlc_pressed = false;
static void handle_signal(int sig)
{
  if (ctrlc_pressed)
    exit(-1);
  ctrlc_pressed = true;
  signal(sig, &handle_signal);
}

void sim_t::request_halt(uint32_t id)
{
  static bool procRequests[64] = {false};
  if(id >= 64) exit(-1);
  procRequests[id] = true;

  for(unsigned int i = 0; i < procs.size(); i++)
  {
    if(!procRequests[i]) {
      if(i != 1) {//TODO remove this and just exit from the management code.
        return;
      }
    }
  }
  fprintf(stdout, "\n>>>>>INSTRUCTION_COUNT<<<<<\n%lu\n", procs[0]->get_csr(CSR_MINSTRET));
  bool csv_style = true;
#ifdef PRAESIDIO_DEBUG
  csv_style = false;
#endif
  while(true) {
    if(csv_style) fprintf(stdout, "\n>>>>>CACHE_OUTPUT<<<<<\n");
    for(size_t i = 0; i < nenclaves + 1; i++)
    {
      if((ics[i] || dcs[i]) && !csv_style) fprintf(stderr, "\nCache stats for enclave %lu:\n", i);
      if(ics[i]) {
        ics[i]->print_stats(csv_style);
        std::cout << std::endl;
      }
      if(dcs[i]) {
        dcs[i]->print_stats(csv_style);
        std::cout << std::endl;
      }
      if(rmts[i]) {
        rmts[i]->print_stats(csv_style);
        std::cout << std::endl;
      }
      if(static_llc[i]) {
        static_llc[i]->print_stats(csv_style);
        std::cout << std::endl;
      }
    }
    if(l2 != NULL)
    {
      if(!csv_style) fprintf(stderr, "\nShared Cache:\n");
      l2->print_stats(csv_style);
      std::cout << std::endl;
    }
    if(csv_style) break;
    csv_style = true;
  }
  for(unsigned int i = 0; i < procs.size(); i++)
  {
    procs[i]->output_histogram();
  }
  exit(0);
}

sim_t::sim_t(const char* isa, size_t nprocs, size_t nenclaves, bool halted, reg_t start_pc,
             std::vector<std::pair<reg_t, mem_t*>> mems,
             const std::vector<std::string>& args,
             std::vector<int> const hartids, unsigned progsize,
             unsigned max_bus_master_bits, bool require_authentication, icache_sim_t **ics, dcache_sim_t **dcs, cache_sim_t *l2, remapping_table_t **rmts, cache_sim_t **static_llc, struct Message_t *mailboxes, size_t num_of_mailboxes, reg_t num_of_pages)
  : htif_t(args), mems(mems), procs(std::max(nprocs, size_t(1))), nenclaves(nenclaves),
    start_pc(start_pc), current_step(0), current_proc(0), debug(false),
    histogram_enabled(false), dtb_enabled(true), remote_bitbang(NULL),
    debug_module(this, progsize, max_bus_master_bits, require_authentication), ics(ics), dcs(dcs), l2(l2), rmts(rmts), static_llc(static_llc)
{
  signal(SIGINT, &handle_signal);
#ifdef PRAESIDIO_DEBUG
  fprintf(stderr, "sim.cc: Constructing simulator with %lu processors and %lu enclaves.\n", nprocs, nenclaves);
#endif

  page_owners = new page_owner_t[num_of_pages];
  for(reg_t i = 0; i < num_of_pages; i++)
  {
    page_owners[i].owner = ENCLAVE_DEFAULT_ID;
    page_owners[i].reader = ENCLAVE_INVALID_ID;
  }

  for (auto& x : mems)
    bus.add_device(x.first, x.second);

  debug_module.add_device(&bus);

  debug_mmu = new mmu_t(this, NULL, page_owners, num_of_pages);

  if (hartids.size() == 0)
  { //TODO add the mailbox slots to the processor_t constructor.
    for (size_t i = 0; i < procs.size() - nenclaves; i++)
    {
      procs[i] = new processor_t(isa, this, i, ENCLAVE_DEFAULT_ID, page_owners, num_of_pages, &mailboxes[0], mailboxes, num_of_mailboxes, halted);
    }
    enclave_id_t current_id = 1;
    for (size_t i = procs.size() - nenclaves; i < procs.size(); i++)
    {
      procs[i] = new processor_t(isa, this, i, current_id, page_owners, num_of_pages, &mailboxes[current_id], mailboxes, num_of_mailboxes, halted);
      current_id += 1;
    }
  }
  else
  {
    if (hartids.size() != procs.size())
    {
      std::cerr << "Number of specified hartids doesn't match number of processors or you specified both hardids and enclaves" << strerror(errno) << std::endl;
      exit(1);
    }
    //for (size_t i = 0; i < procs.size(); i++)
    //{
    //  procs[i] = new processor_t(isa, this, ENCLAVE_DEFAULT_ID, hartids[i], page_owners, num_of_pages, &mailboxes[0], num_of_mailboxes, halted);
    //}
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

//This is moved to the normal world
// void sim_t::make_enclave_pages() {
//   addr_t base_addr = DRAM_BASE;
//   size_t len = 8;
//   char data[8];
//   for(addr_t addr = base_addr; addr < base_addr + PGSIZE; addr += len) {
//     read_chunk(addr, len, data);
//     for (size_t i = 1; i <= nenclaves; i++) {
//       write_chunk(addr+i*NUM_OF_ENCLAVE_PAGES*PGSIZE, len, data);
//     }
//   }
//   for (size_t i = 1; i <= nenclaves; i++) {
//     for (size_t j = 0; j < NUM_OF_ENCLAVE_PAGES; j++) {
//       size_t code_page = NUM_OF_ENCLAVE_PAGES*i + j;
//       size_t stack_page = STACK_PAGE_OFFSET*(procs.size() + i - 1) + NUM_OF_ENCLAVE_PAGES*(i+1) + j;
//       page_owners[code_page].owner = i; //code/data pages
//       page_owners[stack_page].owner = i; //stack pages
//       fprintf(stderr, "sim.cc: Setting page 0x%02lx and 0x%04lx to enclave %lu.\n", code_page, stack_page, i);
//     }
//   }
// }

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
#ifdef PRAESIDIO_DEBUG
  fprintf(stderr, "sim.cc: running htif.\n");
#endif
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

  if(start_pc == reg_t(-1)) {
    start_pc = get_entry_point(); //This is the get_entry_point function of htif_t found in riscv fesvr
#ifdef MANAGEMENT_ENCLAVE_INSTRUCTIONS
    //TODO add the entry point to the management enclave code instead of hardcoding it.
    if(nenclaves > 0) {
      start_pc = MANAGEMENT_ENCLAVE_BASE;
    }
#endif //MANAGEMENT_ENCLAVE_INSTRUCTIONS
  }
#ifdef PRAESIDIO_DEBUG
  fprintf(stderr, "sim.cc: Adding boot rom with start_pc %016lx\n", start_pc);
#endif

  uint32_t reset_vec[reset_vec_size + nenclaves + 1] = {
    0x297,                                      // auipc  t0,0x0
    0x28593 + (reset_vec_size * 4 << 20),       // addi   a1, t0, &dtb
    0xf1402573,                                 // csrr   a0, mhartid
    get_core(0)->get_xlen() == 32 ? //t0 containst 0x1000 at this point and adding 24 to it makes it check 6 words later, which points to the start_pc as you can see below.
      0x0182a283u :                             // lw     t0,24(t0)
      0x0182b283u,                              // ld     t0,24(t0)
    0x28067,                                    // jr     t0
    0,
    (uint32_t) (start_pc & 0xffffffff), //This is the PC that is used by the above instructions to jump to the start pc.
    (uint32_t) (start_pc >> 32),
    (uint32_t) (nenclaves & 0xffffffff)
  };

  for (size_t i = 0; i < nenclaves; i++) {
    //TODO actually use the procs.id value
    reset_vec[i + 1 + reset_vec_size] = procs.size() - nenclaves + i; //This statement assumes all ids are from 0..procs.size()-1
  }

#ifdef PRAESIDIO_DEBUG
  fprintf(stderr, "sim.cc: reset vector contents.\n");
  for(size_t i = 0; i < reset_vec_size + nenclaves + 1; i++) {
    fprintf(stderr, "sim.cc: 0x%08x,\n", reset_vec[i]);
  }
#endif

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
  //fprintf(stderr, "sim.cc: physical addr 0x%0lx is found on device with base 0x%0lx and host addres 0x%0lx\n", addr, desc.first, desc.second);
  abstract_mem_t *mem = (abstract_mem_t *) desc.second;
  if (mem) {
    if (addr - desc.first < mem->size()) {
      return mem->contents() + (addr - desc.first);
    }
  }
  fprintf(stderr, "sim.cc: ERROR returning NULL for address to memory: 0x%016lx.\n", addr);
  exit(-1);
  return NULL;
}

// htif

void sim_t::reset()
{
  //fprintf(stderr, "sim.cc: resetting.\n");
  //make_enclave_pages();
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
  debug_module.proc_reset(id);
}

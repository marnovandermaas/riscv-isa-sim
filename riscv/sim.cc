// See LICENSE for license details.
// Copyright 2018-2020 Marno van der Maas

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

void sim_t::output_stats(reg_t label)
{
  fprintf(stat_log, "%lu, %lu, %lu, ", label, procs[0]->get_csr(CSR_MINSTRET), procs[0]->get_state()->minstretpriv);
  for(size_t i = 0; i < nenclaves + 1; i++)
  {
    if(ics[i]) {
      ics[i]->print_stats(stat_log);
    }
    if(dcs[i]) {
      dcs[i]->print_stats(stat_log);
    }
    if(rmts[i]) {
      rmts[i]->print_stats(stat_log);
    }
    if(static_llc[i]) {
      static_llc[i]->print_stats(stat_log);
    }
  }
  if(l2 != NULL)
  {
    l2->print_stats(stat_log);
  }
  fprintf(stat_log, "\n");
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
  output_stats();
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
             unsigned max_bus_master_bits, bool require_authentication, icache_sim_t **ics, dcache_sim_t **dcs, l2cache_sim_t *l2, l2cache_sim_t **rmts, l2cache_sim_t **static_llc, reg_t num_of_pages, FILE *_stat_log)
  : htif_t(args), mems(mems), procs(std::max(nprocs, size_t(1))), nenclaves(nenclaves),
    start_pc(start_pc), current_step(0), current_proc(0), debug(false),
    histogram_enabled(false), dtb_enabled(true), remote_bitbang(NULL),
    debug_module(this, progsize, max_bus_master_bits, require_authentication), ics(ics), dcs(dcs), l2(l2), rmts(rmts), static_llc(static_llc)
{
  signal(SIGINT, &handle_signal);
#ifdef PRAESIDIO_DEBUG
  fprintf(stderr, "sim.cc: Constructing simulator with %lu processors and %lu enclaves.\n", nprocs, nenclaves);
#endif
  if(_stat_log != NULL) {
    stat_log = _stat_log;
  }

  unaccounted_for_steps = 0;

  tag_directory = new page_tag_t[num_of_pages];
  for(reg_t i = 0; i < num_of_pages; i++)
  {
    tag_directory[i].owner = ENCLAVE_DEFAULT_ID;
    tag_directory[i].reader = ENCLAVE_INVALID_ID;
  }

  for (auto& x : mems)
    bus.add_device(x.first, x.second);

  debug_module.add_device(&bus);

  debug_mmu = new mmu_t(this, NULL, tag_directory, num_of_pages);

  if (hartids.size() == 0)
  {
    for (size_t i = 0; i < procs.size() - nenclaves; i++)
    {
      procs[i] = new processor_t(isa, this, i, ENCLAVE_DEFAULT_ID, tag_directory, num_of_pages, halted);
    }
    enclave_id_t current_id = 1;
    for (size_t i = procs.size() - nenclaves; i < procs.size(); i++)
    {
      procs[i] = new processor_t(isa, this, i, current_id, tag_directory, num_of_pages, halted);
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

void sim_t::process_enclave_read_access(reg_t paddr, enclave_id_t writer_id, enclave_id_t reader_id) {
    //for all processors:
    //  is the enclave identifier the reader?
    //      call DC function to check presence and invalidate line
    //  is the enclave identifier the writer?
    //      call DC function to perform writeback
    unsigned int reader_i = 0;
    bool found_reader = false;
    bool writeback_done = false;
    for(unsigned int i = 0; i < nenclaves + 1; i++) {
        enclave_id_t current_id = procs[procs.size() - nenclaves - 1 + i]->get_enclave_id();
        if(dcs[i]) {
            if(current_id == reader_id) {
                reader_i = i;
                found_reader = true;
            } else if (current_id == writer_id) {
                if(dcs[i]->perform_writeback(paddr)) {
                    writeback_done = true;
                }
            }
        }
    }
    if(writeback_done && found_reader) {
        dcs[reader_i]->invalidate_address(paddr);
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
#ifdef PRAESIDIO_DEBUG
  fprintf(stderr, "sim.cc: running htif.\n");
#endif

  fprintf(stat_log, "label, instruction count (core 0), privileged instruction count (core 0), cache stats ...\n");
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
        unaccounted_for_steps += INTERLEAVE % INSNS_PER_RTC_TICK;
        if(unaccounted_for_steps >= INSNS_PER_RTC_TICK) {
            clint->increment(1);
            unaccounted_for_steps -= INSNS_PER_RTC_TICK;
        }
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
#ifdef MANAGEMENT_SHIM_INSTRUCTIONS
    //TODO add the entry point to the management enclave code instead of hardcoding it.
    if(nenclaves > 0) {
      start_pc = MANAGEMENT_SHIM_BASE;
    }
#endif //MANAGEMENT_SHIM_INSTRUCTIONS
  }
#ifdef PRAESIDIO_DEBUG
  fprintf(stderr, "sim.cc: Adding boot rom with start_pc %016lx\n", start_pc);
#endif //PRAESIDIO_DEBUG

  uint32_t reset_vec[reset_vec_size] = {
    0x297,                                      // 0x1000: auipc  t0,0x0
    0x28593 + (reset_vec_size * 4 << 20),       // 0x1004: addi   a1, t0, &dtb
    0xf1402573,                                 // 0x1008: csrr   a0, mhartid
    get_core(0)->get_xlen() == 32 ? //t0 containst 0x1000 at this point and adding 24 to it makes it check 6 words later, which points to the start_pc as you can see below.
      0x0182a283u :                             // 0x100c: lw     t0,24(t0)
      0x0182b283u,                              // 0x100c: ld     t0,24(t0)
    0x28067,                                    // 0x1010: jr     t0
    0,
    (uint32_t) (start_pc & 0xffffffff),         // 0x1014: This is the PC that is used by the above instructions to jump to the start pc.
    (uint32_t) (start_pc >> 32),                // 0x1018:
  };

  uint32_t enclave_vec[nenclaves + 1] = {
    (uint32_t) (nenclaves & 0xffffffff)
  };

  for (size_t i = 0; i < nenclaves; i++) {
    //TODO actually use the procs.id value
    enclave_vec[i + 1] = procs.size() - nenclaves + i; //This statement assumes all ids are from 0..procs.size()-1
  }

  std::vector<char> rom((char*)reset_vec, (char*)reset_vec + sizeof(reset_vec));

  dts = make_dts(INSNS_PER_RTC_TICK, CPU_HZ, procs, mems, nenclaves);
  std::string dtb = dts_compile(dts);

  rom.insert(rom.end(), dtb.begin(), dtb.end());
  const int align = 0x1000;
  rom.resize((rom.size() + align - 1) / align * align);

  boot_rom.reset(new rom_device_t(rom));
  bus.add_device(DEFAULT_RSTVEC, boot_rom.get());

  std::vector<char> enclave_rom_vec((char*) enclave_vec, (char*)enclave_vec + sizeof(enclave_vec));
  enclave_rom_vec.resize((enclave_rom_vec.size() + align - 1) / align * align);

  if(rom.size() > align) {
    fprintf(stderr, "ERROR: enclave rom cannot be located at 0x2000 because the device tree is too big.\n");
    exit(-1);
  }

  enclave_rom.reset(new rom_device_t(enclave_rom_vec));
  bus.add_device(DEFAULT_RSTVEC + align, enclave_rom.get());
}

char* sim_t::addr_to_mem(reg_t addr) {
  auto desc = bus.find_device(addr);
  auto mem = dynamic_cast<mem_t*>(desc.second);
  if (mem) {
    if (addr - desc.first < mem->size()) {
      return mem->contents() + (addr - desc.first);
    }
  }
  return NULL;
}

// htif

void sim_t::reset()
{
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
  auto data = debug_mmu->load_uint64(taddr, ENCLAVE_DEFAULT_ID);
  memcpy(dst, &data, sizeof data);
}

void sim_t::write_chunk(addr_t taddr, size_t len, const void* src)
{
  assert(len == 8);
  uint64_t data;
  memcpy(&data, src, sizeof data);
  debug_mmu->store_uint64(taddr, data, ENCLAVE_DEFAULT_ID);
}

void sim_t::proc_reset(unsigned id)
{
  debug_module.proc_reset(id);
}

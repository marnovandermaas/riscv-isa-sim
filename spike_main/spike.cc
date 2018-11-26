// See LICENSE for license details.

#include "sim.h"
#include "mmu.h"
#include "remote_bitbang.h"
#include "cachesim.h"
#include "extension.h"
#include <dlfcn.h>
#include <fesvr/option_parser.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <memory>

static void help()
{
  fprintf(stderr, "usage: spike [host options] <target program> [target options]\n");
  fprintf(stderr, "Host Options:\n");
  fprintf(stderr, "  -p<n>                 Simulate <n> processors [default 1]\n");
  fprintf(stderr, "  -m<n>                 Provide <n> MiB of target memory [default 2048]\n");
  fprintf(stderr, "  -m<a:m,b:n,...>       Provide memory regions of size m and n bytes\n");
  fprintf(stderr, "                          at base addresses a and b (with 4 KiB alignment)\n");
  fprintf(stderr, "  -d                    Interactive debug mode\n");
  fprintf(stderr, "  -g                    Track histogram of PCs\n");
  fprintf(stderr, "  -l                    Generate a log of execution\n");
  fprintf(stderr, "  -h                    Print this help message\n");
  fprintf(stderr, "  -H                    Start halted, allowing a debugger to connect\n");
  fprintf(stderr, "  --isa=<name>          RISC-V ISA string [default %s]\n", DEFAULT_ISA);
  fprintf(stderr, "  --pc=<address>        Override ELF entry point\n");
  fprintf(stderr, "  --hartids=<a,b,...>   Explicitly specify hartids, default is 0,1,...\n");
  fprintf(stderr, "  --ic=<S>:<W>:<B>      Instantiate a cache model with S sets,\n");
  fprintf(stderr, "  --dc=<S>:<W>:<B>        W ways, and B-byte blocks (with S and\n");
  fprintf(stderr, "  --l2=<S>:<W>:<B>        B both powers of 2).\n");
  fprintf(stderr, "  --extension=<name>    Specify RoCC Extension\n");
  fprintf(stderr, "  --enclave=<number>    Number of enclave threads to add [default 0]\n");
  fprintf(stderr, "  --extlib=<name>       Shared library to load\n");
  fprintf(stderr, "  --rbb-port=<port>     Listen on <port> for remote bitbang connection\n");
  fprintf(stderr, "  --dump-dts            Print device tree string and exit\n");
  fprintf(stderr, "  --disable-dtb         Don't write the device tree blob into memory\n");
  fprintf(stderr, "  --progsize=<words>    Progsize for the debug module [default 2]\n");
  fprintf(stderr, "  --debug-sba=<bits>    Debug bus master supports up to "
      "<bits> wide accesses [default 0]\n");
  fprintf(stderr, "  --debug-auth          Debug module requires debugger to authenticate\n");
  exit(1);
}

static std::vector<std::pair<reg_t, mem_t*>> make_mems(const char* arg, reg_t *num_of_pages)
{
  // handle legacy mem argument
  char* p;
  auto mb = strtoull(arg, &p, 0);
  if (*p == 0) {
    reg_t size = reg_t(mb) << 20;
    if (size != (size_t)size)
      throw std::runtime_error("Size would overflow size_t");
    *num_of_pages = size / PGSIZE;
    if ((size % PGSIZE) != 0) *num_of_pages++;
    return std::vector<std::pair<reg_t, mem_t*>>(1, std::make_pair(reg_t(DRAM_BASE), new mem_t(size)));
  }
  
  fprintf(stderr, "spike.cc: ERROR currently preasidio does not support multiple mapped working memories.\n");
  exit(-1);
  // handle base/size tuples
  std::vector<std::pair<reg_t, mem_t*>> res;
  while (true) {
    auto base = strtoull(arg, &p, 0);
    if (!*p || *p != ':')
      help();
    auto size = strtoull(p + 1, &p, 0);
    if ((size | base) % PGSIZE != 0)
      help();
    res.push_back(std::make_pair(reg_t(base), new mem_t(size)));
    if (!*p)
      break;
    if (*p != ',')
      help();
    arg = p + 1;
  }
  return res;
}

int main(int argc, char** argv)
{
  bool debug = false;
  bool halted = false;
  bool histogram = false;
  bool log = false;
  typedef uint64_t enclave_id_t;
  bool dump_dts = false;
  bool dtb_enabled = true;
  size_t nprocs = 1;
  size_t nenclaves = 0;
  reg_t start_pc = reg_t(-1);
  std::vector<std::pair<reg_t, mem_t*>> mems;
  const char* ic_string = NULL;
  const char* dc_string = NULL;
  const char* llc_string = NULL;
  std::unique_ptr<cache_sim_t> l2;
  std::unique_ptr<partitioned_cache_sim_t> partitioned_l2;
  std::function<extension_t*()> extension;
  const char* isa = DEFAULT_ISA;
  uint16_t rbb_port = 0;
  bool use_rbb = false;
  unsigned progsize = 2;
  unsigned max_bus_master_bits = 0;
  bool require_authentication = false;
  reg_t num_of_pages = 0;
  std::vector<int> hartids;

  auto const hartids_parser = [&](const char *s) {
    std::string const str(s);
    std::stringstream stream(str);

    int n;
    while (stream >> n)
    {
      hartids.push_back(n);
      if (stream.peek() == ',') stream.ignore();
    }
  };

  option_parser_t parser;
  parser.help(&help);
  parser.option('h', 0, 0, [&](const char* s){help();});
  parser.option('d', 0, 0, [&](const char* s){debug = true;});
  parser.option('g', 0, 0, [&](const char* s){histogram = true;});
  parser.option('l', 0, 0, [&](const char* s){log = true;});
  parser.option('p', 0, 1, [&](const char* s){nprocs = atoi(s);});
  parser.option('m', 0, 1, [&](const char* s){mems = make_mems(s, &num_of_pages);});
  // I wanted to use --halted, but for some reason that doesn't work.
  parser.option('H', 0, 0, [&](const char* s){halted = true;});
  parser.option(0, "rbb-port", 1, [&](const char* s){use_rbb = true; rbb_port = atoi(s);});
  parser.option(0, "pc", 1, [&](const char* s){start_pc = strtoull(s, 0, 0);});
  parser.option(0, "hartids", 1, hartids_parser);
  parser.option(0, "ic", 1, [&](const char* s){ic_string = s;});
  parser.option(0, "dc", 1, [&](const char* s){dc_string = s;});
  parser.option(0, "l2", 1, [&](const char* s){llc_string = s;});
  parser.option(0, "isa", 1, [&](const char* s){isa = s;});
  parser.option(0, "extension", 1, [&](const char* s){extension = find_extension(s);});
  parser.option(0, "enclave", 1, [&](const char* s){nenclaves = atoi(s);});
  parser.option(0, "dump-dts", 0, [&](const char *s){dump_dts = true;});
  parser.option(0, "disable-dtb", 0, [&](const char *s){dtb_enabled = false;});
  parser.option(0, "extlib", 1, [&](const char *s){
    void *lib = dlopen(s, RTLD_NOW | RTLD_GLOBAL);
    if (lib == NULL) {
      fprintf(stderr, "Unable to load extlib '%s': %s\n", s, dlerror());
      exit(-1);
    }
  });
  parser.option(0, "progsize", 1, [&](const char* s){progsize = atoi(s);});
  parser.option(0, "debug-sba", 1,
      [&](const char* s){max_bus_master_bits = atoi(s);});
  parser.option(0, "debug-auth", 0,
      [&](const char* s){require_authentication = true;});

  auto argv1 = parser.parse(argv);
  std::vector<std::string> htif_args(argv1, (const char*const*)argv + argc);
  if (mems.empty())
    mems = make_mems("2048", &num_of_pages);

  if (!*argv1)
    help();

  std::unique_ptr<icache_sim_t> ics[nenclaves + 1];
  std::unique_ptr<dcache_sim_t> dcs[nenclaves + 1];
  std::unique_ptr<remapping_table_t> rmts[nenclaves + 1];
  icache_sim_t *ics_arg[nenclaves + 1];
  dcache_sim_t *dcs_arg[nenclaves + 1];
  remapping_table_t *rmts_arg[nenclaves + 1];

  size_t sets, ways, linesz;
  if(llc_string) {
    if(!nenclaves) {
      l2.reset(cache_sim_t::construct(llc_string, "L2$"));
    } else {
      fprintf(stderr, "spike.cc: Initializing partitioned cache.\n");
      cache_sim_t::parse_config_string(llc_string, &sets, &ways, &linesz);
      partitioned_l2.reset(new partitioned_cache_sim_t(sets*ways));
    }
  }

  for(size_t i = 0; i < nenclaves + 1; i++) {
    if (ic_string != NULL) {
      icache_sim_t *icache = new icache_sim_t(ic_string);
      ics[i].reset(icache);
      ics_arg[i] = icache;
    } else {
      ics_arg[i] = NULL;
    }
    if (dc_string != NULL) {
      dcache_sim_t *dcache = new dcache_sim_t(dc_string);
      dcs[i].reset(dcache);
      dcs_arg[i] = dcache;
    } else {
      dcs_arg[i] = NULL;
    }
    if ( llc_string != NULL && nenclaves > 0) {
      remapping_table_t *rmt = new remapping_table_t(sets, ways, linesz, "RMT", &*partitioned_l2, i);
      rmts[i].reset(rmt);
      rmts_arg[i] = rmt;
    } else {
      rmts_arg[i] = NULL;
    }
  }


  sim_t s(isa, nprocs + nenclaves, nenclaves, halted, start_pc, mems, htif_args, std::move(hartids),
      progsize, max_bus_master_bits, require_authentication, ics_arg, dcs_arg, &*l2, rmts_arg, num_of_pages);
  std::unique_ptr<remote_bitbang_t> remote_bitbang((remote_bitbang_t *) NULL);
  std::unique_ptr<jtag_dtm_t> jtag_dtm(new jtag_dtm_t(&s.debug_module));
  if (use_rbb) {
    remote_bitbang.reset(new remote_bitbang_t(rbb_port, &(*jtag_dtm)));
    s.set_remote_bitbang(&(*remote_bitbang));
  }
  s.set_dtb_enabled(dtb_enabled);

  if (dump_dts) {
    printf("%s", s.get_dts());
    return 0;
  }

  if (ic_string) {
    if(l2)
      ics[0]->set_miss_handler(&*l2);
    if(partitioned_l2)
      ics[0]->set_miss_handler(rmts_arg[0]);
  }
  if (dc_string) {
    if(l2)
      dcs[0]->set_miss_handler(&*l2);
    if(partitioned_l2)
      dcs[0]->set_miss_handler(rmts_arg[0]);
  }
  for (size_t i = 0; i < nprocs; i++)
  {
    if (ic_string) s.get_core(i)->get_mmu()->register_memtracer(&*ics[0]);
    if (dc_string) s.get_core(i)->get_mmu()->register_memtracer(&*dcs[0]);
    if (extension) s.get_core(i)->register_extension(extension());
  }

  for (size_t i = 0; i < nenclaves; i++)
  {
    size_t core_id = i + nprocs;
    if (ic_string) {
      s.get_core(core_id)->get_mmu()->register_memtracer(&*ics[i+1]);
      if(l2) {
        ics[i+1]->set_miss_handler(&*l2);
      }
      if(partitioned_l2) {
        ics[i+1]->set_miss_handler(&*rmts[i+1]);
      }
    }
    if (dc_string) {
      s.get_core(core_id)->get_mmu()->register_memtracer(&*dcs[i+1]);
      if(l2) {
        dcs[i+1]->set_miss_handler(&*l2);
      }
      if(partitioned_l2) {
        dcs[i+1]->set_miss_handler(&*rmts[i+1]);
      }
    }
    if (extension) s.get_core(core_id)->register_extension(extension());
  }

  s.set_debug(debug);
  s.set_log(log);
  s.set_histogram(histogram);
  fprintf(stderr, "spike.cc: starting simulation.\n");
  return s.run();
}

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
#include "debug.h"

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
#ifdef COVERT_CHANNEL_POC
  fprintf(stderr, "  --dram-banks          Instantiate banks with 2^14 rows of 2^13 Bytes\n");
#endif
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

static std::vector<std::pair<reg_t, mem_t*>> make_mems(const char* arg, reg_t *num_of_pages, size_t num_enclaves, reg_t* dram_size)
{
  // handle legacy mem argument
  char* p;
  auto mb = strtoull(arg, &p, 0);
  if (*p == 0) {
    reg_t size = reg_t(mb) << 20;
    if (size != (size_t)size)
      throw std::runtime_error("Size would overflow size_t");
    *dram_size = size;
    *num_of_pages = size / PGSIZE;
    if ((size % PGSIZE) != 0) *num_of_pages++;
    int num_mems = 1;

#ifdef MARNO_DEBUG
    fprintf(stderr, "spike.cc: creating vector with %d elements.\n", num_mems);
#endif
    std::vector<std::pair<reg_t, mem_t*>> memory_vector = std::vector<std::pair<reg_t, mem_t*>>(num_mems, std::make_pair(reg_t(0), (mem_t *) NULL));

#ifdef MARNO_DEBUG
    fprintf(stderr, "spike.cc: making working memory at base: 0x%0x\n", DRAM_BASE);
#endif //MARNO_DEBUG
    memory_vector[0] = std::make_pair(reg_t(DRAM_BASE), new mem_t(size));
    return memory_vector;
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
  const char* llc_partition_string = NULL;
#ifdef COVERT_CHANNEL_POC
  bool enable_banks = false;
  std::unique_ptr<dram_bank_t> dram_bank;
  reg_t dram_size = 0;
#endif //COVERT_CHANNEL_POC
  std::unique_ptr<l2cache_sim_t> l2;
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
  parser.option(0, "enclave", 1, [&](const char* s){
    nenclaves = atoi(s);
  });
  parser.option('h', 0, 0, [&](const char* s){help();});
  parser.option('d', 0, 0, [&](const char* s){debug = true;});
  parser.option('g', 0, 0, [&](const char* s){histogram = true;});
  parser.option('l', 0, 0, [&](const char* s){log = true;});
  parser.option('p', 0, 1, [&](const char* s){nprocs = atoi(s);});
  parser.option('m', 0, 1, [&](const char* s){
    mems = make_mems(s, &num_of_pages, nenclaves, &dram_size);
  });
  // I wanted to use --halted, but for some reason that doesn't work.
  parser.option('H', 0, 0, [&](const char* s){halted = true;});
  parser.option(0, "rbb-port", 1, [&](const char* s){use_rbb = true; rbb_port = atoi(s);});
  parser.option(0, "pc", 1, [&](const char* s){start_pc = strtoull(s, 0, 0);});
  parser.option(0, "hartids", 1, hartids_parser);
  parser.option(0, "ic", 1, [&](const char* s){ic_string = s;});
  parser.option(0, "dc", 1, [&](const char* s){dc_string = s;});
  parser.option(0, "l2", 1, [&](const char* s){llc_string = s;});
  parser.option(0, "l2_partitioning", 1, [&](const char* s){llc_partition_string = s;});
#ifdef COVERT_CHANNEL_POC
  parser.option(0, "dram-banks", 0, [&](const char* s){enable_banks = true;});
#endif
  parser.option(0, "isa", 1, [&](const char* s){isa = s;});
  parser.option(0, "extension", 1, [&](const char* s){extension = find_extension(s);});
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
    mems = make_mems("2048", &num_of_pages, nenclaves, &dram_size);

  if (!*argv1)
    help();

  reg_t sender_base = 0x80004000;
#ifdef COVERT_CHANNEL_POC
  if (enable_banks) {
    sender_base = 0x80020000;
    if(ic_string != NULL || dc_string != NULL || l2 != NULL || llc_partition_string != NULL) {
      fprintf(stderr, "spike.cc: ERROR DRAM banks cannot be enabled when other caching is enabled.\n");
      exit(-1);
    }
    if(dram_size == 0) {
      throw std::runtime_error("DRAM cannot be size 0\n");
    }
    //Test is dram_size is power of 2
    if((dram_size & (dram_size - 1)) != 0) {
      throw std::runtime_error("DRAM needs to be power of 2 if you enable banks\n");
    }
    //Find log2(dram_size)
    int num_bits = sizeof(reg_t)*8;
    reg_t mask = 1;
    uint64_t address_bits = 0;
    while ((num_bits /= 2) != 0) {
      mask <<= num_bits;
      if(mask > dram_size) {
        mask >>= num_bits;
      } else {
        address_bits += num_bits;
      }
    }
    dram_bank.reset(new dram_bank_t(address_bits, 14, 13, 0));
  }
#endif


  std::unique_ptr<icache_sim_t> ics[nenclaves + 1];
  std::unique_ptr<dcache_sim_t> dcs[nenclaves + 1];
  std::unique_ptr<l2cache_sim_t> rmts[nenclaves + 1];
  std::unique_ptr<l2cache_sim_t> static_llc[nenclaves + 1];
  icache_sim_t *ics_arg[nenclaves + 1];
  dcache_sim_t *dcs_arg[nenclaves + 1];
  l2cache_sim_t *rmts_arg[nenclaves + 1];
  l2cache_sim_t *static_llc_arg[nenclaves + 1];

  size_t sets, ways, linesz;
  #define CACHE_PARTITIONING_NONE 0
  #define CACHE_PARTITIONING_RMT 1
  #define CACHE_PARTITIONING_STATIC 2
  int cache_partitioning_type = 0;
  if(llc_string) {
    if(!nenclaves) {
      l2.reset(new l2cache_sim_t(llc_string, "L2$"));
    } else {
      if(llc_partition_string) {
        if(atoi(llc_partition_string) == CACHE_PARTITIONING_NONE) {
          cache_partitioning_type = CACHE_PARTITIONING_NONE;
          l2.reset(new l2cache_sim_t(llc_string, "L2$"));
        } else if(atoi(llc_partition_string) == CACHE_PARTITIONING_RMT) {
          cache_partitioning_type = CACHE_PARTITIONING_RMT;
#ifdef MARNO_DEBUG
          fprintf(stderr, "spike.cc: Initializing partitioned cache.\n");
#endif
          cache_sim_t::parse_config_string(llc_string, &sets, &ways, &linesz);
          partitioned_l2.reset(new partitioned_cache_sim_t(sets*ways));
        } else if(atoi(llc_partition_string) == CACHE_PARTITIONING_STATIC) {
          cache_partitioning_type = CACHE_PARTITIONING_STATIC;
          cache_sim_t::parse_config_string(llc_string, &sets, &ways, &linesz);
        } else {
          fprintf(stderr, "spike.cc: ERROR please define l2 cache partitioning scheme if you would like to use enclaves. You can do this by specifying --l2partitioning= and setting it to 0 for none, 1 for rmt or 2 for static.\n");
          exit(-1);
        }
      } else {
        fprintf(stderr, "spike.cc: ERROR please define l2 cache partitioning scheme if you would like to use enclaves. You can do this by specifying --l2partitioning= and setting it to 0 for none, 1 for rmt or 2 for static.\n");
        exit(-1);
      }
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
    if ( llc_string != NULL && cache_partitioning_type == CACHE_PARTITIONING_RMT) {
      l2cache_sim_t *rmt = new l2cache_sim_t(sets, ways, linesz, "RMT", &*partitioned_l2, i);
      rmts[i].reset(rmt);
      rmts_arg[i] = rmt;
    } else {
      rmts_arg[i] = NULL;
    }
    if ( llc_string != NULL && cache_partitioning_type == CACHE_PARTITIONING_STATIC) {
      if(nenclaves != 1) {
        fprintf(stderr, "spike.cc: ERROR static partitioning currently only supported for 1 enclave.\n");
        exit(-1);
      }
      l2cache_sim_t *static_partition = new l2cache_sim_t(i == 0 ? sets / 2 : sets / 4, ways, linesz, "SPLLC");
      static_llc[i].reset(static_partition);
      static_llc_arg[i] = static_partition;
    } else {
      static_llc_arg[i] = NULL;
    }
  }

  sim_t s(isa, nprocs + nenclaves, nenclaves, halted, start_pc, mems, htif_args, std::move(hartids),
      progsize, max_bus_master_bits, require_authentication, ics_arg, dcs_arg, &*l2, rmts_arg,
      static_llc_arg, num_of_pages, sender_base);
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
  l2cache_sim_t *l2_cachesim = NULL;
  if(l2) {
    l2_cachesim = &*l2;
  } else if (partitioned_l2) {
    l2_cachesim = &*rmts[0];
  } else if (static_llc_arg[0]) {
    l2_cachesim = &*static_llc[0];
  }
  if( (ic_string && l2_cachesim && !dc_string) ||
      (dc_string && l2_cachesim && !ic_string)) {
        fprintf(stderr, "ERROR: currently not supporting having only one of instruction and data cache, while also having an L2 cache. Please have just the L2 cache or enable all three.\n");
        exit(-1);
      }
  if(l2_cachesim != NULL) {
    if (ic_string) {
      ics[0]->set_miss_handler(l2_cachesim);
    }
    if (dc_string) {
      dcs[0]->set_miss_handler(l2_cachesim);
    }
  }
  for (size_t i = 0; i < nprocs; i++)
  {
    if (ic_string) {
      s.get_core(i)->get_mmu()->register_memtracer(&*ics[0]);
    } else if (l2_cachesim != NULL) {
      s.get_core(i)->get_mmu()->register_memtracer(l2_cachesim);
    }
    if (dc_string) {
      s.get_core(i)->get_mmu()->register_memtracer(&*dcs[0]);
    } //l2 is already attached by ic logic if necessary
    if (enable_banks) {
      s.get_core(i)->get_mmu()->register_memtracer(&*dram_bank);
    }
    if (extension) s.get_core(i)->register_extension(extension());
  }

  for (size_t i = 0; i < nenclaves; i++)
  {
    size_t core_id = i + nprocs;
    l2cache_sim_t *l2_cachesim = NULL;
    if(l2) {
      l2_cachesim = &*l2;
    } else if(partitioned_l2) {
      l2_cachesim = &*rmts[i+1];
    } else if(static_llc_arg[0]) {
      l2_cachesim = &*static_llc[i+1];
    }
    if (ic_string) {
      s.get_core(core_id)->get_mmu()->register_memtracer(&*ics[i+1]);
      ics[i+1]->set_miss_handler(l2_cachesim);
    } else if(l2_cachesim != NULL) {
      s.get_core(core_id)->get_mmu()->register_memtracer(l2_cachesim);
    }
    if (dc_string) {
      s.get_core(core_id)->get_mmu()->register_memtracer(&*dcs[i+1]);
      dcs[i+1]->set_miss_handler(l2_cachesim);
    } //l2 is already attached by ic logic if necessary
    if(enable_banks) {
      s.get_core(core_id)->get_mmu()->register_memtracer(&*dram_bank);
    }
    if (extension) s.get_core(core_id)->register_extension(extension());
  }

  s.set_debug(debug);
  s.set_log(log);
  s.set_histogram(histogram);
#ifdef MARNO_DEBUG
  fprintf(stderr, "spike.cc: starting simulation.\n");
#endif
  return s.run();
}

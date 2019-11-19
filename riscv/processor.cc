// See LICENSE for license details.

#include "processor.h"
#include "extension.h"
#include "common.h"
#include "config.h"
#include "simif.h"
#include "mmu.h"
#include "disasm.h"
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <assert.h>
#include <limits.h>
#include <stdexcept>
#include <algorithm>
#include "debug.h"

#undef STATE
#define STATE state

processor_t::processor_t(const char* isa, simif_t* sim, uint32_t id,
        enclave_id_t e_id, page_owner_t *page_owners, size_t num_of_pages, struct Message_t *message, struct Message_t *allMessages, size_t num_of_messages, bool halt_on_reset)
  : debug(false), halt_request(false), sim(sim), ext(NULL), id(id), page_owners(page_owners), num_of_pages(num_of_pages),
  halt_on_reset(halt_on_reset), last_pc(1), executions(1)
{
  enclave_id = e_id;
  mailbox = message;
  allMailboxes = allMessages;
  num_of_mailboxes = num_of_messages;
  parse_isa_string(isa);
  register_base_instructions();

  mmu = new mmu_t(sim, this, page_owners, num_of_pages);

  disassembler = new disassembler_t(max_xlen);
  if (ext)
    for (auto disasm_insn : ext->get_disasms())
      disassembler->add_insn(disasm_insn);

  reset();
}

processor_t::~processor_t()
{
  output_histogram();

  delete mmu;
  delete disassembler;
}

void processor_t::output_histogram() {
#ifdef RISCV_ENABLE_HISTOGRAM
  if (histogram_enabled)
  {
    if(id == 0) fprintf(stdout, "\n>>>>>PC_HISTORGRAM<<<<<\n");
    fprintf(stdout, "Size, %zu\n", pc_histogram.size());
    for (auto it : pc_histogram) {
      fprintf(stdout, "%0" PRIx64 ", %" PRIu64 "\n", it.first, it.second);
    }
  }
#endif
}

static void bad_isa_string(const char* isa)
{
  fprintf(stderr, "error: bad --isa option %s\n", isa);
  abort();
}

void processor_t::parse_isa_string(const char* str)
{
  std::string lowercase, tmp;
  for (const char *r = str; *r; r++)
    lowercase += std::tolower(*r);

  const char* p = lowercase.c_str();
  const char* all_subsets = "imafdqc";

  max_xlen = 64;
  state.misa = reg_t(2) << 62;

  if (strncmp(p, "rv32", 4) == 0)
    max_xlen = 32, state.misa = reg_t(1) << 30, p += 4;
  else if (strncmp(p, "rv64", 4) == 0)
    p += 4;
  else if (strncmp(p, "rv", 2) == 0)
    p += 2;

  if (!*p) {
    p = "imafdc";
  } else if (*p == 'g') { // treat "G" as "IMAFD"
    tmp = std::string("imafd") + (p+1);
    p = &tmp[0];
  } else if (*p != 'i') {
    bad_isa_string(str);
  }

  isa_string = "rv" + std::to_string(max_xlen) + p;
  state.misa |= 1L << ('s' - 'a'); // advertise support for supervisor mode
  state.misa |= 1L << ('u' - 'a'); // advertise support for user mode

  while (*p) {
    state.misa |= 1L << (*p - 'a');

    if (auto next = strchr(all_subsets, *p)) {
      all_subsets = next + 1;
      p++;
    } else if (*p == 'x') {
      const char* ext = p+1, *end = ext;
      while (islower(*end))
        end++;
      register_extension(find_extension(std::string(ext, end - ext).c_str())());
      p = end;
    } else {
      bad_isa_string(str);
    }
  }

  if (supports_extension('D') && !supports_extension('F'))
    bad_isa_string(str);

  if (supports_extension('Q') && !supports_extension('D'))
    bad_isa_string(str);

  if (supports_extension('Q') && max_xlen < 64)
    bad_isa_string(str);

  max_isa = state.misa;
}

void state_t::reset(reg_t max_isa)
{
  memset(this, 0, sizeof(*this));
  misa = max_isa;
  prv = PRV_M;
  pc = DEFAULT_RSTVEC;
  tselect = 0;
  for (unsigned int i = 0; i < num_triggers; i++)
    mcontrol[i].type = 2;
}

void processor_t::set_debug(bool value)
{
  debug = value;
  if (ext)
    ext->set_debug(value);
}

void processor_t::set_histogram(bool value)
{
  histogram_enabled = value;
#ifndef RISCV_ENABLE_HISTOGRAM
  if (value) {
    fprintf(stderr, "PC Histogram support has not been properly enabled;");
    fprintf(stderr, " please re-build the riscv-isa-run project using \"configure --enable-histogram\".\n");
  }
#endif
}

void processor_t::reset()
{
  state.reset(max_isa);
  state.dcsr.halt = halt_on_reset;
  halt_on_reset = false;
  set_csr(CSR_MSTATUS, state.mstatus);

  if (ext)
    ext->reset(); // reset the extension

  if (sim)
    sim->proc_reset(id);
}

// Count number of contiguous 0 bits starting from the LSB.
static int ctz(reg_t val)
{
  int res = 0;
  if (val)
    while ((val & 1) == 0)
      val >>= 1, res++;
  return res;
}

void processor_t::take_interrupt(reg_t pending_interrupts)
{
  reg_t mie = get_field(state.mstatus, MSTATUS_MIE);
  reg_t m_enabled = state.prv < PRV_M || (state.prv == PRV_M && mie);
  reg_t enabled_interrupts = pending_interrupts & ~state.mideleg & -m_enabled;

  reg_t sie = get_field(state.mstatus, MSTATUS_SIE);
  reg_t s_enabled = state.prv < PRV_S || (state.prv == PRV_S && sie);
  // M-ints have highest priority; consider S-ints only if no M-ints pending
  if (enabled_interrupts == 0)
    enabled_interrupts = pending_interrupts & state.mideleg & -s_enabled;

  if (state.dcsr.cause == 0 && enabled_interrupts) {
    // nonstandard interrupts have highest priority
    if (enabled_interrupts >> IRQ_M_EXT)
      enabled_interrupts = enabled_interrupts >> IRQ_M_EXT << IRQ_M_EXT;
    // external interrupts have next-highest priority
    else if (enabled_interrupts & (MIP_MEIP | MIP_SEIP))
      enabled_interrupts = enabled_interrupts & (MIP_MEIP | MIP_SEIP);
    // software interrupts have next-highest priority
    else if (enabled_interrupts & (MIP_MSIP | MIP_SSIP))
      enabled_interrupts = enabled_interrupts & (MIP_MSIP | MIP_SSIP);
    // timer interrupts have next-highest priority
    else if (enabled_interrupts & (MIP_MTIP | MIP_STIP))
      enabled_interrupts = enabled_interrupts & (MIP_MTIP | MIP_STIP);
    else
      abort();

    throw trap_t(((reg_t)1 << (max_xlen-1)) | ctz(enabled_interrupts));
  }
}

static int xlen_to_uxl(int xlen)
{
  if (xlen == 32)
    return 1;
  if (xlen == 64)
    return 2;
  abort();
}

reg_t processor_t::legalize_privilege(reg_t prv)
{
  assert(prv <= PRV_M);

  if (!supports_extension('U'))
    return PRV_M;

  if (prv == PRV_H || !supports_extension('S'))
    return PRV_U;

  return prv;
}

void processor_t::set_privilege(reg_t prv)
{
  mmu->flush_tlb();
  state.prv = legalize_privilege(prv);
}

void processor_t::enter_debug_mode(uint8_t cause)
{
  fprintf(stderr, "Entering debug mode on core %u, because of cause %u\n", id, cause);
  state.dcsr.cause = cause;
  state.dcsr.prv = state.prv;
  set_privilege(PRV_M);
  state.dpc = state.pc;
  state.pc = DEBUG_ROM_ENTRY;
}

void processor_t::take_trap(trap_t& t, reg_t epc)
{
  if (debug) {
    fprintf(stderr, "core %3d: exception %s, epc 0x%016" PRIx64 "\n",
            id, t.name(), epc);
    if (t.has_tval())
      fprintf(stderr, "core %3d:           tval 0x%016" PRIx64 "\n", id,
          t.get_tval());
  }

  if (state.dcsr.cause) {
    if (t.cause() == CAUSE_BREAKPOINT) {
      state.pc = DEBUG_ROM_ENTRY;
    } else {
      state.pc = DEBUG_ROM_TVEC;
    }
    return;
  }

  if (t.cause() == CAUSE_BREAKPOINT && (
              (state.prv == PRV_M && state.dcsr.ebreakm) ||
              (state.prv == PRV_S && state.dcsr.ebreaks) ||
              (state.prv == PRV_U && state.dcsr.ebreaku))) {
    enter_debug_mode(DCSR_CAUSE_SWBP);
    return;
  }

  // by default, trap to M-mode, unless delegated to S-mode
  reg_t bit = t.cause();
  reg_t deleg = state.medeleg;
  bool interrupt = (bit & ((reg_t)1 << (max_xlen-1))) != 0;
  if (interrupt)
    deleg = state.mideleg, bit &= ~((reg_t)1 << (max_xlen-1));
  if (state.prv <= PRV_S && bit < max_xlen && ((deleg >> bit) & 1)) {
    // handle the trap in S-mode
    state.pc = state.stvec;
    state.scause = t.cause();
    state.sepc = epc;
    state.stval = t.get_tval();

    reg_t s = state.mstatus;
    s = set_field(s, MSTATUS_SPIE, get_field(s, MSTATUS_SIE));
    s = set_field(s, MSTATUS_SPP, state.prv);
    s = set_field(s, MSTATUS_SIE, 0);
    set_csr(CSR_MSTATUS, s);
    set_privilege(PRV_S);
  } else {
    reg_t vector = (state.mtvec & 1) && interrupt ? 4*bit : 0;
    state.pc = (state.mtvec & ~(reg_t)1) + vector;
    state.mepc = epc;
    state.mcause = t.cause();
    state.mtval = t.get_tval();

    reg_t s = state.mstatus;
    s = set_field(s, MSTATUS_MPIE, get_field(s, MSTATUS_MIE));
    s = set_field(s, MSTATUS_MPP, state.prv);
    s = set_field(s, MSTATUS_MIE, 0);
    set_csr(CSR_MSTATUS, s);
    set_privilege(PRV_M);
  }
}

void processor_t::disasm(insn_t insn)
{
  uint64_t bits = insn.bits() & ((1ULL << (8 * insn_length(insn.bits()))) - 1);
  if (last_pc != state.pc || last_bits != bits) {
    if (executions != 1) {
      fprintf(stderr, "core %3d: Executed %" PRIx64 " times\n", id, executions);
    }

    fprintf(stderr, "core %3d: 0x%016" PRIx64 " (0x%08" PRIx64 ") %s\n",
            id, state.pc, bits, disassembler->disassemble(insn).c_str());
    last_pc = state.pc;
    last_bits = bits;
    executions = 1;
  } else {
    executions++;
  }
}

int processor_t::paddr_bits()
{
  assert(xlen == max_xlen);
  return max_xlen == 64 ? 50 : 34;
}

void processor_t::set_csr(int which, reg_t val)
{
  val = zext_xlen(val);
  reg_t delegable_ints = MIP_SSIP | MIP_STIP | MIP_SEIP
                       | ((ext != NULL) << IRQ_COP);
  reg_t all_ints = delegable_ints | MIP_MSIP | MIP_MTIP;
  switch (which)
  {
    case CSR_FFLAGS:
      dirty_fp_state;
      state.fflags = val & (FSR_AEXC >> FSR_AEXC_SHIFT);
      break;
    case CSR_FRM:
      dirty_fp_state;
      state.frm = val & (FSR_RD >> FSR_RD_SHIFT);
      break;
    case CSR_FCSR:
      dirty_fp_state;
      state.fflags = (val & FSR_AEXC) >> FSR_AEXC_SHIFT;
      state.frm = (val & FSR_RD) >> FSR_RD_SHIFT;
      break;
    case CSR_MSTATUS: {
      if ((val ^ state.mstatus) &
          (MSTATUS_MPP | MSTATUS_MPRV | MSTATUS_SUM | MSTATUS_MXR))
        mmu->flush_tlb();

      reg_t mask = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_MIE | MSTATUS_MPIE
                 | MSTATUS_FS | MSTATUS_MPRV | MSTATUS_SUM
                 | MSTATUS_MXR | MSTATUS_TW | MSTATUS_TVM
                 | MSTATUS_TSR | MSTATUS_UXL | MSTATUS_SXL |
                 (ext ? MSTATUS_XS : 0);

      reg_t requested_mpp = legalize_privilege(get_field(val, MSTATUS_MPP));
      state.mstatus = set_field(state.mstatus, MSTATUS_MPP, requested_mpp);
      if (supports_extension('S'))
        mask |= MSTATUS_SPP;

      state.mstatus = (state.mstatus & ~mask) | (val & mask);

      bool dirty = (state.mstatus & MSTATUS_FS) == MSTATUS_FS;
      dirty |= (state.mstatus & MSTATUS_XS) == MSTATUS_XS;
      if (max_xlen == 32)
        state.mstatus = set_field(state.mstatus, MSTATUS32_SD, dirty);
      else
        state.mstatus = set_field(state.mstatus, MSTATUS64_SD, dirty);

      state.mstatus = set_field(state.mstatus, MSTATUS_UXL, xlen_to_uxl(max_xlen));
      state.mstatus = set_field(state.mstatus, MSTATUS_UXL, xlen_to_uxl(max_xlen));
      state.mstatus = set_field(state.mstatus, MSTATUS_SXL, xlen_to_uxl(max_xlen));
      // U-XLEN == S-XLEN == M-XLEN
      xlen = max_xlen;
      break;
    }
    case CSR_MIP: {
      reg_t mask = MIP_SSIP | MIP_STIP;
      state.mip = (state.mip & ~mask) | (val & mask);
      break;
    }
    case CSR_MIE:
      state.mie = (state.mie & ~all_ints) | (val & all_ints);
      break;
    case CSR_MIDELEG:
      state.mideleg = (state.mideleg & ~delegable_ints) | (val & delegable_ints);
      break;
    case CSR_MEDELEG: {
      reg_t mask =
        (1 << CAUSE_MISALIGNED_FETCH) |
        (1 << CAUSE_BREAKPOINT) |
        (1 << CAUSE_USER_ECALL) |
        (1 << CAUSE_FETCH_PAGE_FAULT) |
        (1 << CAUSE_LOAD_PAGE_FAULT) |
        (1 << CAUSE_STORE_PAGE_FAULT);
      state.medeleg = (state.medeleg & ~mask) | (val & mask);
      break;
    }
    case CSR_MINSTRET:
    case CSR_MCYCLE:
      if (xlen == 32)
        state.minstret = (state.minstret >> 32 << 32) | (val & 0xffffffffU);
      else
        state.minstret = val;
      // The ISA mandates that if an instruction writes instret, the write
      // takes precedence over the increment to instret.  However, Spike
      // unconditionally increments instret after executing an instruction.
      // Correct for this artifact by decrementing instret here.
      state.minstret--;
      break;
    case CSR_MINSTRETH:
    case CSR_MCYCLEH:
      state.minstret = (val << 32) | (state.minstret << 32 >> 32);
      state.minstret--; // See comment above.
      break;
    case CSR_SCOUNTEREN:
      state.scounteren = val;
      break;
    case CSR_MCOUNTEREN:
      state.mcounteren = val;
      break;
    case CSR_SSTATUS: {
      reg_t mask = SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_SPP | SSTATUS_FS
                 | SSTATUS_XS | SSTATUS_SUM | SSTATUS_MXR;
      return set_csr(CSR_MSTATUS, (state.mstatus & ~mask) | (val & mask));
    }
    case CSR_SIP: {
      reg_t mask = MIP_SSIP & state.mideleg;
      return set_csr(CSR_MIP, (state.mip & ~mask) | (val & mask));
    }
    case CSR_SIE:
      return set_csr(CSR_MIE,
                     (state.mie & ~state.mideleg) | (val & state.mideleg));
    case CSR_SATP: {
      mmu->flush_tlb();
      if (max_xlen == 32)
        state.satp = val & (SATP32_PPN | SATP32_MODE);

#ifdef PRAESIDIO_DEBUG
      if (max_xlen == 64 && get_field(val, SATP64_MODE) != SATP_MODE_SV39 )

        {
          if(get_field(val, SATP64_MODE) == SATP_MODE_OFF)
                    printf("processor.cc: in SATP_MODE_OFF mode\n");

          printf("processor.cc: not in sv39 mode\n");

        }
#endif

      if (max_xlen == 64 && (get_field(val, SATP64_MODE) == SATP_MODE_OFF ||
                             get_field(val, SATP64_MODE) == SATP_MODE_SV39 ||
                             get_field(val, SATP64_MODE) == SATP_MODE_SV48)) {
        state.satp = val & (SATP64_PPN | SATP64_MODE);
#ifdef PRAESIDIO_DEBUG
        printf("processor.cc: setting satp to 0x%lx\n",state.satp);
#endif
      }

      break;
    }
    case CSR_SEPC: state.sepc = val & ~(reg_t)1; break;
    case CSR_STVEC: state.stvec = val >> 2 << 2; break;
    case CSR_SSCRATCH: state.sscratch = val; break;
    case CSR_SCAUSE: state.scause = val; break;
    case CSR_STVAL: state.stval = val; break;
    case CSR_MEPC: state.mepc = val & ~(reg_t)1; break;
    case CSR_MTVEC: state.mtvec = val & ~(reg_t)2; break;
    case CSR_MSCRATCH: state.mscratch = val; break;
    case CSR_MCAUSE: state.mcause = val; break;
    case CSR_MTVAL: state.mtval = val; break;
    case CSR_MISA: {
      // the write is ignored if increasing IALIGN would misalign the PC
      if (!(val & (1L << ('C' - 'A'))) && (state.pc & 2))
        break;

      if (!(val & (1L << ('F' - 'A'))))
        val &= ~(1L << ('D' - 'A'));

      // allow MAFDC bits in MISA to be modified
      reg_t mask = 0;
      mask |= 1L << ('M' - 'A');
      mask |= 1L << ('A' - 'A');
      mask |= 1L << ('F' - 'A');
      mask |= 1L << ('D' - 'A');
      mask |= 1L << ('C' - 'A');
      mask &= max_isa;

      state.misa = (val & mask) | (state.misa & ~mask);
      break;
    }
    case CSR_TSELECT:
      if (val < state.num_triggers) {
        state.tselect = val;
      }
      break;
    case CSR_TDATA1:
      {
        mcontrol_t *mc = &state.mcontrol[state.tselect];
        if (mc->dmode && !state.dcsr.cause) {
          break;
        }
        mc->dmode = get_field(val, MCONTROL_DMODE(xlen));
        mc->select = get_field(val, MCONTROL_SELECT);
        mc->timing = get_field(val, MCONTROL_TIMING);
        mc->action = (mcontrol_action_t) get_field(val, MCONTROL_ACTION);
        mc->chain = get_field(val, MCONTROL_CHAIN);
        mc->match = (mcontrol_match_t) get_field(val, MCONTROL_MATCH);
        mc->m = get_field(val, MCONTROL_M);
        mc->h = get_field(val, MCONTROL_H);
        mc->s = get_field(val, MCONTROL_S);
        mc->u = get_field(val, MCONTROL_U);
        mc->execute = get_field(val, MCONTROL_EXECUTE);
        mc->store = get_field(val, MCONTROL_STORE);
        mc->load = get_field(val, MCONTROL_LOAD);
        // Assume we're here because of csrw.
        if (mc->execute)
          mc->timing = 0;
        trigger_updated();
      }
      break;
    case CSR_TDATA2:
      if (state.mcontrol[state.tselect].dmode && !state.dcsr.cause) {
        break;
      }
      if (state.tselect < state.num_triggers) {
        state.tdata2[state.tselect] = val;
      }
      break;
    case CSR_DCSR:
      state.dcsr.prv = get_field(val, DCSR_PRV);
      state.dcsr.step = get_field(val, DCSR_STEP);
      // TODO: ndreset and fullreset
      state.dcsr.ebreakm = get_field(val, DCSR_EBREAKM);
      state.dcsr.ebreakh = get_field(val, DCSR_EBREAKH);
      state.dcsr.ebreaks = get_field(val, DCSR_EBREAKS);
      state.dcsr.ebreaku = get_field(val, DCSR_EBREAKU);
      state.dcsr.halt = get_field(val, DCSR_HALT);
      fprintf(stderr, "Set halt status register to %d on core %u\n", state.dcsr.halt, id);
      break;
    case CSR_DPC:
      state.dpc = val & ~(reg_t)1;
      break;
    case CSR_DSCRATCH:
      state.dscratch = val;
      break;
#ifdef BARE_METAL_OUTPUT_CSR
    case CSR_BAREMETALOUTPUT:
      fprintf(stderr, "%c", ((int) val & 0xFF));
      break;
    case CSR_BAREMETALEXIT:
      sim->request_halt(id);//exit(0);
      break;
#endif //BARE_METAL_OUTPUT_CSR
#ifdef ENCLAVE_PAGE_COMMUNICATION_SYSTEM
    case CSR_ENCLAVEASSIGNREADER:
      //least significant 16-bits are the enclave ID the rest is page number.
      if(enclave_id == page_owners[val].owner) {//TODO check if val is not out of bounds
        page_owners[val].reader = state.arg_enclave_id;
      }
      break;
    case CSR_ENCLAVEDONATEPAGE:
      if(enclave_id == page_owners[val].owner) {//TODO check if val is not out of bounds
        page_owners[val].owner = state.arg_enclave_id;
      }
      break;
    case CSR_ENCLAVESETARGID:
      state.arg_enclave_id = val;
      break;
#endif //ENCLAVE_PAGE_COMMUNICATION_SYSTEM
#ifdef MANAGEMENT_ENCLAVE_INSTRUCTIONS
    case CSR_MANAGEENCLAVEID:
      //This instruction should only succeed if the pc is in within the management enclave.
      if ((state.pc >= MANAGEMENT_ENCLAVE_BASE) && (state.pc < MANAGEMENT_ENCLAVE_BASE + MANAGEMENT_ENCLAVE_SIZE))
      {
#ifdef PRAESIDIO_DEBUG
        fprintf(stderr, "processor.cc: Enclave ID on core %u changed to 0x%lx\n", id, val);
#endif //PRAESIDIO_DEBUG
        enclave_id = val;
      } else {
#ifdef PRAESIDIO_DEBUG
        fprintf(stderr, "processor.cc: WARNING: pc was not in management enclave code 0x%lx", state.pc);
#endif //PRAESIDIO_DEBUG
      }
      break;
    case CSR_MANAGECHANGEPAGETAG:
      //TODO fail if tag is not cached.
      //This instruction takes as argument an address within the page that needs managing.
      if(enclave_id == ENCLAVE_MANAGEMENT_ID) {
        if(val & DRAM_BASE) {
          int index = (val & (DRAM_BASE - 1)) / PGSIZE; //Assume DRAM_BASE is just one set bit.
#ifdef PRAESIDIO_DEBUG
          fprintf(stderr, "processor.cc: Changing page %d to tag: %lu\n", index, state.arg_enclave_id);
#endif //PRAESIDIO_DEBUG
          page_owners[index].owner = state.arg_enclave_id;
        } else {
          //TODO enable tagging for pages in boot ROM and management pages.
#ifdef PRAESIDIO_DEBUG
          fprintf(stderr, "processor.cc: WARNING: Currently tagging pages outside of DRAM is not supported 0x%lx\n", val);
#endif //PRAESIDIO_DEBUG
        }
      }
      break;
    case CSR_MANAGESENDMESSAGE:
      mailbox->source = enclave_id;
      mailbox->destination = state.arg_enclave_id;
      mailbox->content = val;
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "processor.cc: core %u sending mailbox message: source 0x%016lx, destination 0x%016lx, content 0x%016lx\n", id, mailbox->source, mailbox->destination, mailbox->content);
#endif //PRAESIDIO_DEBUG
      break;
#endif //MANAGEMENT_ENCLAVE_INSTRUCTIONS
#ifdef COVERT_CHANNEL_POC
    case CSR_LLCMISSCOUNT:
      state.llc_miss_count++;
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "processor.cc: setting lcc_miss_count to: 0x%lx\n", state.lcc_miss_count);
#endif //PRAESIDIO_DEBUG
      break;
#endif //COVERT_CHANNEL_POC
  }
}

reg_t processor_t::get_csr(int which)
{
  uint32_t ctr_en = -1;
  if (state.prv < PRV_M)
    ctr_en &= state.mcounteren;
  if (state.prv < PRV_S)
    ctr_en &= state.scounteren;
  bool ctr_ok = (ctr_en >> (which & 31)) & 1;

  if (ctr_ok) {
    if (which >= CSR_HPMCOUNTER3 && which <= CSR_HPMCOUNTER31)
      return 0;
    if (xlen == 32 && which >= CSR_HPMCOUNTER3H && which <= CSR_HPMCOUNTER31H)
      return 0;
  }
  if (which >= CSR_MHPMCOUNTER3 && which <= CSR_MHPMCOUNTER31)
    return 0;
  if (xlen == 32 && which >= CSR_MHPMCOUNTER3H && which <= CSR_MHPMCOUNTER31H)
    return 0;
  if (which >= CSR_MHPMEVENT3 && which <= CSR_MHPMEVENT31)
    return 0;

#ifdef BARE_METAL_OUTPUT_CSR
  if (which == CSR_BAREMETALOUTPUT || which == CSR_BAREMETALEXIT)
  {
    return 0;
  }
#endif //BARE_METAL_OUTPUT_CSR

#ifdef ENCLAVE_PAGE_COMMUNICATION_SYSTEM
  if (which == CSR_ENCLAVEASSIGNREADER || which == CSR_ENCLAVEDONATEPAGE)
  {
    return 0;
  }
  if (which == CSR_ENCLAVESETARGID) {
    return state.arg_enclave_id; //TODO return 0, because this is a security problem.
  }
  if (which == CSR_ENCLAVEGETMAILBOXBASEADDRESS)
  {
    for(size_t i = 0 ; i < num_of_pages; i++)
    {
      if(page_owners[i].reader == enclave_id && page_owners[i].owner == state.arg_enclave_id)
      {
        return (i*PGSIZE) | DRAM_BASE;
      }
    }
    return 0;
  }
#endif //ENCLAVE_PAGE_COMMUNICATION_SYSTEM

#ifdef MANAGEMENT_ENCLAVE_INSTRUCTIONS
  if (which == CSR_MANAGEENCLAVEID)
  {
    return enclave_id;
  }
  if (which == CSR_MANAGECHANGEPAGETAG || which == CSR_MANAGESENDMESSAGE)
  {
    return 0;
  }
  if (which == CSR_MANAGERECEIVEMESSAGE)
  {
    //TODO make this count as an LLC miss.
    for(size_t i = 0; i < num_of_mailboxes; i++) { //This should be done in parallel in hardware
      struct Message_t *message = &allMailboxes[i];
      if(message->destination == enclave_id) {
#ifdef PRAESIDIO_DEBUG
        fprintf(stderr, "processor.cc: core %u at pc 0x%016lx found messages in box %lu, with message: source 0x%016lx, destination 0x%016lx, content 0x%016lx\n", id, state.pc, i, message->source, message->destination, message->content);
#endif //PRAESIDIO_DEBUG
        state.arg_enclave_id = message->source; //TODO this is a security problem.
        message->destination = ENCLAVE_INVALID_ID;
        message->source = ENCLAVE_INVALID_ID;
        return message->content;
      }
    }
    state.arg_enclave_id = ENCLAVE_INVALID_ID;
    return 0;
  }
#endif // MANAGEMENT_ENCLAVE_INSTRUCTIONS

#ifdef COVERT_CHANNEL_POC
  if (which == CSR_LLCMISSCOUNT) {
    return state.llc_miss_count;
  }
#endif //COVERT_CHANNEL_POC

  switch (which)
  {
    case CSR_FFLAGS:
      require_fp;
      if (!supports_extension('F'))
        break;
      return state.fflags;
    case CSR_FRM:
      require_fp;
      if (!supports_extension('F'))
        break;
      return state.frm;
    case CSR_FCSR:
      require_fp;
      if (!supports_extension('F'))
        break;
      return (state.fflags << FSR_AEXC_SHIFT) | (state.frm << FSR_RD_SHIFT);
    case CSR_INSTRET:
    case CSR_CYCLE:
      if (ctr_ok)
        return state.minstret;
      break;
    case CSR_MINSTRET:
    case CSR_MCYCLE:
      return state.minstret;
    case CSR_INSTRETH:
    case CSR_CYCLEH:
      if (ctr_ok && xlen == 32)
        return state.minstret >> 32;
      break;
    case CSR_MINSTRETH:
    case CSR_MCYCLEH:
      if (xlen == 32)
        return state.minstret >> 32;
      break;
    case CSR_SCOUNTEREN: return state.scounteren;
    case CSR_MCOUNTEREN: return state.mcounteren;
    case CSR_SSTATUS: {
      reg_t mask = SSTATUS_SIE | SSTATUS_SPIE | SSTATUS_SPP | SSTATUS_FS
                 | SSTATUS_XS | SSTATUS_SUM | SSTATUS_MXR | SSTATUS_UXL;
      reg_t sstatus = state.mstatus & mask;
      if ((sstatus & SSTATUS_FS) == SSTATUS_FS ||
          (sstatus & SSTATUS_XS) == SSTATUS_XS)
        sstatus |= (xlen == 32 ? SSTATUS32_SD : SSTATUS64_SD);
      return sstatus;
    }
    case CSR_SIP: return state.mip & state.mideleg;
    case CSR_SIE: return state.mie & state.mideleg;
    case CSR_SEPC: return state.sepc & pc_alignment_mask();
    case CSR_STVAL: return state.stval;
    case CSR_STVEC: return state.stvec;
    case CSR_SCAUSE:
      if (max_xlen > xlen)
        return state.scause | ((state.scause >> (max_xlen-1)) << (xlen-1));
      return state.scause;
    case CSR_SATP:
      if (get_field(state.mstatus, MSTATUS_TVM))
        require_privilege(PRV_M);
#ifdef PRAESIDIO_DEBUG
      printf("processor.cc: reading satp 0x%lx\n",state.satp);
#endif //PRAESIDIO_DEBUG
      return state.satp;
    case CSR_SSCRATCH: return state.sscratch;
    case CSR_MSTATUS: return state.mstatus;
    case CSR_MIP: return state.mip;
    case CSR_MIE: return state.mie;
    case CSR_MEPC: return state.mepc & pc_alignment_mask();
    case CSR_MSCRATCH: return state.mscratch;
    case CSR_MCAUSE: return state.mcause;
    case CSR_MTVAL: return state.mtval;
    case CSR_MISA: return state.misa;
    case CSR_MARCHID: return 0;
    case CSR_MIMPID: return 0;
    case CSR_MVENDORID: return 0;
    case CSR_MHARTID: return id;
    case CSR_MTVEC: return state.mtvec;
    case CSR_MEDELEG: return state.medeleg;
    case CSR_MIDELEG: return state.mideleg;
    case CSR_TSELECT: return state.tselect;
    case CSR_TDATA1:
      if (state.tselect < state.num_triggers) {
        reg_t v = 0;
        mcontrol_t *mc = &state.mcontrol[state.tselect];
        v = set_field(v, MCONTROL_TYPE(xlen), mc->type);
        v = set_field(v, MCONTROL_DMODE(xlen), mc->dmode);
        v = set_field(v, MCONTROL_MASKMAX(xlen), mc->maskmax);
        v = set_field(v, MCONTROL_SELECT, mc->select);
        v = set_field(v, MCONTROL_TIMING, mc->timing);
        v = set_field(v, MCONTROL_ACTION, mc->action);
        v = set_field(v, MCONTROL_CHAIN, mc->chain);
        v = set_field(v, MCONTROL_MATCH, mc->match);
        v = set_field(v, MCONTROL_M, mc->m);
        v = set_field(v, MCONTROL_H, mc->h);
        v = set_field(v, MCONTROL_S, mc->s);
        v = set_field(v, MCONTROL_U, mc->u);
        v = set_field(v, MCONTROL_EXECUTE, mc->execute);
        v = set_field(v, MCONTROL_STORE, mc->store);
        v = set_field(v, MCONTROL_LOAD, mc->load);
        return v;
      } else {
        return 0;
      }
      break;
    case CSR_TDATA2:
      if (state.tselect < state.num_triggers) {
        return state.tdata2[state.tselect];
      } else {
        return 0;
      }
      break;
    case CSR_TDATA3: return 0;
    case CSR_DCSR:
      {
        uint32_t v = 0;
        v = set_field(v, DCSR_XDEBUGVER, 1);
        v = set_field(v, DCSR_EBREAKM, state.dcsr.ebreakm);
        v = set_field(v, DCSR_EBREAKH, state.dcsr.ebreakh);
        v = set_field(v, DCSR_EBREAKS, state.dcsr.ebreaks);
        v = set_field(v, DCSR_EBREAKU, state.dcsr.ebreaku);
        v = set_field(v, DCSR_STOPCYCLE, 0);
        v = set_field(v, DCSR_STOPTIME, 0);
        v = set_field(v, DCSR_CAUSE, state.dcsr.cause);
        v = set_field(v, DCSR_STEP, state.dcsr.step);
        v = set_field(v, DCSR_PRV, state.dcsr.prv);
        return v;
      }
    case CSR_DPC:
      return state.dpc & pc_alignment_mask();
    case CSR_DSCRATCH:
      return state.dscratch;
  }
  throw trap_illegal_instruction(0);
}

reg_t illegal_instruction(processor_t* p, insn_t insn, reg_t pc)
{
  throw trap_illegal_instruction(0);
}

insn_func_t processor_t::decode_insn(insn_t insn)
{
  // look up opcode in hash table
  size_t idx = insn.bits() % OPCODE_CACHE_SIZE;
  insn_desc_t desc = opcode_cache[idx];

  if (unlikely(insn.bits() != desc.match)) {
    // fall back to linear search
    insn_desc_t* p = &instructions[0];
    while ((insn.bits() & p->mask) != p->match)
      p++;
    desc = *p;

    if (p->mask != 0 && p > &instructions[0]) {
      if (p->match != (p-1)->match && p->match != (p+1)->match) {
        // move to front of opcode list to reduce miss penalty
        while (--p >= &instructions[0])
          *(p+1) = *p;
        instructions[0] = desc;
      }
    }

    opcode_cache[idx] = desc;
    opcode_cache[idx].match = insn.bits();
  }

  return xlen == 64 ? desc.rv64 : desc.rv32;
}

void processor_t::register_insn(insn_desc_t desc)
{
  instructions.push_back(desc);
}

void processor_t::build_opcode_map()
{
  struct cmp {
    bool operator()(const insn_desc_t& lhs, const insn_desc_t& rhs) {
      if (lhs.match == rhs.match)
        return lhs.mask > rhs.mask;
      return lhs.match > rhs.match;
    }
  };
  std::sort(instructions.begin(), instructions.end(), cmp());

  for (size_t i = 0; i < OPCODE_CACHE_SIZE; i++)
    opcode_cache[i] = {0, 0, &illegal_instruction, &illegal_instruction};
}

void processor_t::register_extension(extension_t* x)
{
  for (auto insn : x->get_instructions())
    register_insn(insn);
  build_opcode_map();
  for (auto disasm_insn : x->get_disasms())
    disassembler->add_insn(disasm_insn);
  if (ext != NULL)
    throw std::logic_error("only one extension may be registered");
  ext = x;
  x->set_processor(this);
}

void processor_t::register_base_instructions()
{
  #define DECLARE_INSN(name, match, mask) \
    insn_bits_t name##_match = (match), name##_mask = (mask);
  #include "encoding.h"
  #undef DECLARE_INSN

  #define DEFINE_INSN(name) \
    REGISTER_INSN(this, name, name##_match, name##_mask)
  #include "insn_list.h"
  #undef DEFINE_INSN

  register_insn({0, 0, &illegal_instruction, &illegal_instruction});
  build_opcode_map();
}

bool processor_t::load(reg_t addr, size_t len, uint8_t* bytes)
{
  switch (addr)
  {
    case 0:
      if (len <= 4) {
        memset(bytes, 0, len);
        bytes[0] = get_field(state.mip, MIP_MSIP);
        return true;
      }
      break;
  }

  return false;
}

bool processor_t::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  switch (addr)
  {
    case 0:
      if (len <= 4) {
        state.mip = set_field(state.mip, MIP_MSIP, bytes[0]);
        return true;
      }
      break;
  }

  return false;
}

void processor_t::trigger_updated()
{
  mmu->flush_tlb();
  mmu->check_triggers_fetch = false;
  mmu->check_triggers_load = false;
  mmu->check_triggers_store = false;

  for (unsigned i = 0; i < state.num_triggers; i++) {
    if (state.mcontrol[i].execute) {
      mmu->check_triggers_fetch = true;
    }
    if (state.mcontrol[i].load) {
      mmu->check_triggers_load = true;
    }
    if (state.mcontrol[i].store) {
      mmu->check_triggers_store = true;
    }
  }
}

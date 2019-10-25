// See LICENSE for license details.

#include "mmu.h"
#include "simif.h"
#include "processor.h"
#include "debug.h"

mmu_t::mmu_t(simif_t* sim, processor_t* proc, page_owner_t *page_owners, size_t num_of_pages)
 : sim(sim), proc(proc), page_owners(page_owners), num_of_pages(num_of_pages),
  check_triggers_fetch(false),
  check_triggers_load(false),
  check_triggers_store(false),
  matched_trigger(NULL)
{
  flush_tlb();
  yield_load_reservation();
}

mmu_t::~mmu_t()
{
}

void mmu_t::flush_icache()
{
  for (size_t i = 0; i < ICACHE_ENTRIES; i++)
    icache[i].tag = -1;
}

void mmu_t::flush_tlb()
{
  memset(tlb_insn_tag, -1, sizeof(tlb_insn_tag));
  memset(tlb_load_tag, -1, sizeof(tlb_load_tag));
  memset(tlb_store_tag, -1, sizeof(tlb_store_tag));

  flush_icache();
}

reg_t mmu_t::translate(reg_t addr, access_type type)
{
  if (!proc)
    return addr;

  reg_t mode = proc->state.prv;
  if (type != FETCH) {
    if (!proc->state.dcsr.cause && get_field(proc->state.mstatus, MSTATUS_MPRV))
      mode = get_field(proc->state.mstatus, MSTATUS_MPP);
  }

  return walk(addr, type, mode) | (addr & (PGSIZE-1));
}

tlb_entry_t mmu_t::fetch_slow_path(reg_t vaddr, enclave_id_t enclave_id)
{
  reg_t paddr = translate(vaddr, FETCH);
  auto host_addr = sim->addr_to_mem(paddr);
  if (host_addr) {
    if(check_identifier(paddr, enclave_id, true)) {
      return refill_tlb(vaddr, paddr, host_addr, FETCH, enclave_id);
    } else {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: Error! Denying fetch to enclave 0x%0lx, virtual address 0x%lx, physical address 0x%lx, number of pages %lu, page size 0x%lx\n", enclave_id, vaddr, host_addr, num_of_pages, PGSIZE);
#endif
      throw trap_instruction_access_fault(vaddr);
    }
  } else {
    if (!sim->mmio_load(paddr, sizeof fetch_temp, (uint8_t*)&fetch_temp))
      throw trap_instruction_access_fault(vaddr);
    tlb_entry_t entry = {(char*)&fetch_temp - vaddr, paddr - vaddr};
    return entry;
  }
}

reg_t reg_from_bytes(size_t len, const uint8_t* bytes)
{
  switch (len) {
    case 1:
      return bytes[0];
    case 2:
      return bytes[0] |
        (((reg_t) bytes[1]) << 8);
    case 4:
      return bytes[0] |
        (((reg_t) bytes[1]) << 8) |
        (((reg_t) bytes[2]) << 16) |
        (((reg_t) bytes[3]) << 24);
    case 8:
      return bytes[0] |
        (((reg_t) bytes[1]) << 8) |
        (((reg_t) bytes[2]) << 16) |
        (((reg_t) bytes[3]) << 24) |
        (((reg_t) bytes[4]) << 32) |
        (((reg_t) bytes[5]) << 40) |
        (((reg_t) bytes[6]) << 48) |
        (((reg_t) bytes[7]) << 56);
  }
  abort();
}

bool mmu_t::check_identifier(reg_t paddr, enclave_id_t id, bool load) {
  if(paddr >= DRAM_BASE && paddr < DRAM_BASE + PGSIZE*num_of_pages) {
    reg_t dram_offset = paddr - DRAM_BASE;
    reg_t page_num = dram_offset / PGSIZE;
    if(load && id == page_owners[page_num].reader) {
      return true;
    }
    return id == page_owners[page_num].owner;
  }
  return true;
}

void mmu_t::load_slow_path(reg_t addr, reg_t len, uint8_t* bytes, enclave_id_t id)
{
  reg_t paddr = translate(addr, LOAD);
  if (auto host_addr = sim->addr_to_mem(paddr)) {
    if(check_identifier(paddr, id, true)) {
      memcpy(bytes, host_addr, len);
      if (tracer.interested_in_range(paddr, paddr + PGSIZE, LOAD))
        tracer.trace(paddr, len, LOAD); //TODO should tracer trace any unauthorized loads?
      else
        refill_tlb(addr, paddr, host_addr, LOAD, id);
    } else {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: Error! Denying load access to enclave 0x%0lx, virtual address 0x%lx, physical address 0x%lx, number of pages %lu, page size 0x%lx\n", id, addr, paddr, num_of_pages, PGSIZE);
#endif
      throw trap_load_access_fault(addr);
    }
  } else if (!sim->mmio_load(paddr, len, bytes)) {
    throw trap_load_access_fault(addr);
  }

  if (!matched_trigger) {
    reg_t data = reg_from_bytes(len, bytes);
    matched_trigger = trigger_exception(OPERATION_LOAD, addr, data);
    if (matched_trigger)
      throw *matched_trigger;
  }
}

void mmu_t::store_slow_path(reg_t addr, reg_t len, const uint8_t* bytes, enclave_id_t id)
{
  reg_t paddr = translate(addr, STORE);
  if (!matched_trigger) {
    reg_t data = reg_from_bytes(len, bytes);
    matched_trigger = trigger_exception(OPERATION_STORE, addr, data);
    if (matched_trigger)
      throw *matched_trigger;
  }

  if (auto host_addr = sim->addr_to_mem(paddr)) {
    if(check_identifier(paddr, id, false)) {
      memcpy(host_addr, bytes, len);
      if (tracer.interested_in_range(paddr, paddr + PGSIZE, STORE))
        tracer.trace(paddr, len, STORE); //TODO should tracer know about an unauthorized store?
      else
        refill_tlb(addr, paddr, host_addr, STORE, id);
    } else {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: Error! Denying store access to enclave 0x%0lx, virtual address 0x%0lx, physical address 0x%0lx, number of pages %lu, page size 0x%0lx\n", id, addr, paddr, num_of_pages, PGSIZE);
#endif
      throw trap_store_access_fault(addr);
    }
  } else if (!sim->mmio_store(paddr, len, bytes)) {
#ifdef PRAESIDIO_DEBUG
    fprintf(stderr, "mmu.cc: Throwing store access fault in store_slow_path.\n");
#endif
    throw trap_store_access_fault(addr);
  }
}

tlb_entry_t mmu_t::refill_tlb(reg_t vaddr, reg_t paddr, char* host_addr, access_type type, enclave_id_t id)
{
  reg_t idx = (vaddr >> PGSHIFT) % TLB_ENTRIES;
  reg_t expected_tag = vaddr >> PGSHIFT;

  if ((tlb_load_tag[idx] & ~TLB_CHECK_TRIGGERS) != expected_tag)
    tlb_load_tag[idx] = -1;
  if ((tlb_store_tag[idx] & ~TLB_CHECK_TRIGGERS) != expected_tag)
    tlb_store_tag[idx] = -1;
  if ((tlb_insn_tag[idx] & ~TLB_CHECK_TRIGGERS) != expected_tag)
    tlb_insn_tag[idx] = -1;

  if ((check_triggers_fetch && type == FETCH) ||
      (check_triggers_load && type == LOAD) ||
      (check_triggers_store && type == STORE))
    expected_tag |= TLB_CHECK_TRIGGERS;

  if (type == FETCH) tlb_insn_tag[idx] = expected_tag;
  else if (type == STORE) tlb_store_tag[idx] = expected_tag;
  else tlb_load_tag[idx] = expected_tag;

  tlb_entry_t entry = {host_addr - vaddr, paddr - vaddr, id};
  tlb_data[idx] = entry;
  return entry;
}

reg_t mmu_t::walk(reg_t addr, access_type type, reg_t mode)
{
  vm_info vm = decode_vm_info(proc->max_xlen, mode, proc->get_state()->satp);
  if (vm.levels == 0)
    return addr & ((reg_t(2) << (proc->xlen-1))-1); // zero-extend from xlen

  bool s_mode = mode == PRV_S;
  bool sum = get_field(proc->state.mstatus, MSTATUS_SUM);
  bool mxr = get_field(proc->state.mstatus, MSTATUS_MXR);

  // verify bits xlen-1:va_bits-1 are all equal
  int va_bits = PGSHIFT + vm.levels * vm.idxbits;
  reg_t mask = (reg_t(1) << (proc->xlen - (va_bits-1))) - 1;
  reg_t masked_msbs = (addr >> (va_bits-1)) & mask;
  if (masked_msbs != 0 && masked_msbs != mask) {
    vm.levels = 0;
  }

  reg_t base = vm.ptbase;
  for (int i = vm.levels - 1; i >= 0; i--) {
    int ptshift = i * vm.idxbits;
    reg_t idx = (addr >> (PGSHIFT + ptshift)) & ((1 << vm.idxbits) - 1);

    // check that physical address of PTE is legal
    auto ppte = sim->addr_to_mem(base + idx * vm.ptesize);
    if (!ppte)
      goto fail_access;

    reg_t pte = vm.ptesize == 4 ? *(uint32_t*)ppte : *(uint64_t*)ppte;
    reg_t ppn = pte >> PTE_PPN_SHIFT;

    if (PTE_TABLE(pte)) { // next level of page table
      base = ppn << PGSHIFT;
    } else if ((pte & PTE_U) ? s_mode && (type == FETCH || !sum) : !s_mode) {
      break;
    } else if (!(pte & PTE_V) || (!(pte & PTE_R) && (pte & PTE_W))) {
      fprintf(stderr, "mmu.cc: TRAP not valid or (write and not read)\n");
      fprintf(stderr, "walk(addr 0x%016lx, type %u, mode %lu)\n", addr, type, mode);
      fprintf(stderr, "mmu.cc: level %d, pte 0x%016lx ppn 0x%016lx, idx 0x%016lx\n", i, pte, ppn, idx);
      fprintf(stderr, "ptbase 0x%016lx content: \n", vm.ptbase);
      for(int i = 0; i < 1024; i++) {
        fprintf(stderr, "%016lx ", ((uint64_t*) (sim->addr_to_mem(vm.ptbase)))[i]);
      }
      fprintf(stderr, "\n");
      exit(-1); //TODO remove
      break;
    } else if (type == FETCH ? !(pte & PTE_X) :
               type == LOAD ?  !(pte & PTE_R) && !(mxr && (pte & PTE_X)) :
                               !((pte & PTE_R) && (pte & PTE_W))) {
      break;
    } else if ((ppn & ((reg_t(1) << ptshift) - 1)) != 0) {
      break;
    } else {
      reg_t ad = PTE_A | ((type == STORE) * PTE_D);
#ifdef RISCV_ENABLE_DIRTY
      // set accessed and possibly dirty bits.
      *(uint32_t*)ppte |= ad;
#else
      // take exception if access or possibly dirty bit is not set.
      if ((pte & ad) != ad)
        break;
#endif
      // for superpage mappings, make a fake leaf PTE for the TLB's benefit.
      reg_t vpn = addr >> PGSHIFT;
      reg_t value = (ppn | (vpn & ((reg_t(1) << ptshift) - 1))) << PGSHIFT;
      return value;
    }
  }

fail:
  switch (type) {
    case FETCH: throw trap_instruction_page_fault(addr);
    case LOAD: throw trap_load_page_fault(addr);
    case STORE: throw trap_store_page_fault(addr);
    default: abort();
  }

fail_access:
  switch (type) {
    case FETCH: throw trap_instruction_access_fault(addr);
    case LOAD: throw trap_load_access_fault(addr);
    case STORE:
      fprintf(stderr, "mmu.cc: throwing trap store access fault in page walk.\n");
      throw trap_store_access_fault(addr);
    default: abort();
  }
}

void mmu_t::register_memtracer(memtracer_t* t)
{
  flush_tlb();
  tracer.hook(t);
}

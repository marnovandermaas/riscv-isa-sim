// See LICENSE for license details.
// Copyright 2018-2020 Marno van der Maas

#include "mmu.h"
#include "simif.h"
#include "processor.h"
#include "debug.h"

mmu_t::mmu_t(simif_t* sim, processor_t* proc, struct page_tag_t *tag_directory, size_t num_of_pages)
 : sim(sim), proc(proc), tag_directory(tag_directory), num_of_pages(num_of_pages),
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
    if(
      ((paddr >= MAILBOX_BASE) && (paddr < MAILBOX_BASE + MAILBOX_SIZE)) ||
      ((paddr >= TAGDIRECTORY_BASE) && (paddr < TAGDIRECTORY_BASE + TAGDIRECTORY_SIZE))
    ) {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: Warning! cannot fetch instructions from mailbox or tag directory addresses.\n");
#endif
      throw trap_instruction_access_fault(vaddr);
    }
    if(check_identifier(paddr, enclave_id, true)) {
      return refill_tlb(vaddr, paddr, host_addr, FETCH);
    } else {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: Warning! Denying fetch to enclave 0x%08x, virtual address 0x%lx, physical address 0x%lx, number of pages %lu, page size 0x%lx\n", enclave_id, vaddr, (uint64_t) host_addr, num_of_pages, PGSIZE);
#endif
      throw trap_instruction_access_fault(vaddr);
    }
  } else {
    if (!sim->mmio_load(paddr, sizeof fetch_temp, (uint8_t*)&fetch_temp)) {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: Warning! Failed MMIO load during fetch by enclave 0x%08x, virtual address 0x%lx, physical address 0x%lx\n", enclave_id, vaddr, (uint64_t) host_addr);
#endif
      throw trap_instruction_access_fault(vaddr);
    }
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

bool mmu_t::check_identifier(reg_t paddr, enclave_id_t id, bool load, enclave_id_t* writer_id) {
#ifdef PRAESIDIO_DEBUG
  if(tag_directory == NULL) {
    fprintf(stderr, "mmu.cc: quiting because tag directory is null\n");
    exit(-11);
  }
#endif
  if(paddr >= DRAM_BASE && paddr < DRAM_BASE + PGSIZE*num_of_pages) {
    reg_t dram_offset = paddr - DRAM_BASE;
    reg_t page_num = dram_offset / PGSIZE;
    if(load && id == tag_directory[page_num].reader) {
      if(writer_id == NULL) {
        printf("Checking identifier and wanting to set writer_id return value to true, but reader pointer is NULL.\n");
        exit(-5);
      }
      *writer_id = tag_directory[page_num].owner;
      return true;
    }
    return id == tag_directory[page_num].owner;
  }
  return true;
}

void mmu_t::load_slow_path(reg_t addr, reg_t len, uint8_t* bytes, enclave_id_t enclave_id)
{
  enclave_id_t writer_id = ENCLAVE_INVALID_ID;
  reg_t paddr = translate(addr, LOAD);
  if (auto host_addr = sim->addr_to_mem(paddr)) {
    if((paddr >= TAGDIRECTORY_BASE) && (paddr < TAGDIRECTORY_BASE + TAGDIRECTORY_SIZE)) {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: loading from the tag directory is not allowed.\n");
#endif
      throw trap_load_access_fault(addr);
    }
    if(check_identifier(paddr, enclave_id, true, &writer_id)) {
      memcpy(bytes, host_addr, len);
      if((paddr >= MAILBOX_BASE) && (paddr < MAILBOX_BASE + MAILBOX_SIZE)) {
        if(((paddr - MAILBOX_BASE) % (sizeof(struct Message_t))) == 0) { //We assume that type is the first element of the mailbox. We will invalidate the message if the correct enclave is reading it.
          struct Message_t *mailbox = (struct Message_t *) sim->addr_to_mem(paddr);
          if(mailbox->type != MSG_INVALID && mailbox->destination == enclave_id) {
#ifdef PRAESIDIO_DEBUG
            fprintf(stderr, "mmu.cc: Invalidating message for enclave 0x%x and address %016lx with type 0x%x, source 0x%x, dest 0x%x\n", enclave_id, paddr, mailbox->type, mailbox->source, mailbox->destination);
#endif
            mailbox->type = MSG_INVALID;
          }
// #ifdef PRAESIDIO_DEBUG
//           else {
//             fprintf(stderr, "mmu.cc: Message read by 0x%x at address %016lx has type 0x%x and destination 0x%x\n", enclave_id, paddr, mailbox->type, mailbox->destination);
//           }
// #endif
        }
// #ifdef PRAESIDIO_DEBUG
//         else {
//           fprintf(stderr, "mmu.cc: Reading mailbox address %016lx with enclave 0x%x\n", paddr, enclave_id);
//         }
// #endif
      }
      if (tracer.interested_in_range(paddr, paddr + PGSIZE, LOAD)) {
        trace_result resultOfTrace = tracer.trace(paddr, len, LOAD);
        if(resultOfTrace == LLC_MISS) {
#ifdef COVERT_CHANNEL_POC
            proc->set_csr(CSR_LLCMISSCOUNT, 1);
#endif //COVERT_CHANNEL_POC
        }
        if(resultOfTrace == NO_LLC_INTERACTION && writer_id != ENCLAVE_INVALID_ID) {
            proc->sim->process_enclave_read_access(paddr, writer_id, enclave_id);
        }
      } else {
        refill_tlb(addr, paddr, host_addr, LOAD);
      }
    } else {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: Warning! Denying load access to enclave 0x%08x, virtual address 0x%016lx, physical address 0x%016lx, number of pages %lu, page size 0x%lx\n", enclave_id, addr, paddr, num_of_pages, PGSIZE);
#endif
      throw trap_load_access_fault(addr);
    }
  } else if (!sim->mmio_load(paddr, len, bytes)) {
#ifdef PRAESIDIO_DEBUG
    fprintf(stderr, "mmu.cc: throwing load access fault for address 0x%016lx\n", addr);
#endif
    throw trap_load_access_fault(addr);
  }

  if (!matched_trigger) {
    reg_t data = reg_from_bytes(len, bytes);
    matched_trigger = trigger_exception(OPERATION_LOAD, addr, data);
    if (matched_trigger)
      throw *matched_trigger;
  }
}

void mmu_t::store_slow_path(reg_t addr, reg_t len, const uint8_t* bytes, enclave_id_t enclave_id)
{
  reg_t paddr = translate(addr, STORE);
  if (!matched_trigger) {
    reg_t data = reg_from_bytes(len, bytes);
    matched_trigger = trigger_exception(OPERATION_STORE, addr, data);
    if (matched_trigger)
      throw *matched_trigger;
  }
  if((paddr >= MAILBOX_BASE) && (paddr < MAILBOX_BASE + MAILBOX_SIZE)) {
    if((paddr - (reg_t) MAILBOX_BASE) > sizeof(struct Message_t)) {
      fprintf(stderr, "mmu.cc: writing out of mailbox bounds.\n");
      throw trap_store_access_fault(addr);
    }
    paddr += (sizeof(struct Message_t)) * (proc->id);
#ifdef PRAESIDIO_DEBUG
    fprintf(stderr, "mmu.cc: enclave 0x%x writing to mailbox address 0x%016lx\n", enclave_id, paddr);
    // fprintf(stderr, "mmu.cc: Content of mailboxes:");
    // for(unsigned int i = 0; i < 4*sizeof(struct Message_t)/4; i++) {
    //   if(i%8 == 0) {
    //     fprintf(stderr, "\nmmu.cc: ");
    //   }
    //   fprintf(stderr, "%08x ", ((int *) sim->addr_to_mem(MAILBOX_BASE))[i]);
    // }
    // fprintf(stderr, "\n");
#endif
  }

  if (auto host_addr = sim->addr_to_mem(paddr)) {
    if(check_identifier(paddr, enclave_id, false)) {
      if((paddr >= TAGDIRECTORY_BASE) && (paddr < TAGDIRECTORY_BASE + TAGDIRECTORY_SIZE)) {
        if(enclave_id != ENCLAVE_MANAGEMENT_ID) {
          size_t id_offset = (paddr - TAGDIRECTORY_BASE) / sizeof(enclave_id_t);
          if((paddr - TAGDIRECTORY_BASE) % sizeof(enclave_id_t)) {
#ifdef PRAESIDIO_DEBUG
            fprintf(stderr, "mmu.cc: trying to do a misaligned store to tag directory while not management shim.\n");
#endif
            throw trap_store_access_fault(addr);
          }
          if((id_offset % 2) == 0) {
            //Only the management shim is allowed to write to owner identifiers
#ifdef PRAESIDIO_DEBUG
            fprintf(stderr, "mmu.cc: trying to store to owner in tag directory while not management shim.\n");
#endif
            throw trap_store_access_fault(addr);
          } else {
            if(len != sizeof(enclave_id_t)) {
#ifdef PRAESIDIO_DEBUG
              fprintf(stderr, "mmu.cc: trying to write more or less than 32-bits in reader field of tag directory.\n");
#endif
              throw trap_store_access_fault(addr);
            }
            size_t entry_offset = (paddr - TAGDIRECTORY_BASE) / sizeof(struct page_tag_t);
            if(tag_directory[entry_offset].owner != enclave_id) {
#ifdef PRAESIDIO_DEBUG
              fprintf(stderr, "mmu.cc: trying to set reader in tag directory while not being the owner.\n");
#endif
              throw trap_store_access_fault(addr);
            }
          }
        }
      }
      memcpy(host_addr, bytes, len);
      if((paddr >= MAILBOX_BASE) && (paddr < MAILBOX_BASE + MAILBOX_SIZE)) {
        struct Message_t *mailbox = (struct Message_t *) sim->addr_to_mem(MAILBOX_BASE + (sizeof(struct Message_t)) * (proc->id));
        mailbox->source = enclave_id; //Make sure the source is always the correct enclave identifier.
#ifdef PRAESIDIO_DEBUG
        fprintf(stderr, "mmu.cc: setting the source to 0x%x of mailbox 0x%016lx\n", enclave_id, paddr);
#endif
      }
      if (tracer.interested_in_range(paddr, paddr + PGSIZE, STORE))
        tracer.trace(paddr, len, STORE); //TODO should tracer know about an unauthorized store?
      else
        refill_tlb(addr, paddr, host_addr, STORE);
    } else {
#ifdef PRAESIDIO_DEBUG
      fprintf(stderr, "mmu.cc: Warning! Denying store access to enclave 0x%08x, virtual address 0x%016lx, physical address 0x%016lx, number of pages %lu, page size 0x%0lx\n", enclave_id, addr, paddr, num_of_pages, PGSIZE);
#endif
      throw trap_store_access_fault(addr);
    }
  } else if (!sim->mmio_store(paddr, len, bytes)) {
    throw trap_store_access_fault(addr);
  }
}

tlb_entry_t mmu_t::refill_tlb(reg_t vaddr, reg_t paddr, char* host_addr, access_type type)
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

  enclave_id_t owner = ENCLAVE_INVALID_ID;
  enclave_id_t reader = ENCLAVE_INVALID_ID;
  if(paddr >= DRAM_BASE && paddr < DRAM_BASE + PGSIZE*num_of_pages) {
    reg_t dram_offset = paddr - DRAM_BASE;
    reg_t page_num = dram_offset / PGSIZE;
    owner = tag_directory[page_num].owner;
    reader = tag_directory[page_num].reader;
  }
  tlb_entry_t entry = {host_addr - vaddr, paddr - vaddr, owner, reader};
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
// #ifdef PRAESIDIO_DEBUG
//       if(s_mode && type == FETCH) {
//         fprintf(stderr, "mmu.cc: U ");
//       }
// #endif
      break;
    } else if (!(pte & PTE_V) || (!(pte & PTE_R) && (pte & PTE_W))) {
// #ifdef PRAESIDIO_DEBUG
//       if(s_mode && type == FETCH) {
//         fprintf(stderr, "mmu.cc: VRW ");
//       }
// #endif
      break;
    } else if (type == FETCH ? !(pte & PTE_X) :
               type == LOAD ?  !(pte & PTE_R) && !(mxr && (pte & PTE_X)) :
                               !((pte & PTE_R) && (pte & PTE_W))) {
// #ifdef PRAESIDIO_DEBUG
//       if(s_mode && type == FETCH) {
//         fprintf(stderr, "mmu.cc: XRW ");
//       }
// #endif
      break;
    } else if ((ppn & ((reg_t(1) << ptshift) - 1)) != 0) {
// #ifdef PRAESIDIO_DEBUG
//       if(s_mode && type == FETCH) {
//         fprintf(stderr, "mmu.cc: ppn ");
//       }
// #endif
      break;
    } else {
      reg_t ad = PTE_A | ((type == STORE) * PTE_D);
#ifdef RISCV_ENABLE_DIRTY
      // set accessed and possibly dirty bits.
      *(uint32_t*)ppte |= ad;
#else
      // take exception if access or possibly dirty bit is not set.
      if ((pte & ad) != ad)
      {
// #ifdef PRAESIDIO_DEBUG
//         if(s_mode) {
//           fprintf(stderr, "mmu.cc: A ");
//         }
// #endif
        break;
      }
#endif
      // for superpage mappings, make a fake leaf PTE for the TLB's benefit.
      reg_t vpn = addr >> PGSHIFT;
      reg_t value = (ppn | (vpn & ((reg_t(1) << ptshift) - 1))) << PGSHIFT;
      return value;
    }
  }

fail:
  switch (type) {
    case FETCH:
// #ifdef PRAESIDIO_DEBUG
//       if(s_mode) {
//         fprintf(stderr, " instruction page fault addr 0x%016lx, mode %d, vm level %d, vm idxbits %lu, vm ptbase 0x%016lx, masked_msbs 0x%016lx, exited at iteration %d, ptshift %d, idx 0x%016lx, pte 0x%016lx, ppn 0x%016lx\n", addr, mode, vm.levels, vm.idxbits, vm.ptbase, masked_msbs, i, ptshift, idx, pte, ppn);
//       }
// #endif
      throw trap_instruction_page_fault(addr);
    case LOAD: throw trap_load_page_fault(addr);
    case STORE: throw trap_store_page_fault(addr);
    default: abort();
  }

fail_access:
#ifdef PRAESIDIO_DEBUG
  fprintf(stderr, "mmu.cc: Failed to access the page table in page walk.\n");
#endif
  switch (type) {
    case FETCH: throw trap_instruction_access_fault(addr);
    case LOAD: throw trap_load_access_fault(addr);
    case STORE:
      throw trap_store_access_fault(addr);
    default: abort();
  }
}

void mmu_t::register_memtracer(memtracer_t* t)
{
  flush_tlb();
  tracer.hook(t);
}

#include "devices.h"

void bus_t::add_device(reg_t addr, abstract_device_t* dev)
{
  // Searching devices via lower_bound/upper_bound
  // implicitly relies on the underlying std::map
  // container to sort the keys and provide ordered
  // iteration over this sort, which it does. (python's
  // SortedDict is a good analogy)
  //fprintf(stderr, "devices.cc: adding device with host address 0x%0lx with base 0x%0lx\n", dev, addr);
  devices[addr] = dev;
}

bool bus_t::load(reg_t addr, size_t len, uint8_t* bytes)
{
  // Find the device with the base address closest to but
  // less than addr (price-is-right search)
  auto it = devices.upper_bound(addr);
  if (devices.empty() || it == devices.begin()) {
    // Either the bus is empty, or there weren't
    // any items with a base address <= addr
    return false;
  }
  // Found at least one item with base address <= addr
  // The iterator points to the device after this, so
  // go back by one item.
  it--;
  return it->second->load(addr - it->first, len, bytes);
}

bool bus_t::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  // See comments in bus_t::load
  auto it = devices.upper_bound(addr);
  if (devices.empty() || it == devices.begin()) {
    return false;
  }
  it--;
  return it->second->store(addr - it->first, len, bytes);
}

std::pair<reg_t, abstract_device_t*> bus_t::find_device(reg_t addr)
{
  // See comments in bus_t::load
  //fprintf(stderr, "devices.cc: addr 0x%0lx, num devs: %ld\n", addr, devices.size());
  abstract_device_t* tmpDevice = NULL;
  reg_t tmpAddress = 0;
  if (devices.empty()) {
    fprintf(stderr, "devices.cc: Warning! devices list is empty.\n");
    return std::make_pair((reg_t) 0, (abstract_device_t *)NULL);
  }
  for(std::map<reg_t,abstract_device_t*>::iterator it = devices.begin(); it != devices.end(); ++it) {
    //fprintf(stderr, "devices.cc: 0x%0lx - 0x%0lx\n", tmpAddress, it->first);
    if(addr >= tmpAddress && addr < it->first) {
      //fprintf(stderr, "devices.cc: address between 0x%0lx and 0x%0lx and host address 0x%0lx\n", tmpAddress, it->first, tmpDevice);
      return std::make_pair(tmpAddress, tmpDevice);
    }
    tmpAddress = it->first;
    tmpDevice = it->second;
  }
  //fprintf(stderr, "devices.cc: on last device with base address 0x%0lx host address 0x%0lx\n", tmpAddress, tmpDevice);
  return std::make_pair(tmpAddress, tmpDevice);
}

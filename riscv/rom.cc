#include "devices.h"

rom_device_t::rom_device_t(std::vector<char> vec_data)
{
  len = vec_data.size();
  data = (char *) calloc(1, len);
  if (!data) {
    throw std::runtime_error("rom.cc: couldn't allocate " + std::to_string(len) + " bytes of target memory");
  }
  //fprintf(stderr, "rom.cc: creating rom at host address 0x%0lx\n", data);
  for(size_t i = 0; i < len; i++) {
    data[i] = vec_data[i];
  }
}

bool rom_device_t::load(reg_t addr, size_t req_len, uint8_t* bytes)
{
  if (addr + req_len > len)
    return false;
  memcpy(bytes, &data[addr], req_len);
  return true;
}

bool rom_device_t::store(reg_t addr, size_t req_len, const uint8_t* bytes)
{
  return false;
}

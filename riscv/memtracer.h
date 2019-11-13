// See LICENSE for license details.

#ifndef _MEMTRACER_H
#define _MEMTRACER_H

#include <cstdint>
#include <string.h>
#include <vector>

enum access_type {
  LOAD,
  STORE,
  FETCH,
};

enum trace_result {
  NO_LLC_INTERACTION,
  LLC_HIT,
  LLC_MISS,
};

class memtracer_t
{
 public:
  memtracer_t() {}
  virtual ~memtracer_t() {}

  virtual bool interested_in_range(uint64_t begin, uint64_t end, access_type type) = 0;
  virtual trace_result trace(uint64_t addr, size_t bytes, access_type type) = 0;
};

class memtracer_list_t : public memtracer_t
{
 public:
  bool empty() { return list.empty(); }
  bool interested_in_range(uint64_t begin, uint64_t end, access_type type)
  {
    for (std::vector<memtracer_t*>::iterator it = list.begin(); it != list.end(); ++it)
      if ((*it)->interested_in_range(begin, end, type))
        return true;
    return false;
  }
  trace_result trace(uint64_t addr, size_t bytes, access_type type)
  {
    trace_result return_value = NO_LLC_INTERACTION;
    trace_result temp_value;
    for (std::vector<memtracer_t*>::iterator it = list.begin(); it != list.end(); ++it) {
      temp_value = (*it)->trace(addr, bytes, type);
      if(temp_value != NO_LLC_INTERACTION) {
        return_value = temp_value;
      }
    }
    return return_value;
  }
  void hook(memtracer_t* h)
  {
    list.push_back(h);
  }
 private:
  std::vector<memtracer_t*> list;
};

#endif

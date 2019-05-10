// See LICENSE for license details.

#include "trap.h"
#include "processor.h"
#include <cstdio>

const char* trap_t::name()
{
  if(uint8_t(which) == which) {
    sprintf(_name, "trap #%u", uint8_t(which));
  } else {
    sprintf(_name, "interrupt #%u", uint8_t(which));
  }
  return _name;
}

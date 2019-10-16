#ifndef _RISCV_DEVICES_H
#define _RISCV_DEVICES_H

#include "decode.h"
#include <cstdlib>
#include <string>
#include <map>
#include <vector>

class processor_t;

class abstract_device_t {
 public:
  virtual bool load(reg_t addr, size_t len, uint8_t* bytes) = 0;
  virtual bool store(reg_t addr, size_t len, const uint8_t* bytes) = 0;
  virtual ~abstract_device_t() {}
  char* contents() {
    fprintf(stderr, "devices.h: ERROR contents() exiting.\n");
    exit(-1);
  }
  size_t size() {
    fprintf(stderr, "devices.h: ERROR size() exiting.\n");
    exit(-1);
  }
};

class bus_t : public abstract_device_t {
 public:
  bool load(reg_t addr, size_t len, uint8_t* bytes);
  bool store(reg_t addr, size_t len, const uint8_t* bytes);
  void add_device(reg_t addr, abstract_device_t* dev);

  std::pair<reg_t, abstract_device_t*> find_device(reg_t addr);

 private:
  std::map<reg_t, abstract_device_t*> devices;
};

class abstract_mem_t : public abstract_device_t {
public:
  char* contents() { return data; }
  size_t size() { return len; }
protected:
  char* data;
  size_t len;
};

class rom_device_t : public abstract_mem_t {
 public:
  rom_device_t(std::vector<char> data);
  bool load(reg_t addr, size_t len, uint8_t* bytes);
  bool store(reg_t addr, size_t len, const uint8_t* bytes);
};

class mem_t : public abstract_mem_t {
 public:
  mem_t(size_t size) {
    len = size;
    if (!size) {
      throw std::runtime_error("zero bytes of target memory requested");
    }
    data = (char*)calloc(1, size);
    if (!data) {
      throw std::runtime_error("couldn't allocate " + std::to_string(size) + " bytes of target memory");
    }
  }
  //This constructor initializes data with the content of initial_data up to length.
  mem_t(size_t size, size_t length, char *initial_data) : mem_t(size) {
    if(length <= size) {
      for(size_t i = 0; i < length; i++) {
        data[i] = initial_data[i];
      }
    } else {
      fprintf(stderr, "devices.h: ERROR could not initialize memory: %lu, %lu\n", size, length);
      exit(-1);
    }
  }
  mem_t(const mem_t& that) = delete;
  ~mem_t() { free(data); }

  bool load(reg_t addr, size_t len, uint8_t* bytes) { return false; }
  bool store(reg_t addr, size_t len, const uint8_t* bytes) { return false; }
};

class clint_t : public abstract_device_t {
 public:
  clint_t(std::vector<processor_t*>&);
  bool load(reg_t addr, size_t len, uint8_t* bytes);
  bool store(reg_t addr, size_t len, const uint8_t* bytes);
  size_t size() { return CLINT_SIZE; }
  void increment(reg_t inc);
 private:
  typedef uint64_t mtime_t;
  typedef uint64_t mtimecmp_t;
  typedef uint32_t msip_t;
  std::vector<processor_t*>& procs;
  mtime_t mtime;
  std::vector<mtimecmp_t> mtimecmp;
};

#endif

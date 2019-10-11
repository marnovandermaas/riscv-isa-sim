#include "praesidio.h"

void normal_world() {
  //This is the code that runs in the normal world.

  enclave_id_t myEnclave = start_enclave_fast(5);
}

void enclave_world(int enclaveID) {
  return;
}

int main() {
  int id = getCurrentEnclaveID();

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else {
    enclave_world(id);
  }
}

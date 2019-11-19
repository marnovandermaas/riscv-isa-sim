#include "praesidio.h"

void normal_world() {
  //This is the code that runs in the normal world.

  enclave_id_t myEnclave = start_enclave();
  if(myEnclave == ENCLAVE_INVALID_ID) return;


  output_string("N\n");
}

void enclave_world() {
  //This is the code that runs in the enclave world.
  output_string("E\n");
}

int main() {
  int id = getCurrentEnclaveID();

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else if (id == 1) {
    enclave_world();
  } else {
    //Do nothing if you are any other enclave.
  }
}

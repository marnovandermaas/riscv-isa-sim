#include "praesidio.h"

#define NUM_OF_ENCLAVES 14 // This is the amount of enclave data that fit in one enclave data page. (126-33) //ascii val for ! up to ascii val for ~

void normal_world() {
  //This is the code that runs in the normal world.

  for(int i = 0; i < NUM_OF_ENCLAVES; i++) {
    enclave_id_t myEnclave = start_enclave();
    if(myEnclave == ENCLAVE_INVALID_ID) return;
  }
}

void enclave_world(int enclaveID) {
  output_char('!' + enclaveID);
  output_char('\n');
}

int main() {
  int id = getCurrentEnclaveID();

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else {
    enclave_world(id);
  }
}

#include "include/praesidio.h"

#define NUM_OF_ENCLAVES 14 // This is the amount of enclave data that fit in one enclave data page. (126-33) //ascii val for ! up to ascii val for ~

void normal_world() {
  //This is the code that runs in the normal world.

  char *enclaveMemory = (char *) DRAM_BASE;
  char *enclavePages[3];


  for(int i = 0; i < NUM_OF_ENCLAVES; i++) {
    enclaveMemory += 3*PAGE_SIZE;
    enclavePages[0] = enclaveMemory;
    enclavePages[1] = enclaveMemory + PAGE_SIZE;
    enclavePages[2] = enclaveMemory + 2*PAGE_SIZE;
    enclave_id_t myEnclave = start_enclave((char *) DRAM_BASE, 3, enclavePages);
    if(myEnclave == ENCLAVE_INVALID_ID) return;
  }
}

void enclave_world(int enclaveID) {
  output_char('!' + enclaveID);
}

int main() {
  int id = getCurrentEnclaveID();

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else {
    enclave_world(id);
  }
}

#include "praesidio.h"

#define NUM_OF_ENCLAVES 14 // This is the amount of enclave data that fit in one enclave data page. (126-33) //ascii val for ! up to ascii val for ~

#define NUMBER_OF_ENCLAVE_PAGES (5)

void normal_world() {
  //This is the code that runs in the normal world.

  char *enclaveMemory = (char *) DRAM_BASE;
  char *enclavePages[NUMBER_OF_ENCLAVE_PAGES];

  for(int i = 0; i < NUM_OF_ENCLAVES; i++) {
    enclaveMemory += NUMBER_OF_ENCLAVE_PAGES*PAGE_SIZE;
    for(int i = 0; i < NUMBER_OF_ENCLAVE_PAGES; i++) {
      enclavePages[i] = enclaveMemory;
      enclaveMemory += PAGE_SIZE;
    }
    enclave_id_t myEnclave = start_enclave((char *) DRAM_BASE, NUMBER_OF_ENCLAVE_PAGES, enclavePages);
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

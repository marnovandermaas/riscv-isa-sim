#include "praesidio.h"

#define INPUT_LEN (10)
#define OUTPUT_LEN (16)
#define REPEAT (4)

#define NUMBER_OF_ENCLAVE_PAGES (5)

void normal_world() {
  //This is the code that runs in the normal world.
  char name[INPUT_LEN] = "Latency";
  long page_num = 2;
  volatile char *address;
  char *input = (char *) PAGE_TO_POINTER(page_num);
  char read_buffer[OUTPUT_LEN];
  int offset = 0;

  char *enclaveMemory = (char *) DRAM_BASE + NUMBER_OF_ENCLAVE_PAGES*PAGE_SIZE;
  char *enclavePages[NUMBER_OF_ENCLAVE_PAGES];
  for(int i = 0; i < NUMBER_OF_ENCLAVE_PAGES; i++) {
    enclavePages[i] = enclaveMemory;
    enclaveMemory += PAGE_SIZE;
  }
  enclave_id_t myEnclave = start_enclave((char *) DRAM_BASE, NUMBER_OF_ENCLAVE_PAGES, enclavePages);
  if(myEnclave == ENCLAVE_INVALID_ID) return;

  for(int i = 0; i < REPEAT; i++) {
    give_read_permission(page_num, myEnclave);
    address = get_receive_mailbox_base_address(myEnclave);

    input += send_enclave_message(input, name, INPUT_LEN);
    offset += get_enclave_message(address+offset, read_buffer);
  }
  output_string("Got: ");
  output_string(read_buffer);
  output_char('\n');
}

void enclave_world() {
  //This is the code that runs in the enclave world.
  volatile char *address;
  long page_num = NUMBER_OF_ENCLAVE_PAGES+3;
  char *output = (char *) PAGE_TO_POINTER(page_num);
  char read_buffer[INPUT_LEN];
  int offset = 0;

  for(int i = 0; i < REPEAT; i++) {
    give_read_permission(page_num, ENCLAVE_DEFAULT_ID);
    address = get_receive_mailbox_base_address(ENCLAVE_DEFAULT_ID);

    offset += get_enclave_message(address+offset, read_buffer);
    output += send_enclave_message(output, read_buffer, OUTPUT_LEN);
  }
  output_string("Enclave: ");
  output_string(read_buffer);
  output_char('\n');
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

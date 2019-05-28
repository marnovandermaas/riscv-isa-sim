#include "include/praesidio.h"

#define INPUT_LEN (10)
#define OUTPUT_LEN (16)
#define NUMBER_OF_NAMES (10)

void normal_world() {
  //This is the code that runs in the normal world.
  char name[INPUT_LEN] = "Marno";
  long page_num = 2;
  volatile char *address;
  char *input = (char *) PAGE_TO_POINTER(page_num);
  char read_buffer[OUTPUT_LEN];

  char *enclaveMemory = (char *) DRAM_BASE + 3*PAGE_SIZE;
  char *enclavePages[3];
  enclavePages[0] = enclaveMemory;
  enclavePages[1] = enclaveMemory + PAGE_SIZE;
  enclavePages[2] = enclaveMemory + 2*PAGE_SIZE;
  enclave_id_t myEnclave = start_enclave((char *) DRAM_BASE, 3, enclavePages);
  if(myEnclave == ENCLAVE_INVALID_ID) return;

  give_read_permission(page_num, myEnclave);
  address = get_receive_mailbox_base_address(myEnclave);

  for (int i = 0; i < NUMBER_OF_NAMES; i++) {
    input += send_enclave_message(input, name, INPUT_LEN);
    address += get_enclave_message(address, read_buffer);
    output_string("Got: ");
    output_string(read_buffer);
    output_char('\n');
    name[0]+=1;
  }
}

void enclave_world() {
  //This is the code that runs in the enclave world.
  volatile char *address;
  long page_num = 5;
  char *output = (char *) PAGE_TO_POINTER(page_num);
  char read_buffer[INPUT_LEN+3];

  read_buffer[0] = 'H';
  read_buffer[1] = 'i';
  read_buffer[2] = ' ';

  give_read_permission(page_num, ENCLAVE_DEFAULT_ID);
  address = get_receive_mailbox_base_address(ENCLAVE_DEFAULT_ID);

  for (int i = 0; i < NUMBER_OF_NAMES; i++) {
    address += get_enclave_message(address, &read_buffer[3]);
    output_string(read_buffer);
    output += send_enclave_message(output, read_buffer, OUTPUT_LEN);
    output_char('\n');
  }
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

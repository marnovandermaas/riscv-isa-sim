#include "praesidio.h"

//This is the code that runs in the normal world.
void normal_world() {
  char character = 'A';

  //Starting enclave
  enclave_id_t myEnclave = start_enclave();
  if(myEnclave == ENCLAVE_INVALID_ID) return;

  //Setting up enclave related variables.
  volatile char *address;
  char *input = (char *) COMMUNICATION_PAGES_BASE;
  char read_buffer[1];
  give_read_permission(((int) input - DRAM_BASE) >> 12, myEnclave);
  address = get_receive_mailbox_base_address(myEnclave);

//  input += send_enclave_message(input, &character, 1);
//  address += get_enclave_message(address, read_buffer);

  output_string("Null Ecall: ");
  if(read_buffer[0] == character)  output_string("SUCCESS!\n");
  else                          output_string("FAIL!\n");
}

//This is the code that runs in the enclave world.
void enclave_world() {
  volatile char *address;
  char *output = (char *) COMMUNICATION_PAGES_BASE + NUMBER_OF_ENCLAVE_PAGES*PAGE_SIZE;
  char read_buffer[1];
  int length;

  give_read_permission(((int) output - DRAM_BASE) >> 12, ENCLAVE_DEFAULT_ID);
  address = get_receive_mailbox_base_address(ENCLAVE_DEFAULT_ID);

  return;
  address += get_enclave_message(address, read_buffer);
  output += send_enclave_message(output, read_buffer, 1);
}

int main() {
  int id = getCurrentEnclaveID();

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else {
    enclave_world();
  }
}

#include "hello.h"
#include <stdio.h>

int main() {
  //This is the code that runs in the normal world.
  char name[INPUT_LEN] = "Marno";
  volatile char *address;
  char *input = malloc(PAGE_SIZE);
  char read_buffer[OUTPUT_LEN];

  enclave_id_t myEnclave = start_enclave();
  if(myEnclave == ENCLAVE_INVALID_ID) return;

  NW_give_read_permission(input, myEnclave);
  address = NW_get_receive_mailbox_base_address(myEnclave);

  for (int i = 0; i < NUMBER_OF_NAMES; i++) {
    input += NW_send_enclave_message(input, name, INPUT_LEN);
    address += NW_get_enclave_message(address, read_buffer);
    printf("Got: %s\n", read_buffer);
    name[0]+=1;
  }
}

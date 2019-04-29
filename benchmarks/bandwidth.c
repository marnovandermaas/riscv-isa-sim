#include "include/praesidio.h"

#define INPUT_LEN (300)

void normal_world() {
  //This is the code that runs in the normal world.
  char name[INPUT_LEN];
  long page_num = 2;
  volatile char *address;
  char *input = PAGE_TO_POINTER(page_num);
  char read_buffer[INPUT_LEN];
  
  for (int i = 0; i < INPUT_LEN; i++) {
    name[i] = i+1;
  }
  name [INPUT_LEN - 1] = '\0';

  give_read_permission(page_num, 1);
  address = get_receive_mailbox_base_address(1);

  input += send_enclave_message(input, name, INPUT_LEN);
  address += get_enclave_message(address, read_buffer);
  
  output_string("Normal world done!\n");
}

void enclave_world() {
  //This is the code that runs in the enclave world.
  volatile char *address;
  long page_num = 5;
  char *output = PAGE_TO_POINTER(page_num);
  char read_buffer[INPUT_LEN];

  give_read_permission(page_num, 0);
  address = get_receive_mailbox_base_address(0);

  address += get_enclave_message(address, read_buffer);
  output += send_enclave_message(output, read_buffer, INPUT_LEN);
  
  output_string("Enclave done\n");
}

int main() {
  int id = get_enclave_id();

  if(id == 0) {
    normal_world();
  } else if (id == 1) {
    enclave_world();
  } else {
    //Do nothing if you are any other enclave.
  }
}

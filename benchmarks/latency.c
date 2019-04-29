#include "include/praesidio.h"

#define INPUT_LEN (10)
#define OUTPUT_LEN (16)
#define REPEAT (4)

void normal_world() {
  //This is the code that runs in the normal world.
  char name[INPUT_LEN] = "Latency";
  long page_num = 2;
  volatile char *address;
  char *input = PAGE_TO_POINTER(page_num);
  char read_buffer[OUTPUT_LEN];
  int offset = 0;

  for(int i = 0; i < REPEAT; i++) {
    give_read_permission(page_num, 1);
    address = get_receive_mailbox_base_address(1);

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
  long page_num = 5;
  char *output = PAGE_TO_POINTER(page_num);
  char read_buffer[INPUT_LEN];
  int offset = 0;

  for(int i = 0; i < REPEAT; i++) {
    give_read_permission(page_num, 0);
    address = get_receive_mailbox_base_address(0);

    offset += get_enclave_message(address+offset, read_buffer);
    output += send_enclave_message(output, read_buffer, OUTPUT_LEN);
  }
  output_string("Enclave: ");
  output_string(read_buffer);
  output_char('\n');
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

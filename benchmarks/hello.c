#include "include/praesidio.h"

#define INPUT_LEN (10)
#define OUTPUT_LEN (16)
#define NUMBER_OF_NAMES (10)

#define MY_ENCLAVE_ID (2)

void normal_world() {
  //This is the code that runs in the normal world.
  char name[INPUT_LEN] = "Marno";
  long page_num = 2;
  volatile char *address;
  char *input = PAGE_TO_POINTER(page_num);
  char read_buffer[OUTPUT_LEN];

  struct Message_t message;
  message.source = ENCLAVE_DEFAULT_ID;
  message.destination = ENCLAVE_MANAGEMENT_ID;
  message.type = MSG_CREATE_ENCLAVE;
  message.content = 0;
  sendMessage(&message);
  give_read_permission(page_num, MY_ENCLAVE_ID);
  address = get_receive_mailbox_base_address(MY_ENCLAVE_ID);

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
  char *output = PAGE_TO_POINTER(page_num);
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
  int id = get_enclave_id();

  char outString[16] = "Core  \n"; //This is the output string we will use.
  char idChar = id + '0'; //We add '0' to the id to convert the core ID to an ASCII code.
  outString[5] = idChar; //Set the second space to the idChar
  output_string(outString); //Output the string.

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else if (id == MY_ENCLAVE_ID) {
    enclave_world();
  } else {
    //Do nothing if you are any other enclave.
  }
}

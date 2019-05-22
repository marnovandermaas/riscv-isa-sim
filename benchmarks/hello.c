#include "include/praesidio.h"
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "stdio.h"

#define INPUT_LEN (10)
#define OUTPUT_LEN (16)
#define NUMBER_OF_NAMES (10)

void normal_world() {
  //This is the code that runs in the normal world.
  char name[INPUT_LEN] = "Marno";
  long page_num = 2;
  volatile char *address;
  char *input = PAGE_TO_POINTER(page_num);
  char read_buffer[OUTPUT_LEN];

  struct Message_t message;
  struct Message_t response;
  message.source = ENCLAVE_DEFAULT_ID;
  message.destination = ENCLAVE_MANAGEMENT_ID;
  message.type = MSG_CREATE_ENCLAVE;
  message.content = 0;
  sendMessage(&message);
  char *enclaveMemory = (char *) DRAM_BASE + 3*PAGE_SIZE;
  for(int i = 0; i < PAGE_SIZE; i++) {
    enclaveMemory[i] = ((char *) DRAM_BASE)[i];
  }
  do {
    receiveMessage(&response);
  } while(response.source != ENCLAVE_MANAGEMENT_ID);
  enclave_id_t myEnclave = response.content;
  message.type = MSG_SET_ARGUMENT;
  message.content = myEnclave;
  sendMessage(&message);
  do {
    receiveMessage(&response);
  } while(response.source != ENCLAVE_MANAGEMENT_ID);
  message.type = MSG_DONATE_PAGE;
  message.content = enclaveMemory; //This is the page for enclave memory
  sendMessage(&message);
  do {
    receiveMessage(&response);
  } while(response.source != ENCLAVE_MANAGEMENT_ID);
  message.type = MSG_DONATE_PAGE;
  message.content = enclaveMemory + PAGE_SIZE; //This is the page for enclave stack
  sendMessage(&message);
  do {
    receiveMessage(&response);
  } while(response.source != ENCLAVE_MANAGEMENT_ID);
  message.type = MSG_DONATE_PAGE;
  message.content = enclaveMemory + 2*PAGE_SIZE; //This is the page for sharing with the normal world
  sendMessage(&message);
  do {
    receiveMessage(&response);
  } while(response.source != ENCLAVE_MANAGEMENT_ID);
  message.type = MSG_SWITCH_ENCLAVE;
  message.content = myEnclave;
  sendMessage(&message);

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
  int id = getCurrentEnclaveID();

  char outString[16] = "Encl  \n"; //This is the output string we will use.
  char idChar = id + '0'; //We add '0' to the id to convert the core ID to an ASCII code.
  outString[5] = idChar; //Set the second space to the idChar
  output_string(outString); //Output the string.


   const char src[50] = "this is from src!";
   char dest[50];
   //strncpy(dest,src, strlen(src));
   //output_string(dest);
   //output_string("Before memcpy dest = %s\n", dest);
   memcpy(dest, src, strlen(src)+1);
   printf("heyyyy %s and %d \n",dest, 1234);

    //output_string(dest);


   //return(0);
/* ZTODO: it breaks after that */
/*
  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else if (id == 1) {
    enclave_world();
  } else {
    //Do nothing if you are any other enclave.
  }
*/
  
}

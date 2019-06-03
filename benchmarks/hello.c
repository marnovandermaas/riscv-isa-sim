#include "include/praesidio.h"
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
//#include "praesidio.h"

#define INPUT_LEN (10)
#define OUTPUT_LEN (16)
#define NUMBER_OF_NAMES (10)

#define __stdin 1
#define __stdout 2 
#define __stderr 3

#define NUMBER_OF_ENCLAVE_PAGES (5)

void normal_world() {
  //This is the code that runs in the normal world.
  char name[INPUT_LEN] = "Marno";
  long page_num = 2;
  volatile char *address;
  char *input = (char *) PAGE_TO_POINTER(page_num);
  char read_buffer[OUTPUT_LEN];

  char *enclaveMemory = (char *) DRAM_BASE + NUMBER_OF_ENCLAVE_PAGES*PAGE_SIZE;
  char *enclavePages[NUMBER_OF_ENCLAVE_PAGES];
  for(int i = 0; i < NUMBER_OF_ENCLAVE_PAGES; i++) {
    enclavePages[i] = enclaveMemory;
    enclaveMemory += PAGE_SIZE;
  }
  enclave_id_t myEnclave = start_enclave((char *) DRAM_BASE, NUMBER_OF_ENCLAVE_PAGES, enclavePages);
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
  long page_num = 8;
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

   const char* src = "this is from src!\n";
   char dest[50];
  strncpy(dest,src, strlen(src)+1);
   printf(dest);
   printf("Before memcpy dest = %s\n", dest);
   memcpy(dest, src, strlen(src)+1);
   printf("heyyyy %s and %d \n",dest, 1234);



  // return(0);
/* ZTODO: it breaks after that */

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else if (id == 1) {
    enclave_world();
  } else {
    //Do nothing if you are any other enclave.
  }

  
}

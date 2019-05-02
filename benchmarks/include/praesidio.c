#include "praesidio.h"

#define READY_SIGNAL (0xAB)
#define BUSY_SIGNAL (0xBA) //Must be different from READY_SIGNAL

void set_arg_id(enclave_id_t arg_id) {
  asm volatile (
    "csrrw zero, 0x40D, %0"
    :
    : "r"(arg_id)
    :
  );
}

//Sets read access to a page to an enclave
void give_read_permission(int pageNumber, enclave_id_t receiver_id) {
  PAGE_TO_POINTER(pageNumber)[0] = BUSY_SIGNAL;
  set_arg_id(receiver_id);
  asm volatile (
    "csrrw zero, 0x40A, %0"
    :
    : "r"(pageNumber)
    :
  );
}

//Donates a page to another enclave
void donate_page(int pageNumber, enclave_id_t receiver_id) {
  set_arg_id(receiver_id);
  asm volatile (
    "csrrw zero, 0x40C, %0"
    :
    : "r"(pageNumber)
    :
  );
}

//Gets base address of mailbox page from which you can receive messages from the enclave specified in sender_id.
volatile char* get_receive_mailbox_base_address(enclave_id_t sender_id) {
  volatile char* retVal = 0;
  set_arg_id(sender_id);
  do {
    asm volatile (
      "csrrs %0, 0x40B, %0"
      : "=r"(retVal)
      :
      :
    );
  } while (retVal == 0);
  return retVal;
}

//Returns the starting index for the next message.
int send_enclave_message(char *mailbox_address, char *message, int length) {
  int offset = 0;

  mailbox_address[offset] = BUSY_SIGNAL;
  offset += 1;

  for(int i = 0; i < sizeof(length); i++) {
    mailbox_address[offset+i] = ((length >> (8*(sizeof(length)-1-i))) & 0xFF);
  }
  offset += sizeof(length);

  for(int i = 0; i < length; i++) {
    mailbox_address[offset+i] = message[i];
  }
  offset += length;

  mailbox_address[offset] = BUSY_SIGNAL;
  mailbox_address[0] = READY_SIGNAL;
  return offset;
}

//Returns the starting index for the next message
int get_enclave_message(volatile char *mailbox_address, char *read_buffer) {
  int offset = 0;

  while (mailbox_address[offset] != READY_SIGNAL);
  offset += 1;

  int length = 0;
  for (int i = 0; i < sizeof(length); i++) {
    length |= (((int) mailbox_address[offset+i]) << (8*(sizeof(length)-1-i)));
  }
  offset += sizeof(length);

  for(int i = 0; i < length; i++) {
    read_buffer[i] = mailbox_address[offset+i];
  }
  offset += length;

  return offset;
}

//This function takes as input the enclave identifier that should be called, a string that will be sent to the enclave and also has a pointer to where it should write the output.
void run_enclave(int enclave_id, char *input, char *output) {
  /*char print_string[16] = "id   in: ";
  print_string[3] = enclave_id_t + '0';
  output_string(print_string);
  output_string(input);
  output_char('\n');*/
}

//Writes a character to display.
void output_char(char c) {
  asm volatile ( //This instruction writes a value to a CSR register. This is a custom register and I have modified Spike to print whatever character is written to this CSR. This is my way of printing without requiring the support of a kernel or the RISC-V front end server.
    "csrrw zero, 0x404, %0" //CSRRW rd, csr, rs1
    : //output operands
    : "r"(c) //input operands
    : //clobbered registers
  );
}

//Calls output_char in a loop.
void output_string(char *s) {
  int i = 0;
  while(s[i] != '\0') {
    output_char(s[i]);
    i++;
  }
}

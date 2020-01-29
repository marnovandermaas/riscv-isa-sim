#include "praesidio.h"
#include "praesidioenclave.h"

#define READY_SIGNAL (0xAB)
#define BUSY_SIGNAL (0xBA) //Must be different from READY_SIGNAL

//Sets read access to a page to an enclave
int give_read_permission(void* page_base, enclave_id_t receiver_id) {
  int page_number = (page_base | DRAM_BASE) >> 12;
  if (page_base < DRAM_BASE) { //Check if pagebase is in DRAM
    return -1;
  }
  if (((page_base >> 12) << 12) != page_base) { //Check if lower bits are zero
    return -2;
  }
  char *byte_base = (char *) page_base;
  byte_base[0] = BUSY_SIGNAL;
  setArgumentEnclaveIdentifier(receiver_id);
  asm volatile (
    "csrrw zero, 0x40A, %0"
    :
    : "r"(page_number)
    :
  );
  return 0;
}

//Gets base address of mailbox page from which you can receive messages from the enclave specified in sender_id.
volatile char* get_receive_mailbox_base_address(enclave_id_t sender_id) {
  volatile char* ret_val = 0;
  setArgumentEnclaveIdentifier(sender_id);
  do {
    asm volatile (
      "csrrs %0, 0x40B, zero"
      : "=r"(ret_val)
      :
      :
    );
  } while (ret_val == 0);
  return ret_val;
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

//Writes a character to display.
void output_char(char c) {
  asm volatile ( //This instruction writes a value to a CSR register. This is a custom register and I have modified Spike to print whatever character is written to this CSR. This is my way of printing without requiring the support of a kernel or the RISC-V front end server.
    "csrrw zero, 0x404, %0" //CSRRW rd, csr, rs1
    : //output operands
    : "r"(c) //input operands
    : //clobbered registers
  );
}

void output_hexbyte(unsigned char c) {
  unsigned char upper = (c >> 4);
  unsigned char lower = (c & 0xF);
  if(upper < 10) {
    output_char(upper + '0');
  } else {
    output_char(upper + 'A' - 10);
  }
  if(lower < 10) {
    output_char(lower + '0');
  } else {
    output_char(lower + 'A' - 10);
  }
}

//Calls output_char in a loop.
void output_string(char *s) {
  int i = 0;
  while(s[i] != '\0') {
    output_char(s[i]);
    i++;
  }
}

int enclave_number = 0;
enclave_id_t start_enclave_fast(unsigned int num_pages_to_copy) {
  enclave_number++;
  unsigned int number_of_enclave_pages = 0;
  if(num_pages_to_copy > NUMBER_OF_ENCLAVE_PAGES || num_pages_to_copy == 0) {
    number_of_enclave_pages = NUMBER_OF_ENCLAVE_PAGES;
  } else {
    number_of_enclave_pages = num_pages_to_copy;
  }
  char *enclaveMemory = (char *) DRAM_BASE + NUMBER_OF_ENCLAVE_PAGES*PAGE_SIZE*enclave_number;
  char *enclavePages[number_of_enclave_pages + NUMBER_OF_STACK_PAGES + NUMBER_OF_COMMUNICATION_PAGES];
  for(int i = 0; i < number_of_enclave_pages; i++) {
    enclavePages[i] = enclaveMemory;
    enclaveMemory += PAGE_SIZE;
  }
  enclaveMemory = (char *) STACK_PAGES_BASE + NUMBER_OF_ENCLAVE_PAGES*PAGE_SIZE*enclave_number;
  for(int i = 0; i < NUMBER_OF_STACK_PAGES; i++) {
    enclaveMemory -= PAGE_SIZE;
    enclavePages[i+number_of_enclave_pages] = enclaveMemory;
  }
  enclaveMemory = (char *) COMMUNICATION_PAGES_BASE + NUMBER_OF_ENCLAVE_PAGES*PAGE_SIZE*enclave_number;
  for(int i = 0; i < NUMBER_OF_COMMUNICATION_PAGES; i++) {
    enclavePages[i+number_of_enclave_pages+NUMBER_OF_STACK_PAGES] = enclaveMemory;
    enclaveMemory += PAGE_SIZE;
  }

  struct Message_t message;
  struct Message_t response;
  enclave_id_t currentEnclave = getCurrentEnclaveID();
  message.source = currentEnclave;
  message.destination = ENCLAVE_MANAGEMENT_ID;
  message.type = MSG_CREATE_ENCLAVE;
  message.content = 0;
  sendMessage(&message);
  do {
    receiveMessage(&response);
  } while(response.source != ENCLAVE_MANAGEMENT_ID);
  //Copy content to enclave pages.
  for(int i = 0; i < number_of_enclave_pages; i++) {
    for(int j = 0; j < PAGE_SIZE / 8; j++) { //Assuming long's are 8 Bytes to make for quicker copying
      ((long *)(enclavePages[i]))[j] = ((long *) DRAM_BASE)[j+i*(PAGE_SIZE/8)];
    }
  }

  enclave_id_t myEnclave = response.content;
  message.type = MSG_SET_ARGUMENT;
  message.content = myEnclave;
  sendMessage(&message);
  do {
    receiveMessage(&response);
  } while(response.source != ENCLAVE_MANAGEMENT_ID);
  for(unsigned int i = 0; i < number_of_enclave_pages + NUMBER_OF_STACK_PAGES + NUMBER_OF_COMMUNICATION_PAGES; i++) {
    message.type = MSG_DONATE_PAGE;
    message.content = (unsigned long) enclavePages[i]; //This is the page for enclave memory
    sendMessage(&message);
    do {
      receiveMessage(&response);
    } while(response.source != ENCLAVE_MANAGEMENT_ID);
  }
  message.type = MSG_SWITCH_ENCLAVE;
  message.content = myEnclave;
  sendMessage(&message);
  do {
    receiveMessage(&response);
  } while(response.source != ENCLAVE_MANAGEMENT_ID);
  return myEnclave;
}

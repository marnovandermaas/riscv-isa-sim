#ifndef PRAESIDIO_HEADER
#define PRAESIDIO_HEADER

#include "management.h"
#include "instructions.h"

// PGSIZE (1 << 12)
// DRAM_OFFSET (0x80000000)
#define PAGE_TO_POINTER(NUM) ((char *) ((NUM * (1 << 12)) | (0x80000000)))

typedef unsigned long enclave_id_t;

void set_arg_id(enclave_id_t arg_id);

void give_read_permission(int pageNumber, enclave_id_t receiver_id);

void donate_page(int pageNumber, enclave_id_t receiver_id);

volatile char* get_receive_mailbox_base_address(enclave_id_t sender_id);

//Sends message of length and encodes it at mailbox address. It returns the amount the mailbox_address should be increased by for the next message.
int send_enclave_message(char *mailbox_address, char *message, int length);

//Gets message from mailbox_address and puts it into the read buffer. It returns the amount the mailbox_address should be increased by for the next message.
int get_enclave_message(volatile char *mailbox_address, char *read_buffer);

void run_enclave(int enclave_id, char *input, char *output);

//Writes a character to display.
void output_char(char c);

//Writes a whole string to display
void output_string(char *s);

int get_enclave_id();

#endif //PRAESIDIO_HEADER

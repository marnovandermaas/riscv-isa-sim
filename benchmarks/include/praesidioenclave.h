#ifndef PRAESIDIO_ENCLAVE_HEADER
#define PRAESIDIO_ENCLAVE_HEADER

#include "praesidio.h"

void give_read_permission(void* address, enclave_id_t receiver_id);

volatile char* get_receive_mailbox_base_address(enclave_id_t sender_id);

//Sends message of length and encodes it at mailbox address. It returns the amount the mailbox_address should be increased by for the next message.
int send_enclave_message(char *mailbox_address, char *message, int length);

//Gets message from mailbox_address and puts it into the read buffer. It returns the amount the mailbox_address should be increased by for the next message.
int get_enclave_message(volatile char *mailbox_address, char *read_buffer);

//Writes a character to display.
void output_char(char c);

//Writes hex of a byte to display.
void output_hexbyte(unsigned char c);

//Writes a whole string to display
void output_string(char *s);

#endif //PRAESIDIO_ENCLAVE_HEADER

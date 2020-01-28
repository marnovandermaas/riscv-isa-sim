#ifndef PRAESIDIO_USER_HEADER
#define PRAESIDIO_USER_HEADER

#include "praesidio.h"

void NW_give_read_permission(void* address, enclave_id_t receiver_id);

volatile char* NW_get_receive_mailbox_base_address(enclave_id_t sender_id);

//Sends message of length and encodes it at mailbox address. It returns the amount the mailbox_address should be increased by for the next message.
int NW_send_enclave_message(char *mailbox_address, char *message, int length);

//Gets message from mailbox_address and puts it into the read buffer. It returns the amount the mailbox_address should be increased by for the next message.
int NW_get_enclave_message(volatile char *mailbox_address, char *read_buffer);

void donate_page(int pageNumber, enclave_id_t receiver_id);

enclave_id_t start_enclave();

enclave_id_t start_enclave_fast(unsigned int num_pages_to_copy);

#endif //PRAESIDIO_USER_HEADER

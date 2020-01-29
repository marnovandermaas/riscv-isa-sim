#ifndef PRAESIDIO_USER_HEADER
#define PRAESIDIO_USER_HEADER

#include "praesidio.h"

#define __NR_create_enclave       292
#define __NR_create_send_mailbox  293
#define __NR_get_receive_mailbox  294

char* NW_create_send_mailbox(enclave_id_t receiver_id);

volatile char* NW_get_receive_mailbox(enclave_id_t sender_id);

enclave_id_t start_enclave();

#endif //PRAESIDIO_USER_HEADER

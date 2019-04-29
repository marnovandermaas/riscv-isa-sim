#include "include/praesidio.h"

#define AES_BLOCK_SIZE (16)
#define ENCRYPT (0xAA)
#define DECRYPT (0x55)

#define MY_ENCLAVE_ID (2)

//Send byte with ENCRYPT/DECRYPT, then 128-bit key, then 128-bit IV, then 32-bit length, then length amount of bytes

//This is the code that runs in the normal world.
void normal_world() {
  //Setting up encryption related variables.
  int length = 3*AES_BLOCK_SIZE;
  int message_length = 1+2*AES_BLOCK_SIZE+sizeof(length)+length;
  char message[message_length];
  char content[length+1];
  content[length] = '\0';
  int offset = 0;
  //initializing content
  char tmp_char = 'A';
  for(int i = 0; i < length; i++, tmp_char++) {
    content[i] = tmp_char;
      if(tmp_char == 'Z') {
        tmp_char = '@';
      }
  }
  output_string("Content is:\n");
  output_string(content);
  output_char('\n');

  //Setting up enclave related variables.
  long page_num = 2;
  volatile char *address;
  char *input = PAGE_TO_POINTER(page_num);
  char read_buffer[length+1];
  read_buffer[length] = '\0';
  give_read_permission(page_num, MY_ENCLAVE_ID);
  address = get_receive_mailbox_base_address(MY_ENCLAVE_ID);

  //Preparing the encryption message.
  message[0] = ENCRYPT;
  offset += 1;
  for(int i = 0; i < AES_BLOCK_SIZE; i++) {
    message[i+offset] = (i%16); //Set key bytes
    message[i+offset+AES_BLOCK_SIZE] = i; //Set IV bytes
  }
  offset += 2*AES_BLOCK_SIZE;
  for(int i = 0; i < sizeof(length); i++) {
    message[i+offset] = ((length >> (8*(sizeof(length)-1-i))) & 0xFF);
  }
  offset += sizeof(length);
  for(int i = 0; i < length; i++) {
    message[i+offset] = content[i];
  }
  input += send_enclave_message(input, message, message_length);
  address += get_enclave_message(address, read_buffer);
  output_string("Got encryption:\n");
  output_string(read_buffer);
  output_char('\n');

  //Preparing the decryption message.
  message[0] = DECRYPT;
  for(int i = 0; i < length; i++) {
    message[i+offset] = read_buffer[i];
  }
  input += send_enclave_message(input, message, message_length);
  address += get_enclave_message(address, read_buffer);
  output_string("Got decryption:\n");
  output_string(read_buffer);
  output_char('\n');
}

int do_aes(char *output, char *input) {
  int length = 0;
  int offset = 1+2*AES_BLOCK_SIZE;
  for (int i = 0; i < sizeof(length); i++) {
    length |= (((int) input[i+offset]) << (8*(sizeof(length)-1-i)));
  }
  offset += sizeof(length);
  if(input[0] == ENCRYPT) {
    for (int i = 0; i < length; i++) {
      output[i] = input[1+(i%AES_BLOCK_SIZE)] ^ input[offset+i]; //TODO make this AES-counter enc
    }
  } else if (input[0] == DECRYPT) {
    for (int i = 0; i < length; i++) {
      output[i] = input[1+(i%AES_BLOCK_SIZE)] ^ input[offset+i]; //TODO make this AES-counter dec
    }
  } else {
    output_string("Error in enclave.\n");
    return 0;
  }
  return length;
}

//This is the code that runs in the enclave world.
void enclave_world() {

  volatile char *address;
  long page_num = 5;
  char *output = PAGE_TO_POINTER(page_num);
  char read_buffer[10*AES_BLOCK_SIZE];
  char write_buffer[10*AES_BLOCK_SIZE];
  int length;

  give_read_permission(page_num, 0); //TODO instead of hardcoding 0 import an "enclave.h" or similar to use.
  address = get_receive_mailbox_base_address(0);

  for(int i = 0; i < 2; i++) {
    address += get_enclave_message(address, read_buffer);
    length = do_aes(write_buffer, read_buffer);
    output += send_enclave_message(output, write_buffer, length);
  }
  output_string("Exiting enclave.\n");
}

int main() {
  int id = get_enclave_id();

  if(id == 0) {
    normal_world();
  } else if (id == MY_ENCLAVE_ID) {
    enclave_world();
  } else {
    //Do nothing if you are any other enclave.
  }
}

#include "praesidio.h"
#include "aes.h"

#define ENCRYPT (0xAA)
#define DECRYPT (0x55)

//Send byte with ENCRYPT/DECRYPT, then 128-bit key, then 128-bit IV, then 32-bit length, then length amount of bytes

#define COMMAND_POSITION (0)
#define KEY_POSITION (COMMAND_POSITION + 1)
#define IV_POSITION (KEY_POSITION + AES_KEYLEN)
#define LENGTH_POSITION (IV_POSITION + AES_BLOCKLEN)

#define NUMBER_OF_ENCLAVE_PAGES (5)

//This is the code that runs in the normal world.
void normal_world() {
  //Setting up encryption related variables.
  int length = 3*AES_BLOCKLEN;
  int message_length = 1 + AES_KEYLEN + AES_BLOCKLEN + sizeof(length) + length;
  char message[message_length];
  char content[length];
  int offset = 0;
  //initializing content
  char tmp_char = 'A';
  for(int i = 0; i < length; i++, tmp_char++) {
    content[i] = tmp_char;
      if(tmp_char == 'Z') {
        tmp_char = '@';
      }
  }
  output_string("Content is:\n0x");
  for (int i = 0; i < length; i++) {
    output_hexbyte(content[i]);
  }
  output_char('\n');

  //Starting enclave
  char *enclaveMemory = (char *) DRAM_BASE + 6*PAGE_SIZE;
  char *enclavePages[NUMBER_OF_ENCLAVE_PAGES];
  for(int i = 0; i < NUMBER_OF_ENCLAVE_PAGES; i++) {
    enclavePages[i] = enclaveMemory;
    enclaveMemory += PAGE_SIZE;
  }
  enclave_id_t myEnclave = start_enclave((char *) DRAM_BASE, NUMBER_OF_ENCLAVE_PAGES, enclavePages);
  if(myEnclave == ENCLAVE_INVALID_ID) return;

  //Setting up enclave related variables.
  long page_num = 2;
  volatile char *address;
  char *input = (char *) PAGE_TO_POINTER(page_num);
  char read_buffer[length];
  give_read_permission(page_num, myEnclave);
  address = get_receive_mailbox_base_address(myEnclave);

  //Preparing the encryption message.
  message[COMMAND_POSITION] = ENCRYPT;
  for(int i = 0; i < AES_BLOCKLEN; i++) {
    message[i+KEY_POSITION] = (i%16); //Set key bytes
    message[i+IV_POSITION] = i; //Set IV bytes
  }
  for(int i = 0; i < sizeof(length); i++) {
    message[i+LENGTH_POSITION] = ((length >> (8*(sizeof(length)-1-i))) & 0xFF);
  }
  offset = LENGTH_POSITION + sizeof(length);
  for(int i = 0; i < length; i++) {
    message[i+offset] = content[i];
  }
  input += send_enclave_message(input, message, message_length);
  address += get_enclave_message(address, read_buffer);
  output_string("Got encryption:\n0x");
  for (int i = 0; i < length; i++) {
    output_hexbyte(read_buffer[i]);
  }
  output_char('\n');

  //Preparing the decryption message.
  message[0] = DECRYPT;
  for(int i = 0; i < length; i++) {
    message[i+offset] = read_buffer[i];
  }
  input += send_enclave_message(input, message, message_length);
  address += get_enclave_message(address, read_buffer);
  output_string("Got decryption:\n0x");
  for (int i = 0; i < length; i++) {
    output_hexbyte(read_buffer[i]);
  }
  output_char('\n');
}

struct AES_ctx aes_context;

int do_aes(char *output, char *input) {
  int length = 0;
  int offset = LENGTH_POSITION;
  for (int i = 0; i < sizeof(length); i++) {
    length |= (((int) input[i+offset]) << (8*(sizeof(length)-1-i)));
  }
  offset += sizeof(length);
  AES_init_ctx_iv(&aes_context, &input[KEY_POSITION], &input[IV_POSITION]);
  // output_string("Key:\n0x");
  // for(int i = 0; i < AES_KEYLEN; i++) {
  //   output_hexbyte(input[KEY_POSITION+i]);
  // }
  // output_string("\nIV:\n0x");
  // for(int i = 0; i < AES_BLOCKLEN; i++) {
  //   output_hexbyte(input[IV_POSITION+i]);
  // }
  // output_char('\n');
  if(input[0] == ENCRYPT || input[0] == DECRYPT) { //Encryption and decryption is the same function for CTR mode.
    AES_CTR_xcrypt_buffer(&aes_context, &input[offset], length);
    for (int i = 0; i < length; i++) {
      output[i] = input[offset+i]; //TODO make counter mode function output into the output variable.
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
  long page_num = NUMBER_OF_ENCLAVE_PAGES+3;
  char *output = (char *) PAGE_TO_POINTER(page_num);
  char read_buffer[10*AES_BLOCKLEN];
  char write_buffer[10*AES_BLOCKLEN];
  int length;

  give_read_permission(page_num, ENCLAVE_DEFAULT_ID);
  address = get_receive_mailbox_base_address(ENCLAVE_DEFAULT_ID);

  for(int i = 0; i < 2; i++) {
    address += get_enclave_message(address, read_buffer);
    length = do_aes(write_buffer, read_buffer);
    output += send_enclave_message(output, write_buffer, length);
  }
  output_string("Exiting enclave.\n");
}

int main() {
  int id = getCurrentEnclaveID();

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else {
    enclave_world();
  }
}

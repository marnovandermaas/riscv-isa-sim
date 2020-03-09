#include "praesidio.h"


volatile int y;

void normal_world() {
  //This is the code that runs in the normal world.
  volatile int x;
  enclave_id_t myEnclave = start_enclave();
  if(myEnclave == ENCLAVE_INVALID_ID) return;

  int start, end;

  start = getMissCount();
  output_string("N\n");
  end = getMissCount();
  output_hexbyte(end-start);
  output_char('\n');
  start = getMissCount();
  x = y;
  end = getMissCount();
  output_hexbyte(end-start);
  output_char('\n');
  start = getMissCount();
  x = y;
  end = getMissCount();
  output_hexbyte(end-start);
  output_char('\n');
}

void enclave_world() {
  //This is the code that runs in the enclave world.
  output_string("E\n");
}

int main() {
  int id = getCurrentEnclaveID();

  if(id == ENCLAVE_DEFAULT_ID) {
    normal_world();
  } else if (id == 1) {
    enclave_world();
  } else {
    //Do nothing if you are any other enclave.
  }
}

#include "management.h"
#include "instructions.h"
#include <stdio.h>

struct ManagementState_t state;

enum boolean initialization_done = BOOL_FALSE;

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

void flushRemappingTable() {
  //TODO flush all entries in this core's remapping table
}

void flushL1Cache() {
  //TODO flush all entries in this core's L1 cache
}

void fillPage(char *basePointer, char value) {
  for(int i = 0; i < PAGE_SIZE; i++) {
    basePointer[i] = value;
  }
}

char putPageEntry(char *basePointer, enclave_id_t id) {
  //This load is to simulate the latency that would be caused to get this tag into the cache.
  volatile char x = *basePointer;
  setArgumentEnclaveIdentifier(id);
  //Set the page to the specified enclave identifier.
  asm volatile (
    "csrrw zero, 0x40F, %0"
    :
    : "r"(basePointer)
    :
  );
  return x;
}

void resetNormalWorld() {
  //TODO somehow reset the remapping table of the normal world.
}

void clearWorkingMemory() {
  //TODO go through all pages in working memory (except for the management code)
  //This is not necessary in Spike, but will be necessary when implementing in actual hardware to avoid cold boot attacks.
}

enum boolean getMessage(struct Message_t *msg, enclave_id_t destination) {
  //TODO
  //Check if there is a message in LLC for this destination
      //If yes: put message into msg and return true
      //If no: return false
}

//TODO this function should be inline
void saveCurrentContext(struct Context_t *context) {
  //TODO save current context to context argument.
}

void initialize() {
  switchEnclave(ENCLAVE_MANAGEMENT_ID);
  flushRemappingTable();
  flushL1Cache();
  CoreID_t coreID = getCoreID();
  CoreID_t *enclaveCores = (CoreID_t *) 0x1024 /*ROM location of enclave 0's core ID*/;
  if(coreID == enclaveCores[0]) { //TODO fill state.enclaveCores
    output_string("In management enclave.\n");
    state.nextEnclaveID = 1;
    for(int i = 0; i < NUMBER_OF_ENCLAVE_CORES; i++) {
      state.runningEnclaves[i] = ENCLAVE_MANAGEMENT_ID;
    }
    putPageEntry(MANAGEMENT_CODE_BASE_ADDRESS, ENCLAVE_MANAGEMENT_ID);
    putPageEntry(PAGE_DIRECTORY_BASE_ADDRESS, ENCLAVE_MANAGEMENT_ID);
    putPageEntry(ENCLAVE_DATA_BASE_ADDRESS, ENCLAVE_MANAGEMENT_ID);
    fillPage(PAGE_DIRECTORY_BASE_ADDRESS, 0xFF);
    fillPage(ENCLAVE_DATA_BASE_ADDRESS, 0x00);

    resetNormalWorld();
    clearWorkingMemory();
    initialization_done = BOOL_TRUE; //This starts the normal world
    while(1) {
      managementRoutine();
      //wait for 1000 milliseconds
    }
  }
  //setManagementInterruptTimer(1000); //Time in milliseconds
  switchEnclave(2); //TODO remove this and go to waitForEnclave instead.
  //waitForEnclave();
}

void managementRoutine() {
  struct Context_t savedContext;
  saveCurrentContext(&savedContext);
  enclave_id_t savedEnclaveID = getCurrentEnclaveID();

  struct Message_t message;
  receiveMessage(&message);

  struct Message_t response;
  response.source = message.destination;
  response.destination = message.source;
  response.type = message.type;

  if(message.source == ENCLAVE_DEFAULT_ID &&
      message.destination == ENCLAVE_MANAGEMENT_ID) {
    switch(message.type) {
      case MSG_CREATE_ENCLAVE:
        output_string("Received create enclave message.\n");
        break;
      case MSG_DELETE_ENCLAVE:
        output_string("Received delete enclave message.\n");
        break;
      case MSG_ATTEST: //Not needed yet.
        break;
      case MSG_ACQUIRE_PHYS_CAP:
        break;
      case MSG_DONATE_PAGE:
        output_string("Received donate page enclave message.\n");
        break;
      case MSG_SWITCH_ENCLAVE: //Not needed yet.
        break;
    }
  }
  //TODO inter enclave messages
  //TODO send response
}

Address_t waitForEnclave() {
  Address_t entryPoint = 0;
  while(1) {
    //TODO check for message that says to start an enclave.
  }
  return entryPoint;
}

void normalWorld() {
  while(initialization_done == BOOL_FALSE) {
  }
}

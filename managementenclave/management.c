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

void fillPage(Address_t baseAddress, char value) {
  char *basePointer = (char *) baseAddress;
  for(int i = 0; i < PAGE_SIZE; i++) {
    basePointer[i] = value;
  }
}

//Returns char because then the load does not get optimized away.
char putPageEntry(Address_t baseAddress, enclave_id_t id) {
  //This load is to simulate the latency that would be caused to get this tag into the cache.
  char *basePointer = (char *) baseAddress;
  volatile char x = *basePointer; //TODO remove this, because it will not allow this if management enclave does not own the page.
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

//TODO this function should be inline
void saveCurrentContext(struct Context_t *context) {
  //TODO save current context to context argument.
}

int createEnclave() {
  //TODO add support for multiple enclaves
  struct EnclaveData_t *enclaveData = (struct EnclaveData_t *) ENCLAVE_DATA_BASE_ADDRESS;
  enclaveData->eID = state.nextEnclaveID;
  state.nextEnclaveID += 1;
  enclaveData->state = STATE_CREATED;
  return 0;
}

enum boolean donatePage(enclave_id_t recipient, Address_t page_base) {
  //TODO look for enclave data in enclave data structure.
  struct EnclaveData_t *enclaveData = (struct EnclaveData_t *) ENCLAVE_DATA_BASE_ADDRESS;
  if(recipient != enclaveData->eID) {
    output_string("Donate Page: enclaveID ERROR!\n");
    return BOOL_FALSE;
  }
  if(page_base & (PAGE_SIZE-1)) {
    output_string("Donate Page: address not at base of page ERROR!\n");
    return BOOL_FALSE;
  }
  switch(enclaveData->state) {
    case STATE_CREATED:
      enclaveData->codeEntryPoint = page_base; //This assumes the first page that is given to an enclave is also the code entry point.
      enclaveData->state = STATE_RECEIVINGPAGES;
      putPageEntry(page_base, recipient);
      break;
    case STATE_RECEIVINGPAGES:
      putPageEntry(page_base, recipient);
      break;
    case STATE_FINALIZED:
      output_string("Donate Page: enclave already running ERROR!\n");
      return BOOL_FALSE;
  }
}

void switchEnclave(CoreID_t coreID, enclave_id_t eID) {
  //TODO make this work for actually switching enclaves and not just starting one up.
  struct Message_t command;
  command.source = ENCLAVE_MANAGEMENT_ID;
  command.destination = ENCLAVE_MANAGEMENT_ID - coreID;
  command.type = MSG_SWITCH_ENCLAVE;
  command.content = eID;
  sendMessage(&command);
  do {
    receiveMessage(&command);
  } while(command.source != ENCLAVE_MANAGEMENT_ID - coreID);
}

Address_t waitForEnclave() {
  Address_t entryPoint = 0;
  struct Message_t message;
  struct EnclaveData_t *enclaveData;
  enclave_id_t tmpID;
  while(1) {
    receiveMessage(&message);
    if(message.source == ENCLAVE_MANAGEMENT_ID) {
      tmpID = message.source;
      message.source = message.destination;
      message.destination = tmpID;
      sendMessage(&message);

      switchEnclaveID(message.content);
      enclaveData = (struct EnclaveData_t *) ENCLAVE_DATA_BASE_ADDRESS; //TODO make this dependent on the received message.
      entryPoint = enclaveData->codeEntryPoint;
      break;
    }
  }
  return entryPoint;
}

enclave_id_t internalArgument = ENCLAVE_INVALID_ID;

void managementRoutine() {
  struct Context_t savedContext;
  saveCurrentContext(&savedContext);
  enclave_id_t savedEnclaveID = getCurrentEnclaveID();
  char prntString[5] = "t  \n";

  struct Message_t message;
  receiveMessage(&message);

  struct Message_t response;
  response.source = message.destination;
  response.destination = message.source;
  response.type = message.type;
  response.content = 0;

  if(message.source == ENCLAVE_DEFAULT_ID &&
      message.destination == ENCLAVE_MANAGEMENT_ID) {
    prntString[2] = ((char) message.type) + '0';
    output_string(prntString);
    switch(message.type) {
      case MSG_CREATE_ENCLAVE:
        output_string("Received create enclave message.\n");
        int index = createEnclave();
        response.content = ((struct EnclaveData_t *) ENCLAVE_DATA_BASE_ADDRESS)[index].eID;
        break;
      case MSG_DELETE_ENCLAVE:
        output_string("Received delete enclave message.\n");
        break;
      case MSG_ATTEST: //Not needed yet.
        break;
      case MSG_ACQUIRE_PHYS_CAP: //Not needed yet.
        break;
      case MSG_DONATE_PAGE:
        output_string("Received donate page enclave message.\n");
        donatePage(internalArgument, message.content);
        break;
      case MSG_SWITCH_ENCLAVE:
        output_string("Received switch enclave message.\n");
        switchEnclave(2, message.content);
        break;
      case MSG_SET_ARGUMENT: //TODO include this in the donate page message.
        output_string("Received set argument message.\n");
        internalArgument = message.content;
        break;
    }
    sendMessage(&response);
  }
  //TODO inter enclave messages
}

Address_t initialize() {
  CoreID_t coreID = getCoreID();
  switchEnclaveID(ENCLAVE_MANAGEMENT_ID - coreID);
  flushRemappingTable();
  flushL1Cache();
  CoreID_t *enclaveCores = (CoreID_t *) 0x1024 /*ROM location of enclave 0's core ID*/;
  if(coreID == enclaveCores[0]) { //TODO fill state.enclaveCores
    switchEnclaveID(ENCLAVE_MANAGEMENT_ID);
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
  return waitForEnclave();
}

void normalWorld() {
  while(initialization_done == BOOL_FALSE) {
    //Waiting until enclave core 0 has finished initialization
  }
}

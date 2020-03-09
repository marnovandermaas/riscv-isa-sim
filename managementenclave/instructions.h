#ifndef PRAESIDIO_INSTRUCTIONS_H
#define PRAESIDIO_INSTRUCTIONS_H

#include "management.h"

void switchEnclaveID(enclave_id_t id);
int* derivePhysicalCapability(struct PhysCap_t sourceCap);
void startNormalWorld();
CoreID_t getCoreID();
struct PhysCap_t getRootCapability();
void setManagementInterruptTimer(int milis);
int getMissCount();
enclave_id_t getCurrentEnclaveID();
//This is a helper instruction, but will be removed in final design
void setArgumentEnclaveIdentifier(enclave_id_t id);
void sendMessage(struct Message_t *txMsg);
void receiveMessage(struct Message_t *rxMsg);

#endif //PRAESIDIO_INSTRUCTIONS_H

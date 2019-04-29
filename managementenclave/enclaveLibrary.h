#ifndef ENCLAVE_LIBRARY_HEADER
#define ENCLAVE_LIBRARY_HEADER

#define ENCLAVE_MANAGEMENT_ID (0xFFFFFFFFFFFFFFFF)
#define ENCLAVE_DEFAULT_ID    (0)
#define ENCLAVE_INVALID_ID    (0xFFFFFFFFFFFFFFFE)

#define MANAGEMENT_ENCLAVE_BASE 0x04000000 //This number should be above 0x020c 0000 which is the end of CLINT and below 0x4000 0000 which is the start of IO

enum MessageType_t {
  MSG_CREATE_ENCLAVE    = 0x0,
  MSG_DELETE_ENCLAVE    = 0x1,
  MSG_ATTEST            = 0x2,
  MSG_ACQUIRE_PHYS_CAP  = 0x3,
  MSG_DONATE_PAGE       = 0x4,
  MSG_SWITCH_ENCLAVE    = 0x5,
  MSG_INTER_ENCLAVE     = 0x6,
  MSG_MASK              = 0xF,
};

#endif //ENCLAVE_LIBRARY_HEADER

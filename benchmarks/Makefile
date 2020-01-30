RV_PREFIX = riscv64-unknown-linux-gnu-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
DEBUG_ENABLED ?=1
INCLUDE_DIR = include
BUILD_DIR = build
MANAGE_DIR = ../managementenclave
BITCNT_DIR = bitcount
AES_DIR = aes
CCFLAGS = -Werror-implicit-function-declaration
CCFLAGS += -I$(MANAGE_DIR)
CCFLAGS += -I$(INCLUDE_DIR)
ifeq ($(DEBUG_ENABLED), 1)
CCFLAGS += -g -Og

endif
ENCLAVE_CCFLAGS = $(CCFLAGS) -nostdlib -mcmodel=medany -T$(INCLUDE_DIR)/link.ld -O #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.

DEPS = init praesidio
SUFFIX ?=1

Enclave_Link_Flags=-L$(BUILD_DIR)

OBJS=$(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

DEFAULTBUILD = hello.out

all: hello

$(BUILD_DIR)/init.o:
	$(CC) -c $(INCLUDE_DIR)/init.s -o $(BUILD_DIR)/init.o
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(INCLUDE_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(MANAGE_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: %.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/hello%.o: hello/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(AES_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(BITCNT_DIR)/%.c
	@$(CC) $(LIBCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"


hello: $(OBJS) $(BUILD_DIR)/helloenclave.o
	@$(CC) $(ENCLAVE_CCFLAGS) $^ -o $@.out
	@echo "LINK =>  $@"

# BITCNT_OBJS = $(BUILD_DIR)/bitcnt_1.o $(BUILD_DIR)/bitcnt_2.o $(BUILD_DIR)/bitcnt_3.o $(BUILD_DIR)/bitcnt_4.o
#
# bitcount: $(OBJS) $(LIBC_OBJS) $(BITCNT_OBJS)
# 	$(CC) $(LIBCFLAGS) -c $(BITCNT_DIR)/main.c -o $(BUILD_DIR)/$@.o
# 	$(CC) $(LIBCFLAGS) -o $@.out $(BITCNT_OBJS) $(BUILD_DIR)/$@.o $(LINK_FILES) $(BUILD_DIR)/printf.o $(OBJS) $(Enclave_Link_Flags)
# 	@echo "LINK =>  $@"
#
# startenclaves: $(OBJS) $(BUILD_DIR)/startenclaves.o
# 	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
# 	@echo "LINK =>  $@"
#
# aes: $(OBJS) $(BUILD_DIR)/aes.o
# 	$(CC) $(CCFLAGS) -c $(AES_DIR)/main.c -o $(BUILD_DIR)/$@_main.o
# 	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/$@_main.o $(OBJS) $(Enclave_Link_Flags)
# 	@echo "LINK =>  $@"
#
# latency: $(OBJS) $(BUILD_DIR)/latency.o
# 	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
# 	@echo "LINK =>  $@"
#
# bandwidth: $(OBJS) $(BUILD_DIR)/bandwidth.o
# 	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
# 	@echo "LINK =>  $@"
#
# createenclave: $(OBJS) $(BUILD_DIR)/createenclave.o
# 	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
# 	@echo "LINK =>  $@"
#
# nullecall: $(OBJS) $(BUILD_DIR)/nullecall.o
# 	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
# 	@echo "LINK =>  $@"

#Remove .out and .o files
clean:
	rm -rf *.out $(BUILD_DIR)/*.o

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(DEFAULTBUILD)

run:
	./runTests.sh $(SUFFIX)

test:
	spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

#not supported yet
remote_debug:
	spike --rbb-port=9824 --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

debug:
	spike -d --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

#caches specified with sets:ways:blocks

RV_PREFIX = riscv64-unknown-elf-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
INCLUDE_DIR = include
BUILD_DIR = build
MANAGE_DIR = ../managementenclave/
CCFLAGS = -nostdlib -mcmodel=medany -T$(INCLUDE_DIR)/link.ld #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.
DEPS = init praesidio

CCFLAGS += -I$(MANAGE_DIR)

DEFAULTBUILD = hello

all: $(DEFAULTBUILD)

#Compile init.s
init:
	$(CC) -c $(INCLUDE_DIR)/init.s -o $(BUILD_DIR)/init.o

praesidio:
	$(CC) $(CCFLAGS) -c $(INCLUDE_DIR)/praesidio.c -o $(BUILD_DIR)/praesidio.o
	$(CC) $(CCFLAGS) -c $(MANAGE_DIR)/instructions.c -o $(BUILD_DIR)/instructions.o

#%.c: $(DEPS)
#	$(CC) $(CCFLAGS) -c $@ -o $(BUILD_DIR)/$@.o
#$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

#Compile hello.c and link hello.o, praesidio.o and init.o
hello: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

#Compile aes.c and link aes.o, praesidio.o and init.o
aes: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

#Compile latency.c and link with praesidio.o and init.o
latency: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

#Compile bandwidth.c and link with praesidio.o and init.o
bandwidth: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

#Remove .out and .o files
clean:
	rm *.out $(BUILD_DIR)/*.o

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(DEFAULTBUILD).out

run:
	spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 $(DEFAULTBUILD).out #caches specified with sets:ways:blocks

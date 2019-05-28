RV_PREFIX = riscv64-unknown-elf-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
INCLUDE_DIR = include
BUILD_DIR = build
MANAGE_DIR = ../managementenclave/
BITCNT_DIR = bitcount
CCFLAGS = -nostdlib -mcmodel=medany -T$(INCLUDE_DIR)/link.ld #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.
#TODO split CCFLAGS and LDFLAGS
DEPS = init praesidio
LINK_FILES = $(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

CCFLAGS += -I$(MANAGE_DIR)
CCFLAGS += -I$(INCLUDE_DIR)

DEFAULTBUILD = hello

all: hello startenclaves bitcount bandwidth latency aes

#Compile init.s
init:
	$(CC) -c $(INCLUDE_DIR)/init.s -o $(BUILD_DIR)/init.o

praesidio:
	$(CC) $(CCFLAGS) -c $(INCLUDE_DIR)/praesidio.c -o $(BUILD_DIR)/praesidio.o
	$(CC) $(CCFLAGS) -c $(MANAGE_DIR)/instructions.c -o $(BUILD_DIR)/instructions.o

libraries:
	$(CC) $(CCFLAGS) -c lib/printf.c -o $(BUILD_DIR)/printf.o

#%.c: $(DEPS)
#	$(CC) $(CCFLAGS) -c $@ -o $(BUILD_DIR)/$@.o
#$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

#Compile hello.c and link hello.o, praesidio.o and init.o
hello: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

bitcount: $(DEPS) libraries
	$(CC) $(CCFLAGS) -c $(BITCNT_DIR)/bitcnt_1.c -o $(BUILD_DIR)/bitcnt_1.o
	$(CC) $(CCFLAGS) -c $(BITCNT_DIR)/bitcnt_2.c -o $(BUILD_DIR)/bitcnt_2.o
	$(CC) $(CCFLAGS) -c $(BITCNT_DIR)/bitcnt_3.c -o $(BUILD_DIR)/bitcnt_3.o
	$(CC) $(CCFLAGS) -c $(BITCNT_DIR)/bitcnt_4.c -o $(BUILD_DIR)/bitcnt_4.o
	$(CC) $(CCFLAGS) -c $(BITCNT_DIR)/main.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/bitcnt_1.o $(BUILD_DIR)/bitcnt_2.o $(BUILD_DIR)/bitcnt_3.o $(BUILD_DIR)/bitcnt_4.o $(BUILD_DIR)/$@.o $(LINK_FILES) $(BUILD_DIR)/printf.o

startenclaves: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(LINK_FILES)

#Compile aes.c and link aes.o, praesidio.o and init.o
aes: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(LINK_FILES)

#Compile latency.c and link with praesidio.o and init.o
latency: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(LINK_FILES)

#Compile bandwidth.c and link with praesidio.o and init.o
bandwidth: $(DEPS)
	$(CC) $(CCFLAGS) -c $@.c -o $(BUILD_DIR)/$@.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(LINK_FILES)

run:
	./runTests.sh

#Remove .out and .o files
clean:
	rm *.out $(BUILD_DIR)/*.o

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(DEFAULTBUILD).out

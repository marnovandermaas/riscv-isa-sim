RV_PREFIX = riscv64-unknown-elf-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
DEBUG_ENABLED ?=1
INCLUDE_DIR = include
BUILD_DIR = build
MANAGE_DIR = ../managementenclave
BITCNT_DIR = bitcount
AES_DIR = aes
CCFLAGS = -nostdlib -mcmodel=medany -T$(INCLUDE_DIR)/link.ld -Werror-implicit-function-declaration -DDEFINE_MALLOC -DDEFINE_FREE -O #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.
DEPS = init praesidio
SUFFIX ?=1

CCFLAGS += -I$(MANAGE_DIR)
CCFLAGS += -I$(INCLUDE_DIR)

ifeq ($(DEBUG_ENABLED), 1)
CCFLAGS += -g -Og

endif

LIBC_DIR = nolibc

LIBCFLAGS = $(CCFLAGS) -I. -I$(MANAGE_DIR)/include -I$(LIBC_DIR)/include -std=c99 -Wall -Werror -Wno-parentheses -Wno-unused-variable #-O2 -std=c99 -Wall  -Werror


Enclave_Link_Flags=-L$(BUILD_DIR) 

OBJS=$(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

LIBC_OBJS=$(BUILD_DIR)/ctype.o \
  	 $(BUILD_DIR)/dtoa.o  \
	 $(BUILD_DIR)/float.o \
     $(BUILD_DIR)/memchr.o $(BUILD_DIR)/memcmp.o $(BUILD_DIR)/memcpy.o $(BUILD_DIR)/memmove.o $(BUILD_DIR)/memset.o \
     $(BUILD_DIR)/strcmp.o $(BUILD_DIR)/strlen.o $(BUILD_DIR)/strtol.o $(BUILD_DIR)/strchr.o $(BUILD_DIR)/strchrnul.o $(BUILD_DIR)/strncpy.o $(BUILD_DIR)/stpncpy.o \
     $(BUILD_DIR)/strstr.o \
     $(BUILD_DIR)/stubs.o \
	 $(BUILD_DIR)/printf.o \
	 $(BUILD_DIR)/tiny-malloc.o \
	 $(BUILD_DIR)/frexp.o \
	 $(BUILD_DIR)/sysdeps_riscv.o $(BUILD_DIR)/riscv_spec.o #$(BUILD_DIR)/vfprintf.o $(BUILD_DIR)/vsnprintf.o $(BUILD_DIR)/snprintf.o $(BUILD_DIR)/fprintf.o $(BUILD_DIR)/puts.o 



DEFAULTBUILD = hello.out

all: hello startenclaves bitcount bandwidth latency aes

$(BUILD_DIR)/init.o: 
	$(CC) -c $(INCLUDE_DIR)/init.s -o $(BUILD_DIR)/init.o	
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(INCLUDE_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"


$(BUILD_DIR)/%.o: %.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(AES_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(BITCNT_DIR)/%.c
	@$(CC) $(LIBCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(MANAGE_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"


$(BUILD_DIR)/%.o: $(LIBC_DIR)/%.c
	@$(CC) $(LIBCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(LIBC_DIR)/dtoa.o: LIBCFLAGS+=-fno-strict-aliasing


hello: $(OBJS) $(LIBC_OBJS) $(BUILD_DIR)/hello.o
	@$(CC) $(LIBCFLAGS) $^ -o $@.out $(Enclave_Link_Flags) 	
	@echo "LINK =>  $@"

BITCNT_OBJS = $(BUILD_DIR)/bitcnt_1.o $(BUILD_DIR)/bitcnt_2.o $(BUILD_DIR)/bitcnt_3.o $(BUILD_DIR)/bitcnt_4.o

bitcount: $(OBJS) $(LIBC_OBJS) $(BITCNT_OBJS)
	$(CC) $(LIBCFLAGS) -c $(BITCNT_DIR)/main.c -o $(BUILD_DIR)/$@.o
	$(CC) $(LIBCFLAGS) -o $@.out $(BITCNT_OBJS) $(BUILD_DIR)/$@.o $(LINK_FILES) $(BUILD_DIR)/printf.o $(OBJS) $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

startenclaves: $(OBJS) $(BUILD_DIR)/startenclaves.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

aes: $(OBJS) $(BUILD_DIR)/aes.o
	$(CC) $(CCFLAGS) -c $(AES_DIR)/main.c -o $(BUILD_DIR)/$@_main.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(BUILD_DIR)/$@_main.o $(OBJS) $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

latency: $(OBJS) $(BUILD_DIR)/latency.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

bandwidth: $(OBJS) $(BUILD_DIR)/bandwidth.o
	$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

#Remove .out and .o files
clean:
	rm -r *.out $(BUILD_DIR)/*.o

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(DEFAULTBUILD)

run:
	./runTests.sh

run_updated:
	./runTests_update.sh $(SUFFIX)

test:
	spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

#not supported yet
remote_debug:
	spike --rbb-port=9824 --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

debug:
	spike -d --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

#caches specified with sets:ways:blocks

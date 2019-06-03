RV_PREFIX = riscv64-unknown-elf-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
DEBUG_ENABLED ?=1
INCLUDE_DIR = include
BUILD_DIR = build
MANAGE_DIR = ../managementenclave
CCFLAGS = -nostdlib -mcmodel=medany -T$(INCLUDE_DIR)/link.ld #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.
DEPS = init praesidio

CCFLAGS += -I$(MANAGE_DIR)

ifeq ($(DEBUG_ENABLED), 1)
CCFLAGS += -g -Og

endif

LIBC_DIR = $(MANAGE_DIR)/nolibc

LIBCFLAGS = $(CCFLAGS) -I. -I$(MANAGE_DIR)/include -I$(LIBC_DIR)/include -std=c99 -Wall -Werror -Wno-parentheses -Wno-unused-variable -Wno-unused-but-set-variable#-O2 -std=c99 -Wall  -Werror


Enclave_Link_Flags=-L$(BUILD_DIR) 

OBJS=$(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o  $(BUILD_DIR)/hello.o

LIBC_OBJS=$(BUILD_DIR)/ctype.o \
    $(BUILD_DIR)/dtoa.o  \
	 $(BUILD_DIR)/float.o \
     $(BUILD_DIR)/memchr.o $(BUILD_DIR)/memcmp.o $(BUILD_DIR)/memcpy.o $(BUILD_DIR)/memmove.o $(BUILD_DIR)/memset.o \
     $(BUILD_DIR)/strcmp.o $(BUILD_DIR)/strlen.o $(BUILD_DIR)/strtol.o $(BUILD_DIR)/strchr.o $(BUILD_DIR)/strchrnul.o $(BUILD_DIR)/strncpy.o $(BUILD_DIR)/stpncpy.o \
     $(BUILD_DIR)/strstr.o \
     $(BUILD_DIR)/stubs.o \
	 $(BUILD_DIR)/printf.o $(BUILD_DIR)/frexp.o $(BUILD_DIR)/sysdeps_riscv.o $(BUILD_DIR)/riscv_spec.o #$(BUILD_DIR)/vfprintf.o $(BUILD_DIR)/vsnprintf.o $(BUILD_DIR)/snprintf.o $(BUILD_DIR)/fprintf.o $(BUILD_DIR)/puts.o 



DEFAULTBUILD = hello

all: $(DEFAULTBUILD)

$(BUILD_DIR)/init.o: 
	$(CC) -c $(INCLUDE_DIR)/init.s -o $(BUILD_DIR)/init.o	
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(INCLUDE_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"


$(BUILD_DIR)/%.o: %.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(MANAGE_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"


$(BUILD_DIR)/%.o: $(LIBC_DIR)/%.c
	@$(CC) $(LIBCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(LIBC_DIR)/dtoa.o: LIBCFLAGS+=-fno-strict-aliasing


$(DEFAULTBUILD): $(OBJS) $(LIBC_OBJS)
	@$(CC) $(LIBCFLAGS) $^ -o $@ $(Enclave_Link_Flags) 	
	@echo "LINK =>  $@"

#Remove .out and .o files
clean:
	rm -rf $(DEFAULTBUILD) $(BUILD_DIR)/*.o

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(DEFAULTBUILD)

run:
	spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

#not supported yet
remote_debug:
	spike --rbb-port=9824 --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

debug:
	spike -d --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

#caches specified with sets:ways:blocks

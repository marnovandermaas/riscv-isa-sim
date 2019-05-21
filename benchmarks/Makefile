RV_PREFIX = riscv64-unknown-elf-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
INCLUDE_DIR = include
BUILD_DIR = build
MANAGE_DIR = ../managementenclave/
CCFLAGS = -nostdlib -mcmodel=medany -T$(INCLUDE_DIR)/link.ld #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.
DEPS = init praesidio

CCFLAGS += -I$(MANAGE_DIR)

LIBCFLAGS = $(CCFLAGS) -I$(MANAGE_DIR)/include

LIBC_DIR = $(MANAGE_DIR)/nolibc

Enclave_Link_Flags=-L$(BUILD_DIR) 

OBJS=$(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o  $(BUILD_DIR)/hello.o

LIBC_OBJS=$(LIBC_DIR)/ctype.o 
    # $(LIBC_DIR)/dtoa.o /
	# $(LIBC_DIR)/float.o /
     #$(LIBC_DIR)/ memchr.o $(LIBC_DIR)/memcmp.o $(LIBC_DIR)/memcpy.o $(LIBC_DIR)/memmove.o $(LIBC_DIR)/memset.o /
     #$(LIBC_DIR)/strcmp.o $(LIBC_DIR)/strlen.o $(LIBC_DIR)/strtol.o $(LIBC_DIR)/strchr.o $(LIBC_DIR)/strchrnul.o $(LIBC_DIR)/strncpy.o $(LIBC_DIR)/stpncpy.o /
     #$(LIBC_DIR)/strstr.o /
     #$(LIBC_DIR)/stubs.o /
     #$(LIBC_DIR)/vfprintf.o $(LIBC_DIR)/vsnprintf.o $(LIBC_DIR)/snprintf.o $(LIBC_DIR)/fprintf.o $(LIBC_DIR)/printf.o


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


$(LIBC_DIR)/%.o: $(LIBC_DIR)/%.c
	@$(CC) $(Enclave_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

$(LIBC_DIR)/dtoa.o: LIBCFLAGS+=-fno-strict-aliasing


$(DEFAULTBUILD): $(OBJS) 
	@$(CC) $(CCFLAGS) $^ -o $@ $(Enclave_Link_Flags) 
	
	@echo "LINK =>  $@"

#Remove .out and .o files
clean:
	rm *.out $(BUILD_DIR)/*.o

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(DEFAULTBUILD)

run:
	spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

#caches specified with sets:ways:blocks

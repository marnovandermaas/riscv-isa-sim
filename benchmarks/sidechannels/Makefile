RV_PREFIX = riscv64-unknown-elf-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
INCLUDE_DIR = ../include
BUILD_DIR = build
MANAGE_DIR = ../../managementenclave
CCFLAGS = -nostdlib -mcmodel=medany -T$(INCLUDE_DIR)/link.ld -Werror-implicit-function-declaration -O #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.
#TODO split CCFLAGS and LDFLAGS
DEPS = init praesidio
SUFFIX ?=1

CCFLAGS += -I$(MANAGE_DIR)
CCFLAGS += -I$(INCLUDE_DIR)

Enclave_Link_Flags=-L$(BUILD_DIR)

OBJS=$(BUILD_DIR)/init.o $(BUILD_DIR)/praesidio.o $(BUILD_DIR)/instructions.o

all: primeprobe

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

primeprobe: $(OBJS) $(BUILD_DIR)/primeprobe.o
	@$(CC) $(CCFLAGS) -o $@.out $(BUILD_DIR)/$@.o $(OBJS) $(Enclave_Link_Flags)
	@echo "LINK =>  $@"

#Remove .out and .o files
clean:
	rm -rf *.out $(BUILD_DIR)/*.o

DEFAULTBUILD=primeprobe.out

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(DEFAULTBUILD)

test:
	spike --l2=128:1:8 -p1 --enclave=1 --manage-path=../../build/management.bin -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

debug:
	spike -d --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 $(DEFAULTBUILD)

#caches specified with sets:ways:blocks

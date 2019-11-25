RV_PREFIX = riscv64-unknown-elf-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
BUILD_DIR = build
CCFLAGS = -nostdlib -mcmodel=medany -Tlink.ld -Werror-implicit-function-declaration -O #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.
#TODO split CCFLAGS and LDFLAGS
SUFFIX ?=1

all: primeprobe

$(BUILD_DIR)/primeprobe.o:
	$(CC) -c primeprobe.s -o $(BUILD_DIR)/primeprobe.o
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: $(INCLUDE_DIR)/%.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"

$(BUILD_DIR)/%.o: %.c
	@$(CC) $(CCFLAGS) -c $< -o $@
	@echo "CC   <=  $<"


primeprobe: $(BUILD_DIR)/primeprobe.o
	$(CC) $(CCFLAGS) -o primeprobe.out $(BUILD_DIR)/primeprobe.o

#Remove .out and .o files
clean:
	rm -rf *.out $(BUILD_DIR)/*.o

DEFAULTBUILD=primeprobe.out

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(DEFAULTBUILD)

test:
	spike --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=0 -d $(DEFAULTBUILD)


#caches specified with sets:ways:blocks

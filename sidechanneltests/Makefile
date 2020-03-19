RV_PREFIX = riscv64-unknown-elf-#Prefix for our RISC-V tools (e.g. gcc and objdump)
CC = $(RV_PREFIX)gcc #Our compiler
BUILD_DIR = build
CCFLAGS = -nostdlib -mcmodel=medany -Werror-implicit-function-declaration -O #Flags telling GCC to compile without libraries (e.g. baremetal), to use a differen memory model so that we can put our code at 0x80000000 and to use the specified linker script.
SUFFIX ?=1

PRIME = primeprobe
DRAM = dramrowbuffer

all: $(PRIME) $(DRAM)

$(BUILD_DIR)/$(PRIME).o:
	$(CC) -c $(PRIME).s -o $(BUILD_DIR)/$(PRIME).o

$(BUILD_DIR)/$(DRAM).o:
	$(CC) -c $(DRAM).s -o $(BUILD_DIR)/$(DRAM).o

$(PRIME): $(BUILD_DIR)/$(PRIME).o
	$(CC) $(CCFLAGS) -T$(PRIME).ld -o $(PRIME).out $(BUILD_DIR)/$(PRIME).o

$(DRAM): $(BUILD_DIR)/$(DRAM).o
	$(CC) $(CCFLAGS) -T$(DRAM).ld -o $(DRAM).out $(BUILD_DIR)/$(DRAM).o

#Remove .out and .o files
clean:
	rm -rf *.out $(BUILD_DIR)/*.o

#For debugging purposes and objdump of our output file.
dump:
	$(RV_PREFIX)objdump -D $(PRIME).out | less

prime: $(PRIME)
	spike --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=0 -d $(PRIME).out
#caches specified with sets:ways:blocks

dram: $(DRAM)
	spike -p1 --enclave=1 -m2048 --dram-banks -d $(DRAM).out


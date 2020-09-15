# Side-Channel Spike Simulator
Author: Marno van der Maas

Year: 2020

For the original Spike readme, please see [Spike_README](Spike_README.md)

## Building
This process has been tested on Ubuntu 18.04.

Enter the `setup-scripts` directory and install the dependencies using the command in `build_dependencies.sh`

Then execute `build_toolchain.sh` which will create a riscv-tools folder.

Finally execute `build_spike.sh` to build this simulator.

An example command sequence:
```
$ cd setup-scripts
$ sudo ./build_dependencies.sh
$ ./build_toolchain.sh
$ ./build_spike.sh
$ cd ..
```

## Running Tests
To start out execute the following commands starting from this repositories root directory:
```
$ cd sidechanneltests
$ source source.sh
$ mkdir -p build
```

Then we can start executing our experiments.

### Prime and Probe
To make and run the prime and probe experiment execute `make prime`. This will make the test program and run the spike simulator in debug mode. You can press enter to execute single instructions.

The secret is being transmitted by core 1 to core 0. After core 1's program counter reaches 0x8000400C, the register t0 has the value 0x55555555, which is the secret being translated. You can check this by typing `reg 1 t0`. At this point the same register in core 0 is equal to zero. To execute the complete program you can type `until pc 0 0x80000038` and check that the secret has been transfered to core 0 by `reg 0 t0`, which should now be equal to 0x55555555.

### DRAM Row Buffer Collisions
A very similar program also works for DRAM row buffer collissions, to start out this use the `make dram` command.

After core 1's program counter reaches 0x8002000C, the register t0 has the secret value 0x55555555. Then similar to the prime and probe example, you can use `until pc 0 0x80000038` to run until the end of the program and check that t0 is equal to the secret value.

Example output for this experiment:
```
$ make dram
riscv64-unknown-elf-gcc  -nostdlib -mcmodel=medany -Werror-implicit-function-declaration -O  -Tdramrowbuffer.ld -o dramrowbuffer.out build/dramrowbuffer.o
spike -p1 --enclave=1 -m2048 --dram-banks -d dramrowbuffer.out
warning: tohost and fromhost symbols not in ELF; can't communicate with target
: until pc 1 0x8002000c
: reg 0 t0
0x0000000000000000
: reg 1 t0
0x0000000055555555
: until pc 0 0x80000038
: reg 0 t0
0x0000000055555555
: 
core   1: 0x0000000080020016 (0x00030f63) beqz    t1, pc + 30
: 
core   0: 0x0000000080000038 (0x40501073) csrw    bareMetalExit, zero
: 
core   1: 0x0000000080020034 (0x40501073) csrw    bareMetalExit, zero

>>>>>INSTRUCTION_COUNT<<<<<
366

>>>>>CACHE_OUTPUT<<<<<
```

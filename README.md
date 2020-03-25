# Side-Channel Spike Simulator
Author: Marno van der Maas
Year: 2020

This simulator is part of a workshop paper that turns side channels into direct channels. For the original Spike readme, please see [Spike_README](Spike_README.md)

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

The secret is being transmitted by core 1 to core 0. After core 1's program counter reaches 0x8004008, the register t0 has the value 0x55555555, which is the secret being translated. You can check this by typing `reg 1 t0`. At this point the same register in core 0 is equal to zero. To execute the complete program you can type `until pc 0 0x80000038` and check that the secret has been transfered to core 0 by `reg 0 t0`, which should now be equal to 0x55555555.

### DRAM Row Buffer Collisions
A very similar program also works for DRAM row buffer collissions, the only difference is you start out with the `make dram` command.

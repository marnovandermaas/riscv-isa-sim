#!/bin/bash
set -x
make clean
make all
#caches specified with sets:ways:blocks
export OUTPUT_DIR=output
spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 hello.out > $OUTPUT_DIR/hello.log
spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=14 -m1024 --l2_partitioning=1 -g startenclaves.out > $OUTPUT_DIR/startenclaves.log
spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 -m1024 --l2_partitioning=1 bitcount.out > $OUTPUT_DIR/bitcount.log
spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 bandwidth.out > $OUTPUT_DIR/bandwidth.log
spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 latency.out > $OUTPUT_DIR/latency.log
spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 aes.out > $OUTPUT_DIR/aes.log

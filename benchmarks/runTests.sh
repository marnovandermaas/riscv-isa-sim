#!/bin/bash
set -x
make clean
make all
#caches specified with sets:ways:blocks
OUTPUT_DIR=output
CACHE_WAYS_AND_BLOCK_SIZE=:1:16
INSTRUCTION_CACHE=16$CACHE_WAYS_AND_BLOCK_SIZE
DATA_CACHE=$INSTRUCTION_CACHE
SHARED_CACHE=256$CACHE_WAYS_AND_BLOCK_SIZE
spike --ic=$INSTRUCTION_CACHE --dc=$DATA_CACHE --l2=$SHARED_CACHE -p1 --enclave=1 -m1024 --l2_partitioning=1 hello.out > $OUTPUT_DIR/hello.log
spike --ic=$INSTRUCTION_CACHE --dc=$DATA_CACHE --l2=$SHARED_CACHE -p1 --enclave=14 -m1024 --l2_partitioning=1 -g startenclaves.out > $OUTPUT_DIR/startenclaves.log
python3 tools/interprethistogram.py $OUTPUT_DIR/startenclaves.log $OUTPUT_DIR/startenclaveshistogram.csv
spike --ic=$INSTRUCTION_CACHE --dc=$DATA_CACHE --l2=$SHARED_CACHE -p1 -m1024 --l2_partitioning=1 bitcount.out > $OUTPUT_DIR/bitcount.log
spike --ic=$INSTRUCTION_CACHE --dc=$DATA_CACHE --l2=$SHARED_CACHE -p1 --enclave=1 -m1024 --l2_partitioning=1 bandwidth.out > $OUTPUT_DIR/bandwidth.log
spike --ic=$INSTRUCTION_CACHE --dc=$DATA_CACHE --l2=$SHARED_CACHE -p1 --enclave=1 -m1024 --l2_partitioning=1 latency.out > $OUTPUT_DIR/latency.log
for PARTITIONING_SCHEME in {0..2}
do
  for SETS_POWER in {6..12}
  do
    SHARED_CACHE_SETS=$((1<<SETS_POWER))
    spike --ic=$INSTRUCTION_CACHE --dc=$DATA_CACHE --l2=$SHARED_CACHE_SETS$CACHE_WAYS_AND_BLOCK_SIZE -p1 --enclave=1 -m1024 --l2_partitioning=$PARTITIONING_SCHEME aes.out > $OUTPUT_DIR/aes-$PARTITIONING_SCHEME-$SHARED_CACHE_SETS-.log
  done
done
python3 tools/interpretcache.py $OUTPUT_DIR/interpret_aes_cache.csv output/aes-*-.log

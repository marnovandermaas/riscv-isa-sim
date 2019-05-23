#caches specified with sets:ways:blocks
spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=1 -m1024 --l2_partitioning=1 hello.out
#spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 --enclave=90 -m1024 --l2_partitioning=1 startenclaves.out
spike --ic=16:1:8 --dc=16:1:8 --l2=128:1:8 -p1 -m1024 --l2_partitioning=1 bitcount.out

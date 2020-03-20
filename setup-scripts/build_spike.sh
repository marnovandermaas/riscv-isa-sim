cd ../riscv-tools
export RISCV=$PWD/toolchain-build
export PATH="$RISCV/bin:$PATH"
rm -rf riscv-isa-sim
cd ..
mkdir build
cd build
../configure --prefix=$RISCV --with-fesvr=$RISCV --enable-histogram
make
make install
cd ../setup-scripts

echo "Make sure to include the necessary packages (see README.md in riscv-tools)"
cd ..
git clone https://github.com/riscv/riscv-tools.git
cd riscv-tools
git checkout 86cfff3db09020650642cf789b313ab897208007
git submodule update --init --recursive
mkdir toolchain-build
export RISCV=$PWD/toolchain-build
echo "Attention: this build takes a long time to build, about 45 min on my machine."
./build.sh
cd ..
cd setup-scripts/

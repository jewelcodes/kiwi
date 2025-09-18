#!/bin/sh

export HOST="$(pwd)/host"
export CROSS="$(pwd)/cross"
export TARGET="x86_64-kiwi"
export PATH="$CROSS/bin:$PATH"

echo "Attempting to build a toolchain targeting $TARGET into $CROSS..."

mkdir -p downloads host cross gmp mpfr mpc binutils gcc
mkdir -p gcc/build

echo "Downloading gmp..."
curl https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz -o downloads/gmp.tar.xz
echo "Downloading mpfr..."
curl https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.1.tar.gz -o downloads/mpfr.tar.gz
echo "Downloading mpc..."
curl https://ftp.gnu.org/gnu/mpc/mpc-1.3.1.tar.gz -o downloads/mpc.tar.gz
echo "Downloading binutils..."
curl https://ftp.gnu.org/gnu/binutils/binutils-2.43.tar.gz -o downloads/binutils.tar.gz
echo "Downloading gcc..."
curl https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.gz -o downloads/gcc.tar.gz

echo "Extracting downloads..."
tar -xvJf downloads/gmp.tar.xz -C gmp
tar -xvzf downloads/mpfr.tar.gz -C mpfr
tar -xvzf downloads/mpc.tar.gz -C mpc
tar -xvzf downloads/binutils.tar.gz -C binutils
tar -xvzf downloads/gcc.tar.gz -C gcc

echo "Building gmp..."
cd gmp/gmp-6.3.0
./configure --prefix="$HOST"
make
make install

echo "Building mpfr..."
cd ../../mpfr/mpfr-4.2.1
./configure --prefix="$HOST" --with-gmp-include="$HOST/include" --with-gmp-lib="$HOST/lib"
make
make install 

echo "Building mpc..."
cd ../../mpc/mpc-1.3.1
./configure --prefix="$HOST" --with-gmp-include="$HOST/include" --with-gmp-lib="$HOST/lib"
make
make install

echo "Building binutils..."
cd ../../binutils/binutils-2.43
sed -i -e 's/haiku/kiwi/g' config.sub
sed -i -e 's/haiku/kiwi/g' bfd/config.bfd
sed -i -e 's/haiku/kiwi/g' gas/configure.tgt
sed -i -e 's/em=kiwi/ /g' gas/configure.tgt
sed -i -e 's/haiku/kiwi/g' ld/configure.tgt
sed -i -e 's/targ_extra_emuls="elf_x86_64 elf_i386_kiwi"/targ_extra_emuls="elf_x86_64 elf_i386_kiwi elf_i386"/g' ld/configure.tgt
cp ../../emulparams/* ld/emulparams/
sed -i -e 's/haiku/kiwi/g' ld/Makefile.am
cd ld
mkdir ldscripts
mkdir tmpdir
automake
cd ..
./configure --target="$TARGET" --prefix="$CROSS" --with-sysroot --disable-nls --disable-werror
make
make install

echo "Building gcc..."
cd ../../gcc/gcc-14.2.0
sed -i -e 's/haiku/kiwi/g' config.sub
sed -i -e '/tmake_file="t-darwin "/r ../../gcc-config/config.gcc.add1' gcc/config.gcc
sed -i -e '/tm_file="${tm_file} elfos.h newlib-stdint.h"/r ../../gcc-config/config.gcc.add2' gcc/config.gcc
cp ../../gcc-config/kiwi.h gcc/config/
sed -i -e '/case "${host}" in/r ../../gcc-config/crossconfig.m4.add' libstdc++-v3/crossconfig.m4
cd libstdc++-v3
autoconf
cd ..
sed -i -e '/\*-\*-darwin\*)/r ../../gcc-config/config.host.add' libgcc/config.host
sed -i -e '/aarch64\*-\*-freebsd\*)/r ../../gcc-config/config.host.add' libgcc/config.host
sed -i -e '/case $machine in/r ../../gcc-config/mkfixinc.sh.add' fixincludes/mkfixinc.sh
cd ../build
../gcc-14.2.0/configure --target="$TARGET" --prefix="$CROSS" --disable-nls --enable-languages=c,c++ --without-headers --with-gmp-include="$HOST/include" --with-gmp-lib="$HOST/lib"
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc

cd ../..

echo "Cleaning up..."
rm -rf downloads gmp mpfr mpc binutils gcc

echo
echo "gcc and binutils are now installed in $CROSS/bin"

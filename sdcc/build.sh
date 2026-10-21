#!/bin/sh
set -eu
V=4.6.0
SHA=5fd6a93e5997ce01756868fe35e441095cfb637894a80c262514a634094973b6
PREFIX=$1
D=$(cd "$(dirname "$0")" && pwd)
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
curl -fL -o "$W/sdcc.tar.bz2" "https://downloads.sourceforge.net/project/sdcc/sdcc/$V/sdcc-src-$V.tar.bz2"
if command -v sha256sum >/dev/null; then
	echo "$SHA  $W/sdcc.tar.bz2" | sha256sum -c -
else
	echo "$SHA  $W/sdcc.tar.bz2" | shasum -a 256 -c -
fi
tar -C "$W" -xjf "$W/sdcc.tar.bz2"
cd "$W/sdcc-$V"
for p in "$D"/*.patch; do
	patch -p0 < "$p"
done
off=""
for p in z80 z180 r2k r2ka r3ka r4k r5k r6k sm83 tlcs90 ez80 ez80_z80 z80n r800 ds390 ds400 pic14 pic16 hc08 s08 stm8 pdk13 pdk14 pdk15 mos6502 mos65c02 huc6280 f8 f8l; do
	off="$off --disable-$p-port"
done
./configure --prefix="$PREFIX" $off --disable-sdcdb --disable-ucsim --disable-non-free
make -j"$(getconf _NPROCESSORS_ONLN)"
make install

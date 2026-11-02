#!/bin/sh
# PC harness build, inside br-tools:1 with the project at /p:
#   MSYS_NO_PATHCONV=1 docker run --rm --label watchdog.max=600 -v "C:\New folder\batrider360:/p" br-tools:1 sh /p/pc/build.sh
set -e
B="${BR_BUILD:-/p/build/pc}"   # BR_BUILD: another output folder (a second build beside the main one)
G=/p/build/gen
mkdir -p "$B" "$G"
M=/p/third_party/musashi
# Musashi's opcode tables, generated once from m68k_in.c
if [ ! -f "$G/m68kops.c" ]; then
	gcc -O1 -o "$G/m68kmake" "$M/m68kmake.c"
	"$G/m68kmake" "$G" "$M/m68k_in.c"
fi
CF="${EXTRA_CF} -O2 -g -Wall -Wno-unused-function -DMUSASHI_CNF=\"m68kconf_br.h\" -I/p/core -I$M -I$G -I/p/third_party/z80 -I/p/third_party/ymfm/src -I/p/third_party/zlib -I$G"
# every core/*.c (new modules are picked up without editing this line)
for f in $(cd /p && ls core/*.c) third_party/z80/z80.c pc/harness.c; do
	gcc $CF -c "/p/$f" -o "$B/$(basename $f .c).o"
done
gcc $CF -w -c "$M/m68kcpu.c" -o "$B/m68kcpu.o"
gcc $CF -w -c "$G/m68kops.c" -o "$B/m68kops.o"
gcc $CF -w -c "$M/softfloat/softfloat.c" -o "$B/softfloat.o"
for z in adler32 crc32 deflate trees inffast inflate inftrees zutil; do gcc -O2 -w -c /p/third_party/zlib/$z.c -o "$B/z_$z.o"; done
g++ -O2 -g -std=c++14 -I/p/core -I/p/third_party/ymfm/src -c /p/core/ym.cpp -o "$B/ym.o"
g++ -O2 -g -std=c++14 -I/p/third_party/ymfm/src -c /p/third_party/ymfm/src/ymfm_opm.cpp -o "$B/ymfm_opm.o"
g++ -o "$B/harness" "$B"/*.o -lm
echo built "$B/harness"

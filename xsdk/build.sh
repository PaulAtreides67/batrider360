#!/bin/sh
# batrider360 .xex build (xsdk). From Git Bash on the PC:
#   MSYS_NO_PATHCONV=1 docker run --rm --memory=5g --label watchdog.max=3600 -v xsdk-gcc:/opt/xsdk-gcc \
#     -v "C:\New folder\batrider360:/p" -v "C:\New folder\xsdk-src\xsdk:/xsdk:ro" -v br-xsdk:/b \
#     xsdk-base:1 sh /p/xsdk/build.sh
# The build folder is the volume br-xsdk; batrider360.xex, its ELF and its symbols land in /p/xsdk/out.
set -e
B=/b/build
O=/p/xsdk/out
G=/p/build/gen
mkdir -p "$B" "$O" "$G"
if [ ! -f "$G/m68kops.c" ]; then
	gcc -O1 -o /tmp/m68kmake /p/third_party/musashi/m68kmake.c
	/tmp/m68kmake "$G" /p/third_party/musashi/m68k_in.c
fi
cmake -S /p/xsdk -B "$B" -G Ninja -DCMAKE_TOOLCHAIN_FILE=/xsdk/cmake/xsdk.cmake -DXSDK_TOOLCHAIN=/opt/xsdk-gcc \
	-DCMAKE_BUILD_TYPE=None
cmake --build "$B" -j "${JOBS:-6}"
cp "$B/batrider360.xex" "$O/"
cp "$B/batrider360" "$O/batrider360.elf"
/opt/xsdk-gcc/bin/powerpc-xsdk-elf-nm -n --defined-only "$O/batrider360.elf" > "$O/batrider360.syms"
ls -l "$O"

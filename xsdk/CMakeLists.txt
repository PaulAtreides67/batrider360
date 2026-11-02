# batrider360: Armed Police Batrider + CHASE! on the Xbox 360, an xsdk .xex (build.sh beside this file builds it in
# Docker). The board's emulation is ../core (+ ../third_party: Musashi, superzazu/z80, ymfm, zlib); Musashi's opcode
# tables come from ../build/gen (m68kmake, run by build.sh on the build machine).
cmake_minimum_required(VERSION 3.25)
project(batrider360 C CXX)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(TP "${ROOT}/third_party")
set(GEN "${ROOT}/build/gen")

# every core/*.c (test 4 adds modules: gadgets, the front end's pages, the HUD)
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS ${ROOT}/core/*.c)
set(THIRD_SOURCES ${TP}/musashi/m68kcpu.c ${GEN}/m68kops.c ${TP}/musashi/softfloat/softfloat.c ${TP}/z80/z80.c
	${TP}/zlib/adler32.c ${TP}/zlib/crc32.c ${TP}/zlib/deflate.c ${TP}/zlib/trees.c ${TP}/zlib/inffast.c
	${TP}/zlib/inflate.c ${TP}/zlib/inftrees.c
	${TP}/zlib/zutil.c)
set(CXX_SOURCES ${ROOT}/core/ym.cpp ${TP}/ymfm/src/ymfm_opm.cpp)

add_executable(batrider360 main.c ${CORE_SOURCES} ${THIRD_SOURCES} ${CXX_SOURCES})
set_target_properties(batrider360 PROPERTIES C_STANDARD 17 C_EXTENSIONS ON CXX_STANDARD 17)
target_include_directories(batrider360 PRIVATE ${ROOT}/core ${TP}/musashi ${GEN} ${TP}/z80 ${TP}/ymfm/src ${TP}/zlib)
target_compile_definitions(batrider360 PRIVATE "MUSASHI_CNF=\"m68kconf_br.h\"")
target_compile_options(batrider360 PRIVATE -O2 -g -fno-strict-aliasing -fwrapv)
set_source_files_properties(main.c ${CORE_SOURCES} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wno-unused-parameter")
set_source_files_properties(${THIRD_SOURCES} ${CXX_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
# Test 3: every log line through main.c's ring to its own thread (__wrap_xsdk_log_write).
target_link_options(batrider360 PRIVATE -Wl,--wrap=xsdk_log_write)
xsdk_add_shaders(batrider360 quad.vert quad.frag)
xsdk_add_xex(batrider360)

#!/bin/sh
# batrider360 release folder: the .xex, the manuals, the README, the third-party licences and (user, 2026-11-02, private
# release) the mame folder: the CHASE! program ROMs for MAME's batriderj + the GADGETS artwork for both games.
#   sh tools/mkrelease.sh <version>        (from Git Bash, in the project folder; e.g. 1.0)
# Makes release/batrider360_v<version>/ and release/batrider360_v<version>.zip from xsdk/out/batrider360.xex,
# release/manual/*.pdf and release/batrider360_manual.bin. The user supplies MAME's batrider.zip beside the .xex (README.txt says how).
set -e
V="$1"
[ -n "$V" ] || { echo "usage: sh tools/mkrelease.sh <version>"; exit 1; }
cd "$(dirname "$0")/.."
D="release/batrider360_v$V"
rm -rf "$D" "$D.zip"
mkdir -p "$D/licences"
cp xsdk/out/batrider360.xex "$D/"
cp release/manual/batrider360_manual_en.pdf release/manual/batrider360_manual_ja.pdf "$D/"
# test 8: the manual's pages for the screen (tools/mkmanual.py from the PDFs above)
cp release/batrider360_manual.bin "$D/"
sed "s/@VERSION@/$V/g" tools/release_readme.txt > "$D/README.txt"
cp third_party/musashi/readme.txt "$D/licences/musashi.txt"
cp third_party/ymfm/LICENSE "$D/licences/ymfm.txt"
cp third_party/z80/LICENSE "$D/licences/z80.txt"
sed -n '/Copyright/,/Mark Adler/p' third_party/zlib/zlib.h > "$D/licences/zlib.txt"
cp "/c/New folder/xsdk-src/xsdk/LICENSE" "$D/licences/xsdk.txt"
# the MAME folder (public release, 2026-11-06: NO ROM data): the GADGETS artwork for both games (batrider-chase
# out/artwork/batriderj, from tools/mame/mkgadgets_mame.py), the .bat launchers, and the CHASE! xdelta kit
# (tools/chase_xdelta: builds batriderj.zip from the user's own batrider.zip). roms\ is left for the user to fill.
mkdir -p "$D/mame/roms" "$D/mame/artwork"
cp -r "/c/New folder/batrider-chase/out/artwork/batriderj" "$D/mame/artwork/"
cp tools/mame_release/batrider_chase.bat tools/mame_release/batrider_original.bat tools/mame_release/README_MAME.txt "$D/mame/"
mkdir -p "$D/mame/chase_xdelta"
cp tools/chase_xdelta/README.txt tools/chase_xdelta/make_batriderj.bat tools/chase_xdelta/make_batriderj.ps1 tools/chase_xdelta/*.xdelta "$D/mame/chase_xdelta/"
# no ROM data may ship: refuse any zip or ROM-like file
if find "$D" \( -iname '*.zip' \) -o \( -iname '*.u2*' ! -name '*.xdelta' \) -o \( -iname '*.u7*' ! -name '*.xdelta' \) -o -iname 'rom-*' -o \( -iname '*.bin' ! -name batrider360_manual.bin \) | grep -q .; then
	echo "ROM-like file in $D: refusing"; exit 1
fi
(cd release && powershell.exe -NoProfile -Command "Compress-Archive -Path 'batrider360_v$V' -DestinationPath 'batrider360_v$V.zip'")
md5sum "$D/batrider360.xex"
ls -l "$D" "$D.zip"

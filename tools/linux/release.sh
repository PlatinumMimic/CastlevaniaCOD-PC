#!/bin/bash
# The Linux release, as release.bat makes the Windows one: Castlevania, CastlevaniaEU and the launcher without the game
# code. The executable of the player's release builds its library (<exe>.so) on its first start from the player's own
# default.xbe with the compiler folder (the SDK's runtime/linux/game_code.cpp): the recompiler, Clang and LLD (the LLVM
# project's own Linux build), the game's configs and the headers the code includes.
#   tools/linux/release.sh [version]   writes out/release/CastlevaniaCOD-PC-<version>-linux.tar.xz
# Builds in tools/linux/Containerfile's image (Ubuntu 22.04, so the release runs on glibc 2.35 and later) with podman.
# Needs both releases' default.xbe, as release.bat does: the USA one in assets, the European one in assets/eu (the
# build reads them for their SHA-1s; nothing of them is packed). Downloads, kept in $CV_CACHE (~/.cache/cvcod-release):
# vkd3d's and FFmpeg's sources, LLVM's Linux build, URW's P052; the build itself goes to $CV_WORK
# (~/.cache/cvcod-release/work).
set -euo pipefail
VKD3D=2.1
FFMPEG=7.1.2
LLVM=23.1.4
URW_FONTS=20200910
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)

if [ "${1:-}" != "--inside" ]; then
    VERSION=${1:-}
    SDK=${XBE_SDK:-$(cd "$REPO/../XboxXBE-Recomp" && pwd)}
    CACHE=${CV_CACHE:-$HOME/.cache/cvcod-release}
    WORK=${CV_WORK:-$CACHE/work}
    mkdir -p "$CACHE" "$WORK"
    [ -f "$REPO/assets/default.xbe" ] || { echo "  Put the game's default.xbe in assets first, see README.md."; exit 1; }
    [ -f "$REPO/assets/eu/default.xbe" ] || { echo "  Put the European release's default.xbe in assets/eu first."; exit 1; }
    echo "[1/5] Build machine"
    podman build -q -t cvcod-linux-build -f "$HERE/Containerfile" "$HERE" > /dev/null
    # the repository read only but out/, where the release goes; the SDK beside it, as CMakeLists.txt finds it
    mkdir -p "$REPO/out/release"
    exec podman run --rm --userns=keep-id -e "VERSION=$VERSION" \
        -v "$REPO:/src/CastlevaniaCOD-PC:ro" -v "$REPO/out/release:/out" -v "$SDK:/src/XboxXBE-Recomp:ro" \
        -v "$CACHE:/cache" -v "$WORK:/work" cvcod-linux-build bash /src/CastlevaniaCOD-PC/tools/linux/release.sh --inside
fi

SRC=/src/CastlevaniaCOD-PC
export CC=clang-21 CXX=clang++-21

echo "[2/5] vkd3d $VKD3D, FFmpeg $FFMPEG, LLVM $LLVM's clang and lld, the launcher's fonts"
cd /cache
[ -f vkd3d-$VKD3D.tar.xz ] || curl -fsSLO https://dl.winehq.org/vkd3d/source/vkd3d-$VKD3D.tar.xz
[ -f ffmpeg-$FFMPEG.tar.xz ] || curl -fsSLO https://ffmpeg.org/releases/ffmpeg-$FFMPEG.tar.xz
[ -f LLVM-$LLVM-Linux-X64.tar.zst ] || curl -fsSLO https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM/LLVM-$LLVM-Linux-X64.tar.zst
URW=https://github.com/ArtifexSoftware/urw-base35-fonts/raw/$URW_FONTS
for f in Roman Bold BoldItalic; do [ -f P052-$f.otf ] || curl -fsSLO $URW/fonts/P052-$f.otf; done
for f in LICENSE COPYING; do [ -f urw-base35-$f.txt ] || curl -fsSL -o urw-base35-$f.txt $URW/$f; done
if [ ! -f /work/vkd3d/lib/libvkd3d.so.1 ]; then
    rm -rf /work/vkd3d-src && mkdir -p /work/vkd3d-src && tar xf vkd3d-$VKD3D.tar.xz -C /work/vkd3d-src --strip-components=1
    (cd /work/vkd3d-src && ./configure -q --prefix=/work/vkd3d --disable-tests --disable-demos --disable-doxygen-doc \
        CFLAGS="-O2 -march=x86-64-v2" > /work/vkd3d-configure.log 2>&1 && make -s -j"$(nproc)" > /work/vkd3d-make.log 2>&1 &&
        make -s install > /dev/null) || { echo "      vkd3d failed, see $CV_WORK/vkd3d-*.log"; exit 1; }
fi
# FFmpeg for the HD movies (movies.cpp): LGPL, shared, the H.264 and HEVC decoders, the MP4 demuxer and swscale
# alone, nothing of the build machine's found and linked
if [ ! -f /work/ffmpeg/lib/libavcodec.so ]; then
    rm -rf /work/ffmpeg-src && mkdir -p /work/ffmpeg-src && tar xf ffmpeg-$FFMPEG.tar.xz -C /work/ffmpeg-src --strip-components=1
    (cd /work/ffmpeg-src && ./configure --prefix=/work/ffmpeg --cc=clang-21 --extra-cflags=-march=x86-64-v2 \
        --enable-shared --disable-static --enable-pic --disable-programs --disable-doc --disable-network \
        --disable-autodetect --disable-everything --disable-avdevice --disable-avfilter --disable-swresample \
        --disable-postproc --enable-decoder=h264,hevc --enable-parser=h264,hevc --enable-demuxer=mov \
        --enable-protocol=file --enable-swscale > /work/ffmpeg-configure.log 2>&1 &&
        make -j"$(nproc)" > /work/ffmpeg-make.log 2>&1 && make install > /dev/null 2>&1) ||
        { echo "      FFmpeg failed, see $CV_WORK/ffmpeg-*.log"; exit 1; }
fi
if [ ! -x /work/llvm/ld.lld ]; then
    rm -rf /work/llvm && mkdir -p /work/llvm
    # only clang and lld of it: the real files behind bin/clang and bin/ld.lld
    # (packed with zstd's long window)
    root=LLVM-$LLVM-Linux-X64
    clang=$(zstd -dq --long=31 -c LLVM-$LLVM-Linux-X64.tar.zst | tar -tf - | grep -E "^$root/bin/clang-[0-9]+$" | head -1)
    zstd -dq --long=31 -c LLVM-$LLVM-Linux-X64.tar.zst | tar -xf - -C /work/llvm --strip-components=2 "$clang" "$root/bin/lld"
    mv /work/llvm/"$(basename "$clang")" /work/llvm/clang && mv /work/llvm/lld /work/llvm/ld.lld
    llvm-strip-21 /work/llvm/clang /work/llvm/ld.lld
fi

echo "[3/5] Both releases' executables, the launcher and the compiler folder (no game code: no library is built)"
B=/work/build
# the libraries in lib beside the executables, theirs too (DT_RPATH). The C++ runtime is the system's: the game code's
# exceptions unwind through libgcc_s's _Unwind_Resume, which a static one hides, and LLVM's clang needs it anyway
LINK="-Wl,--disable-new-dtags"
cmake -S $SRC -B $B -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 \
    -DCMAKE_EXE_LINKER_FLAGS="$LINK" -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON -DXBE_VKD3D_DIR=/work/vkd3d \
    -DCV_FFMPEG_DIR=/work/ffmpeg -DCV_GAME_ICON=OFF \
    > /work/release_build_log.txt 2>&1 || { echo "      configuring failed, see $CV_WORK/release_build_log.txt"; exit 1; }
cmake --build $B --target Castlevania CastlevaniaEU CastlevaniaLauncher xbe_recomp Castlevania_hooks CastlevaniaEU_hooks \
    -- -k 0 >> /work/release_build_log.txt 2>&1 ||
    { echo "      the build failed, see release_build_log.txt in the work folder"; exit 1; }
VERSION=${VERSION:-$(cat $B/version.txt)}

echo "[4/5] Package"
PKG=/work/pkg/CastlevaniaCOD-PC
rm -rf /work/pkg && mkdir -p $PKG/assets $PKG/textures $PKG/lib $PKG/fonts
cp $B/Castlevania $B/CastlevaniaEU $B/CastlevaniaLauncher $PKG/
for l in libvkd3d.so.1 libvkd3d-shader.so.1 libvkd3d-utils.so.1; do cp -L /work/vkd3d/lib/$l $PKG/lib/; done
# FFmpeg's, by the names the executables ask for (libavcodec.so.61 and the like)
for l in avformat avcodec avutil swscale; do
    cp -L /work/ffmpeg/lib/$(cd /work/ffmpeg/lib && ls lib$l.so.* | awk -F. 'NF == 3') $PKG/lib/
done
# each library finds the others beside it: their RUNPATH was the build's folder, and a RUNPATH keeps the loader from
# using the executable's $ORIGIN/lib for a library's own libraries (the launcher needs libvkd3d.so.1 alone)
patchelf --set-rpath '$ORIGIN' $PKG/lib/*.so.*
# the compiler folder without its work folder, where the recompiled code is made
(cd $B/compiler && tar cf - --exclude=./work .) | (cd $PKG && mkdir compiler && tar xf - -C compiler)
cp /work/llvm/clang /work/llvm/ld.lld $PKG/compiler/
cp $B/fonts/MaterialIcons-Regular.ttf /cache/P052-*.otf $PKG/fonts/
cp $SRC/tools/release/README-linux.txt $PKG/README.txt
cp $SRC/LICENSE $PKG/LICENSE.txt
cat /src/XboxXBE-Recomp/THIRD_PARTY.txt $SRC/tools/linux/THIRD_PARTY-linux.txt /cache/urw-base35-LICENSE.txt \
    /cache/urw-base35-COPYING.txt /work/ffmpeg-src/COPYING.LGPLv2.1 > $PKG/THIRD_PARTY.txt
echo "Put your game here: its disc image (.iso), or the extracted disc (default.xbe with its folders)." > "$PKG/assets/PUT YOUR GAME HERE.txt"
echo "Optional: an HD texture pack made for PCSX2. Put its replacement pictures here (.png or .dds files, folders inside are fine)." > "$PKG/textures/PUT A TEXTURE PACK HERE.txt"
# files copied from a Windows drive come out executable: only the programs are (a file manager offers to run the rest)
find $PKG -type f -exec chmod 644 {} + && find $PKG -type d -exec chmod 755 {} +
chmod 755 $PKG/Castlevania $PKG/CastlevaniaEU $PKG/CastlevaniaLauncher $PKG/compiler/xbe_recomp $PKG/compiler/clang \
    $PKG/compiler/ld.lld
# nothing of the game goes out: no game code or its sources, no xbe or disc image
bad=$(find $PKG \( -name '*.xbe' -o -name '*.iso' -o -name '*.cpp' -o -name '*.o' -o -name 'x86_recomp*' -o \
    -name image_symbols.h -o -name Castlevania.so -o -name CastlevaniaEU.so \) -print)
[ -z "$bad" ] || { echo "      game files in the package: $bad"; exit 1; }
[ ! -e $PKG/compiler/work ] || { echo "      the compiler's work folder is in the package"; exit 1; }

echo "[5/5] Archive"
out=/out/CastlevaniaCOD-PC-$VERSION-linux.tar.xz
tar -C /work/pkg -cf - CastlevaniaCOD-PC | xz -T0 -6 > $out.tmp && mv $out.tmp $out
echo "      out/release/$(basename $out)  ($(stat -c %s $out) bytes)"

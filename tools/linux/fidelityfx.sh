#!/bin/bash
# AMD FSR 3's upscaler on Vulkan for the Linux release: the FidelityFX SDK 1.1.4's unofficial Linux fork
# (https://github.com/DethRaid/FidelityFX-SDK-Linux, MIT) at a fixed commit, with fidelityfx-linux.patch, its upscaler
# and Vulkan backend alone built as static libraries with clang, staged in <cache>/fidelityfx (include, bin/ffx_sdk) for
# the game's build (XBE_FSR_VK_DIR). Built on the host, not in release.sh's container: the fork's shader compiler
# (tools/binary_store/FidelityFX_SC, a prebuilt program) wants a newer C++ runtime than Ubuntu 22.04's. The libraries
# are then linked in the container, against its runtime, as the rest of the release is.
#   tools/linux/fidelityfx.sh <cache>     needs git, cmake, ninja and clang on the host
# The patch: the backend's scratch arrays aligned to 64 bytes (clang's aligned moves into them faulted), its descriptor
# pool capped at what drivers on D3D12 heaps take (Mesa's dozen: 2048 samplers; it asked 2,048,000), and its 16 bit
# shaders left out (it chose them by what the GPU has, but vkd3d's device need not have shaderFloat16 on).
set -euo pipefail
COMMIT=84aa556b9c4650930ecf047e67244ff152b2b4aa
HERE=$(cd "$(dirname "$0")" && pwd)
CACHE=$1
OUT=$CACHE/fidelityfx
STAMP=$OUT/.built-$COMMIT-$(sha256sum "$HERE/fidelityfx-linux.patch" | cut -c1-16)
[ -f "$STAMP" ] && exit 0
SRC=$CACHE/fidelityfx-src
if [ ! -d "$SRC/.git" ]; then
    rm -rf "$SRC"
    git clone -q https://github.com/DethRaid/FidelityFX-SDK-Linux.git "$SRC"
fi
git -C "$SRC" checkout -q -f $COMMIT
git -C "$SRC" clean -q -fdx
(cd "$SRC/sdk" && patch -s -p1 < "$HERE/fidelityfx-linux.patch")
B=$CACHE/fidelityfx-build
rm -rf "$B" && mkdir -p "$B"
off=""
for c in FI OF FSR1 FSR2 SPD CAS LPM BLUR CACAO DOF LENS PARALLEL_SORT SSSR VRS CLASSIFIER DENOISER BRIXELIZER BRIXELIZER_GI BREADCRUMBS; do
    off="$off -DFFX_$c=OFF"
done
(cd "$B" && CC=clang CXX=clang++ cmake -G Ninja "$SRC/sdk" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-march=x86-64-v2 \
    -DCMAKE_CXX_FLAGS=-march=x86-64-v2 -DFFX_API_BACKEND=VK_X64 -DFFX_FSR3=ON -DFFX_FSR3UPSCALER=ON $off \
    -DFFX_AUTO_COMPILE_SHADERS=ON > "$CACHE/fidelityfx-configure.log" 2>&1 && ninja > "$CACHE/fidelityfx-build.log" 2>&1) ||
    { echo "      the FidelityFX SDK failed, see $CACHE/fidelityfx-*.log"; exit 1; }
rm -rf "$OUT" && mkdir -p "$OUT/bin/ffx_sdk"
cp -r "$SRC/sdk/include" "$OUT/include"
cp "$SRC/sdk/bin/ffx_sdk/libffx_fsr3upscaler_x64.a" "$SRC/sdk/bin/ffx_sdk/libffx_backend_vk_x64.a" "$OUT/bin/ffx_sdk/"
cp "$SRC/LICENSE.txt" "$OUT/LICENSE.txt"
touch "$STAMP"

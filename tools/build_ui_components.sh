#!/usr/bin/env bash
set -euo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
PORT=${HERCULES_PORT_ROOT:-$REPO/local/edk2-sdm845}
SI=$REPO/source/simple-init
RUN=${HERCULES_BUILD_ROOT:-$REPO/local/build-ui}
EDK=$PORT/Common/edk2
PLATFORMS=$PORT/Common/edk2-platforms
[ -f "$EDK/edksetup.sh" ] || { echo 'Run tools/prepare_sources.py first.' >&2; exit 1; }
export WORKSPACE=$RUN/workspace
export PACKAGES_PATH=$EDK:$PLATFORMS:$SI:$PORT:$PORT/GPLDrivers
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
export CLANG38_AARCH64_PREFIX=aarch64-linux-gnu-
export CROSS_COMPILE=aarch64-linux-gnu-
export PATH=$REPO/tools/host-bin:$PATH
unset XNU_PAYLOAD
mkdir -p "$WORKSPACE/Conf" "$SI/build"
cp "$PORT/tools/tools_def.txt" "$WORKSPACE/Conf/tools_def.txt"
cp "$PORT/tools/build_rule.txt" "$WORKSPACE/Conf/build_rule.txt"
cp "$REPO/source/edk2-integration/HerculesUiOnly.dsc" "$PORT/Platform/Xiaomi/sdm845/HerculesUiOnly.dsc"
for catalog in "$SI"/po/*.po; do
  name=$(basename "$catalog" .po)
  mkdir -p "$SI/root/usr/share/locale/$name/LC_MESSAGES"
  msgfmt -o "$SI/root/usr/share/locale/$name/LC_MESSAGES/simple-init.mo" "$catalog"
done
bash "$SI/scripts/gen-rootfs-source.sh" "$SI" "$SI/build"
cd "$PORT"
set +u
source "$EDK/edksetup.sh"
set -u
make -C "$EDK/BaseTools" -j6
for module in src/main/SimpleInitMain.inf Platform/RenegadePkg/Application/Reboot2PayloadApp/Reboot2PayloadApp.inf; do
  build -s -n 6 -a AARCH64 -t CLANG38 -p "$PORT/Platform/Xiaomi/sdm845/HerculesUiOnly.dsc" -m "$module" -b RELEASE \
    -D HERCULES_SI_ROOT="$SI" -D HERCULES_PORT_ROOT="$PORT" -D FIRMWARE_VER=public-ui-v0.1.0 -D USE_UART=0 -D FIX_CLANG=1 -D NO_EXCEPTION_DISPLAY=0 \
    -D FD_BASE=0xCE000000 -D FD_SIZE=0x00700000 -D ENABLE_LINUX_UTILS=1
done
echo "UI components built under $WORKSPACE. No phone command was executed."

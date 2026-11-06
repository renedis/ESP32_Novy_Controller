#!/bin/bash
# ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0
# Boot guard end to end under Espressif's QEMU (ESP32-C3): the firmware's boot_guard.c and
# boot_guard_idf.c with the real partition table, NVS, otadata and the IDF 5.5.3 bootloader.
#   S1 crash loop, valid image in app1   -> third boot switches to app1 (hello_world stands in for 2.3)
#   S2 crash loop, app1 empty            -> no switch, the count goes on
#   S3 stable boot with "WiFi" from 2 s  -> count cleared and image proven at 60 s
#   S4 (--long) new image, never an IP     -> switches to app1 at 300 s
# QEMU's ESP32-C3 model cannot run an app from app1 (0x150000): it hangs or aborts in its GDMA model
# right after the bootloader loaded it, so the switch is checked up to "Loaded app ... 0x150000".
# usage: tests/qemu/run_boot_guard.sh [--long]   (about 3 minutes, 9 with --long)
# QEMU: $QEMU, or the qemu-riscv32 tarball that idf_tools.py downloaded to ~/.espressif/tools/dist.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
APP=$HERE/boot_guard
WORK=$HERE/build
mkdir -p "$WORK"
# ESP-IDF environment: the calling shell's (. $IDF_PATH/export.sh), else .local/idfenv.sh if present.
if ! command -v idf.py >/dev/null 2>&1 && [ -f "$ROOT/.local/idfenv.sh" ]; then
    source "$ROOT/.local/idfenv.sh" >/dev/null 2>&1
fi
command -v idf.py >/dev/null 2>&1 || { echo "run inside an ESP-IDF 5.5 environment (. \$IDF_PATH/export.sh)"; exit 2; }
PY=$(command -v python)

# ── QEMU binary (+ a stub libSDL2 when Homebrew's is missing; QEMU runs with -display none) ──
QEMU=${QEMU:-}
if [ -z "$QEMU" ]; then
    QEMU=$(ls "$HOME"/.espressif/tools/tools/qemu-riscv32/*/qemu/bin/qemu-system-riscv32 2>/dev/null | head -1)
fi
if [ -z "$QEMU" ]; then
    TAR=$(ls "$HOME"/.espressif/tools/dist/qemu-riscv32-softmmu-*-aarch64-apple-darwin.tar.xz 2>/dev/null | head -1)
    [ -n "$TAR" ] || { echo "qemu-riscv32 not found: python \$IDF_PATH/tools/idf_tools.py download qemu-riscv32"; exit 2; }
    mkdir -p "$WORK/qemu" && tar -xf "$TAR" -C "$WORK/qemu"
    QEMU=$WORK/qemu/qemu/bin/qemu-system-riscv32
fi
SDL=$(otool -L "$QEMU" 2>/dev/null | awk '/libSDL2/{print $1}')
if [ -n "$SDL" ] && [ ! -e "$SDL" ]; then
    mkdir -p "$WORK/stub"
    XCODE_SDK=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
    [ -d "$XCODE_SDK" ] && export SDKROOT=$XCODE_SDK      # CommandLineTools SDK is newer than its linker
    if [ ! -e "$WORK/stub/libSDL2-2.0.0.dylib" ]; then
        { echo '/* SDL is never called with -display none */'
          nm -u "$QEMU" | sed -n 's/^_\(SDL_[A-Za-z0-9_]*\)$/int \1(void) { return -1; }/p' | sort -u; } > "$WORK/stub/sdl_stub.c"
        cc -dynamiclib -o "$WORK/stub/libSDL2-2.0.0.dylib" "$WORK/stub/sdl_stub.c" -install_name "$SDL" \
           -compatibility_version 3201.0.0 -current_version 3201.8.0 || exit 2
    fi
    export DYLD_LIBRARY_PATH=$WORK/stub
fi

# ── Images ──
build() {   # build <dir> <extra defaults or ''>
    local defaults="sdkconfig.defaults"
    [ -n "$2" ] && defaults="$defaults;$2"
    rm -rf "$1"                                   # fresh sdkconfig from the defaults every time
    (cd "$APP" && idf.py -B "$1" -DSDKCONFIG="$1/sdkconfig" -DSDKCONFIG_DEFAULTS="$defaults" build >"$1.log" 2>&1) \
        || { echo "build failed, see $1.log"; exit 2; }
}
LONG=0
[ "${1:-}" = "--long" ] && LONG=1
echo "CONFIG_BGTEST_CRASH=n" > "$WORK/stable.defaults"
printf 'CONFIG_BGTEST_CRASH=n\nCONFIG_BGTEST_NO_IP=y\nCONFIG_BGTEST_SECONDS=330\n' > "$WORK/noip.defaults"
build "$WORK/crash" ""
build "$WORK/stable" "$WORK/stable.defaults"
[ $LONG = 1 ] && build "$WORK/noip" "$WORK/noip.defaults"
if [ ! -f "$WORK/hello/build/hello_world.bin" ]; then
    rm -rf "$WORK/hello" && cp -r "$IDF_PATH/examples/get-started/hello_world" "$WORK/hello"
    (cd "$WORK/hello" && idf.py set-target esp32c3 >"$WORK/hello.log" 2>&1 && idf.py build >>"$WORK/hello.log" 2>&1) \
        || { echo "hello_world build failed"; exit 2; }
fi
B=$WORK/crash
"$PY" - "$IDF_PATH/tools/idf_py_actions/qemu_ext.py" "$WORK/efuse.bin" <<'PYEOF'
import binascii, re, sys
t = open(sys.argv[1]).read()
blk = t[t.index("'esp32c3': QemuTarget("):t.index("'-global driver=esp32c3.gpio")]
open(sys.argv[2], "wb").write(binascii.unhexlify("".join(re.findall(r"'([0-9a-f]+)'", blk))))
PYEOF
flash() {   # flash <out> <app0.bin> <app1.bin or ''>
    local extra=()
    [ -n "$3" ] && extra=(0x150000 "$3")
    "$PY" -m esptool --chip esp32c3 merge_bin --output "$1" --fill-flash-size 4MB \
        0x0 "$B/bootloader/bootloader.bin" 0x8000 "$B/partition_table/partition-table.bin" \
        0xe000 "$B/ota_data_initial.bin" 0x10000 "$2" ${extra[@]+"${extra[@]}"} >/dev/null
}
run() {     # run <seconds> <flash> <uart log>
    "$QEMU" -M esp32c3 -drive file="$2",if=mtd,format=raw \
        -drive file="$WORK/efuse.bin",if=none,format=raw,id=efuse \
        -global driver=nvram.esp32c3.efuse,property=drive,value=efuse \
        -global driver=timer.esp32c3.timg,property=wdt_disable,value=true \
        -display none -monitor none -serial file:"$3" >/dev/null 2>&1 &
    local q=$!
    ( sleep "$1"; kill $q 2>/dev/null ) &
    local t=$!
    wait $q 2>/dev/null
    kill $t 2>/dev/null
    wait $t 2>/dev/null                           # no "Terminated" job notice
}

FAIL=0
expect() {  # expect <log> <description> <regex>...: the regexes must match in this order
    local log=$1 what=$2 from=1 n
    shift 2
    for re in "$@"; do
        n=$(tail -n +$from "$log" | grep -n -E -m1 -- "$re" | cut -d: -f1)
        if [ -z "$n" ]; then echo "FAIL $what: missing /$re/ (log $log)"; FAIL=1; return; fi
        from=$((from + n))
    done
    echo "PASS $what"
}
reject() {  # reject <log> <description> <regex>
    if grep -q -E -- "$3" "$1"; then echo "FAIL $2: unexpected /$3/"; FAIL=1; else echo "PASS $2"; fi
}

HELLO=$WORK/hello/build/hello_world.bin
flash "$WORK/s1.bin" "$WORK/crash/boot-guard-qemu.bin" "$HELLO"
run 40 "$WORK/s1.bin" "$WORK/s1.log"
expect "$WORK/s1.log" "S1 crash loop switches to the other slot on the third boot" \
    'BGTEST boot: running=app0 fast_boots=1 ' 'BGTEST crash' \
    'BGTEST boot: running=app0 fast_boots=2 ' 'BGTEST crash' \
    'bootguard: app1: hello_world .* verifies' 'Too many boots ending within 60 s: booting app1' \
    'rst:0xc \(RTC_SW_CPU_RST\)' 'Loaded app from partition at offset 0x150000'
reject "$WORK/s1.log" "S1 the third boot never reaches the crash" 'BGTEST boot: running=app0 fast_boots=3 '

flash "$WORK/s2.bin" "$WORK/crash/boot-guard-qemu.bin" ""
run 25 "$WORK/s2.bin" "$WORK/s2.log"
expect "$WORK/s2.log" "S2 without a usable other slot the guard stays and keeps counting" \
    'fast_boots=1 ' 'fast_boots=2 ' 'bootguard: app1: no app description' 'fast_boots=3 ' 'fast_boots=4 '
reject "$WORK/s2.log" "S2 never switches" 'booting app1'

flash "$WORK/s3.bin" "$WORK/stable/boot-guard-qemu.bin" "$HELLO"
run 80 "$WORK/s3.bin" "$WORK/s3.log"
expect "$WORK/s3.log" "S3 a stable boot clears the count and proves the image at 60 s" \
    'BGTEST boot: running=app0 fast_boots=1 proven=0' 'BGTEST t59: running=app0 fast_boots=1 proven=0' \
    'Firmware confirmed \(60 s up with WiFi\)' 'BGTEST t61: running=app0 fast_boots=0 proven=1' 'BGTEST done'

if [ $LONG = 1 ]; then
    flash "$WORK/s4.bin" "$WORK/noip/boot-guard-qemu.bin" "$HELLO"
    run 360 "$WORK/s4.bin" "$WORK/s4.log"
    expect "$WORK/s4.log" "S4 a new image without network switches at 300 s" \
        'BGTEST boot: running=app0 fast_boots=1 proven=0' 'BGTEST t61: running=app0 fast_boots=0 proven=0' \
        'No WiFi for 300 s on a new firmware, switching to the other slot' 'No network: booting app1' \
        'BGTEST restart: boot guard: no network' 'Loaded app from partition at offset 0x150000'
fi

exit $FAIL

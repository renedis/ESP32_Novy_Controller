<!-- ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 -->
# Boot guard under QEMU

`run_boot_guard.sh` builds `boot_guard/` (the firmware's `main/boot_guard.c` and
`main/boot_guard_idf.c` with a crash or tick loop, no WiFi), puts it into the real partition table
with the IDF 5.5.3 bootloader and runs it in Espressif's QEMU (ESP32-C3), with ESP-IDF's
`hello_world` as the image in app1 (stand-in for the Arduino firmware).

| Scenario | Expected |
|---|---|
| S1 crash loop, valid app1 | boots 1 and 2 crash, boot 3 verifies app1, sets it and restarts; the bootloader loads app1 |
| S2 crash loop, empty app1 | no switch, the count keeps rising |
| S3 stable, IP from 2 s | count 1 until 60 s, then 0 and proven (app marked valid) |
| S4 (`--long`) no IP ever | at 300 s the never-proven image switches to app1 |

Limits of the emulator, not of the firmware:

- A WiFi app does not start on QEMU's C3: the ADC2 self-calibration that WiFi links in before
  `app_main` waits for an ADC the model does not have. Hence the separate test app.
- QEMU's C3 model does not run an app from app1 (0x150000): right after the bootloader loaded it,
  it hangs or aborts in its GDMA model. The switch is therefore checked up to the bootloader line
  `Loaded app from partition at offset 0x150000`.

QEMU: `$QEMU`, an installed `qemu-riscv32` tool, or the tarball in `~/.espressif/tools/dist`
(`python $IDF_PATH/tools/idf_tools.py download qemu-riscv32`). The arm64 build links Homebrew's
SDL2; when it is missing the script builds a stub library in `tests/qemu/build/stub` (QEMU runs with
`-display none`).

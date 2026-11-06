# ESP32 Novy Controller (ESP-IDF) Implementation Plan

**Goal:** Port the NOVY + ceiling fan RF controller (Arduino firmware 2.3 on an ESP32-C3) to
ESP-IDF 5.5.3, with identical RF, MQTT and Home Assistant behaviour, an app-level rollback guard
and the Arduino flash layout, so the first install goes over ArduinoOTA.

**Architecture:** pure C modules for every decision (RF encoder and command sequences, discovery
payloads, device and light state, LED priorities, boot guard, backoff, event log, status JSON,
settings validation, peer list), host-tested with clang; thin ESP-IDF modules for RMT, led_strip,
WiFi, MQTT, HTTP, NVS and OTA.

**Spec:** `docs/specs/2026-11-06-novy-rf-design.md`

**Conventions for every task:**
- Branch `master`.
- Header `ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0` on every source file
  (`/* */`, `<!-- -->` or `#`).
- Conventional Commits.
- Pure modules: write the host test first, run it and see it fail, then implement until
  `make -C tests/host` passes.
- A new `main/*.c` is added to `SRCS` in `main/CMakeLists.txt` in the same commit.
- Build: ESP-IDF 5.5.3 environment in the shell (`. $IDF_PATH/export.sh`), `idf.py set-target
  esp32c3` (once), `idf.py build`, `idf.py size`.
- Own values (WiFi, MQTT, device name, RF codes, captures) stay in git-ignored files:
  `sdkconfig.secrets` and `.local/`.
- Device access during this plan is read-only (HTTP GET, `mosquitto_sub`); no flashing, OTA,
  reboots or publishes.

> **Execution notes (2026-11-06):** deviations found while executing; the repository is the
> source of truth.
> - rc-switch `setProtocol(12)` after `setPulseLength(350)` puts 320 µs on air, and NOVY sends keep
>   the fan's `setRepeatTransmit(3)`: replicated, and proven by `test_reference.cpp` (2288 presses,
>   seven deliberate mutations of the firmware code all caught).
> - The 2.3 light entity never worked (swapped topics, no handler, `unknown` in HA). The discovery
>   stays byte-equal; the firmware now serves the topics HA actually uses (spec, RGB LED section).
> - The Arduino core 3.x bootloader has rollback enabled and ArduinoOTA marks the new image `NEW`,
>   so the app must confirm itself (60 s with WiFi, or a web action). The boot guard keeps one entry
>   per slot with a `failed` mark, so it never switches back to an image it left after a crash loop.
> - `test_reference` links `obj/ref_rcswitch_lib.o`: `obj/RCSwitch.o` and `obj/rcswitch.o` are the
>   same file on a case-insensitive file system.
> - Extra: `tests/qemu/run_boot_guard.sh` runs the boot guard under Espressif's QEMU. QEMU cannot
>   start a WiFi app on the C3 nor run an app from app1; see `tests/qemu/README.md`.
> - Added after the first release: the peers menu (`peers.c`, `peers_web.c`, `web/peers.js`) with
>   labels in capitals.
> - Public release: the generic Novy constants (`rf_novy.h`) are tracked, the own channel and fan
>   codes come from `.local/rf_codes.h` (else `rf_codes_example.h`); the tracked fixtures are
>   example copies written by `tools/make_public_fixtures.py`, and `make -C tests/host local` runs
>   the tests on the real captures in `.local/`.

## File structure

| Path | Responsibility |
|---|---|
| `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv` | Project, optional `sdkconfig.secrets` and `.local/sdkconfig.local`, size options, Arduino partition table |
| `main/Kconfig.projbuild` | Secrets defaults, device name, setup AP, static IP defaults |
| `main/idf_component.yml` | `espressif/mdns`, `espressif/led_strip` |
| `main/rcswitch.*`, `main/rf_cmd.*`, `main/rf_novy.h`, `main/rf_codes_example.h`, `main/rf_wave.*` | RF encoder (pure); own codes from `.local/rf_codes.h` |
| `main/devstate.*`, `main/discovery.*`, `main/light.*` | MQTT payloads and state (pure) |
| `main/led_logic.*`, `main/boot_guard.*`, `main/backoff.*`, `main/evlog.*`, `main/status_fmt.*` | Decisions and formats (pure) |
| `main/config_types.h`, `main/config_validate.*`, `main/net_config.*`, `main/peers.*` | Settings and peer list (pure) |
| `main/boot_guard_idf.*`, `main/nvs_config.*`, `main/wifi_mgr.*`, `main/time_sync.*`, `main/restart.*` | ESP-IDF glue |
| `main/rf_tx.*`, `main/status_led.*`, `main/mqtt_pub.*`, `main/web_server.*`, `main/peers_web.*`, `main/main.c` | ESP-IDF glue |
| `main/web/index.html`, `main/web/config.html`, `main/web/peers.js` | Pages (embedded) |
| `tests/host/Makefile`, `tests/host/check.h`, `tests/host/test_*.c(pp)` | Host tests (public and local mode) |
| `tests/host/fixtures/*` | MQTT and HTTP captures and the RF part of `config.h` (example copies) |
| `tests/host/ref/*` | Verbatim rc-switch 2.6.4, Arduino shim, sketch extract |
| `tools/make_secrets.py`, `tools/make_rf_codes.py`, `tools/ota_upload.py`, `tools/extract_ref.py`, `tools/make_public_fixtures.py` | Helpers |

## Task 1: Scaffold

Files: `.gitignore`, `CMakeLists.txt` (`PROJECT_VER 3.0.0`, optional `sdkconfig.secrets`),
`partitions.csv`, `sdkconfig.defaults`, `main/CMakeLists.txt`, `main/idf_component.yml`,
`main/Kconfig.projbuild`, `main/main.c` (empty `app_main`), `LICENSE`.

- [ ] `idf.py set-target esp32c3 && idf.py build` succeeds.
- [ ] `idf.py partition-table` prints the six partitions of the spec.
- [ ] Commit `chore: ESP-IDF scaffold for ESP32-C3 with the Arduino partition layout`.

## Task 2: Host harness, fixtures and reference code

- [ ] `tests/host/check.h` and `Makefile` (C tests per module, C++ reference test, cJSON from
  `$(IDF_PATH)/components/json/cJSON`, `check-sources` target, Xcode SDK workaround).
- [ ] `tools/extract_ref.py` writes `config_rf.h` (RF part of `config.h`, no secrets) and
  `sketch_ref.inc` (enum, cycle index array, transmitter, state, button table, declarations,
  `toLowerCase`, `handleButtonPress`, `getButtonNameByCommand`, `sendRFCommand`,
  `sendFanRFCommand`, `publishMQTTStatus`, `mqttCallback`, the three transmitter lines of
  `setup()`).
- [ ] Copy `RCSwitch.cpp/.h` 2.6.4 verbatim to `tests/host/ref/`; write `ref/Arduino.h` (String,
  pins, delays, interrupts).
- [ ] Store the MQTT capture as `discovery.txt` and `state_topics.txt`; write `http_live.txt` from
  the GET capture.
- [ ] Commit `test: host harness, captures and verbatim rc-switch/sketch reference`.

## Task 3: rc-switch transmit emulation (`rcswitch`)

Test first (`test_rcswitch.c`): constructor defaults (repeat 10, protocol 1, 350 µs, pin -1);
`set_protocol(12)` after `set_pulse_length(350)` gives 320 µs; out-of-range protocol falls back
to 1; `send` with pin -1 emits nothing; bit order MSB first; inverted levels; final LOW; string
sends treat every non-`0` as 1.

- [ ] Implement `main/rcswitch.c/.h`. Commit `feat: rc-switch transmit emulation`.

## Task 4: RF waveform recorder and RMT symbols (`rf_wave`)

Test first: merge of equal levels, zero-length steps dropped, final level kept, total duration,
symbol packing (pairs, odd tail, split above 32767 µs), overflow flag.

- [ ] Implement `main/rf_wave.c/.h`. Commit `feat: RF waveform recorder and RMT symbol packing`.

## Task 5: Command table and sketch sequences (`rf_cmd`)

Test first (`test_rf_cmd.c`): table names, ids, icons, paths, replies; lookups; NOVY code words;
fan full codes; per-command cycle wrap; repeat count 10 → 3 after a fan command; log texts.
Reference test (`test_reference.cpp`): all commands fresh, 2000-command sequence, hand-computed
frames, button table equality, pin equality.

- [ ] Implement the RF codes header and `main/rf_cmd.c/.h`.
- [ ] Commit `feat: RF command table and sketch-equal send sequences`.

## Task 6: Device state and press messages (`devstate`)

Test first: defaults, every command's state change, the six messages of a press, the
`state_topics.txt` formats; reference test compares web-press messages with `handleButtonPress()`
and checks that `mqttCallback()` messages are the tail of ours.

- [ ] Implement `main/devstate.c/.h`. Commit `feat: tracked device state and press messages`.

## Task 7: Discovery (`discovery`)

Test first: 23 topics, each payload byte-equal and `cJSON_Compare`-equal to the device's lines of
`fixtures/discovery.txt`; none of the stale entries of an older device name; buffer-too-small
handling.

- [ ] Implement `main/discovery.c/.h`. Commit `feat: Home Assistant discovery equal to firmware 2.3`.

## Task 8: Light, LED logic, backoff, event log, status formats

Tests first for each: `light` (ON/OFF, brightness, rgb parsing and rejects, state JSON, scaled
output); `led_logic` (every priority row of the spec, timings); `backoff` (1, 2, 4 … 60, reset);
`evlog` (format, 50-line ring, clear, truncation); `status_fmt` (RSSI and heap helpers equal to the
sketch functions, JSON byte-equal to `fixtures/http_live.txt`).

- [ ] Implement each module; one commit per module (`feat: ...`).

## Task 9: Boot guard decisions (`boot_guard`)

Test first: fresh record, same image increments, other image resets, threshold 3 with usable /
unusable other slot (lazy check), count cap, stable clear, proven flag, network deadline only for
unproven images, record validation (magic).

- [ ] Implement `main/boot_guard.c/.h`. Commit `feat: boot guard decision logic`.

## Task 10: Settings types and validation

`config_types.h`, `net_config.c`, `config_validate.c`: DHCP default, hostname from
`CONFIG_NOVY_DEVICE_NAME`, 20 dBm, MQTT prefix `ESP`, static IP defaults from Kconfig.

- [ ] Commit `feat: settings types, defaults and validation`.

## Task 11: ESP-IDF services

- [ ] `nvs_config` (namespace `novyrf`, pending network, setup AP keys), `restart` (+ hook list),
  `time_sync`: commit `feat: NVS settings, restart hooks and SNTP`.
- [ ] `wifi_mgr` with the hostname from settings and connection events for the event log: commit
  `feat: WiFi manager with DHCP default, strongest AP, setup AP and mDNS`.
- [ ] `boot_guard_idf`: commit `feat: boot guard on NVS and OTA slots`.
- [ ] `rf_tx` (RMT channel, queue, worker, restart hook): commit `feat: RMT RF transmitter`.
- [ ] `status_led` (led_strip, 50 Hz task): commit `feat: status LED`.
- [ ] `mqtt_pub` (client id, LWT, keepalive 15, subscriptions, deferred discovery, backoff,
  publish queue, light): commit `feat: MQTT with deferred discovery, backoff, buttons and light`.

## Task 12: Web

- [ ] `web/index.html` (dashboard, `<nav id="peers"></nav>`), `web/config.html` (Network, MQTT, RF,
  console, OTA): commits `feat: dashboard page`, `feat: settings page`.
- [ ] `web_server` (2.3 endpoints from the command table, 404/405 → `Not found`, JSON API, OTA with
  project check, console ring): commit `feat: web server with the 2.3 endpoints, JSON API and OTA`.

## Task 13: main and size

- [ ] `main.c`: boot guard first, TWDT, LED, settings, network, SNTP, RF, web, MQTT; 1 s loop for
  stability (60 s), proven mark, network deadline (300 s), heap log (5 min), WiFi events.
- [ ] `idf.py build`, `idf.py size`: image ≤ 1,179,648 bytes. Commit `feat: init order, stability
  timers and app-valid marking`.

## Task 14: Tools and README

- [ ] `tools/make_secrets.py` (from a `config.h` given as argument, keeps an existing setup AP
  password, prints no secret), `tools/ota_upload.py` (`--force`): commit `chore: secrets and OTA
  helpers`.
- [ ] README with migration command and post-flash checklist: commit `docs: README`.

## Migration (not part of this plan's execution)

Done by the owner after review, from a computer in the same network:
`python3 espota.py -i 192.168.1.50 -p 3232 -a <OTA password> -f build/esp32-novy-rf.bin`, then the
README checklist.

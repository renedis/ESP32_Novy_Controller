# ESP32 Novy Controller (NOVY hood + ceiling fan, ESP-IDF) — Design

Date: 2026-11-06
Status: approved

## Goal

Replace the Arduino firmware 2.3 of this project (ESP32-C3 with a 433 MHz transmitter) with ESP-IDF
firmware. The device sends 433 MHz OOK commands to a NOVY cooker hood and a ceiling fan. On air, on
MQTT and in Home Assistant nothing changes: the same RF waveforms, the same topics, the same
discovery payloads and the same entities. The web interface, network handling and update path are
new.

## Sources

| Source | Use |
|---|---|
| Arduino sketch 2.3 (with ceiling fan support) and its `config.h` | behaviour to keep, RF pin and codes |
| rc-switch 2.6.4 | RF encoder the sketch links |
| MQTT and HTTP captures of a running 2.3 device | test fixtures (public copies with example names and codes) |

Retained discovery topics of older firmware under another device name may still exist on a broker;
this firmware never publishes them.

## Hardware

| Item | Value |
|---|---|
| Board | ESP32-C3 SuperMini, 4 MB flash |
| RF transmitter | STX882, DATA on GPIO4 (`RF_TX_GPIO`), 3.3 V |
| Status LED | one WS2812 (GRB) on GPIO8 (`CONFIG_NOVY_LED_GPIO`) |
| Bootloader | the one written by the Arduino upload; this firmware never replaces it |

## Base and repository

- ESP-IDF 5.5.3, target `esp32c3`.
- GPL-3.0, header `ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0` on every source file.
  `tests/host/ref/RCSwitch.*` are verbatim rc-switch 2.6.4 files (LGPL-2.1-or-later, test only).
- Managed components: `espressif/mdns` 1.13.1, `espressif/led_strip` 3.x.
- Own values never live in the repository:
  - `sdkconfig.secrets`: WiFi, MQTT, setup AP password (`tools/make_secrets.py` from a `config.h`);
  - `.local/sdkconfig.local`: device name, setup AP SSID, static IP defaults;
  - `.local/rf_codes.h`: hood channel and fan remote (`tools/make_rf_codes.py`); without it the
    build uses `main/rf_codes_example.h` (made-up fan codes);
  - `.local/local.mk`, `.local/fixtures/`: the real captures for `make -C tests/host local`.
  CMake adds the first two to `SDKCONFIG_DEFAULTS` and defines `RF_CODES_H` for the third when they
  exist.

## Modules (`main/`)

| File | Responsibility | Host-tested |
|---|---|---|
| `rcswitch.c/.h` | Transmit half of rc-switch 2.6.4: protocol table, setters with the original side effects, `send()` | yes |
| `rf_cmd.c/.h`, `rf_novy.h`, `rf_codes_example.h` | Command table (22 buttons), sketch-equal send sequences, fan cycle indexes, log text; generic Novy constants; example RF codes | yes |
| `rf_wave.c/.h` | Records `digitalWrite`/`delayMicroseconds` steps, merges them, packs RMT symbols | yes |
| `devstate.c/.h` | Tracked state (novy_light, fan_speed, fan_light, fan_direction) and the MQTT messages of a press | yes |
| `discovery.c/.h` | Home Assistant discovery topics and payloads, byte-equal to the Arduino output | yes |
| `light.c/.h` | HA light entity: command parsing, state JSON, LED output | yes |
| `led_logic.c/.h` | Status LED priorities and colours | yes |
| `boot_guard.c/.h` | Boot counter and rollback decisions | yes |
| `backoff.c/.h` | MQTT reconnect backoff 1 s → 60 s | yes |
| `evlog.c/.h` | Event log ring in the Arduino `/log` format | yes |
| `status_fmt.c/.h` | `/heap`, `/rssi`, `/mqtt-status` values and JSON, byte-equal to the Arduino output | yes |
| `net_config.c/.h`, `config_validate.c/.h`, `config_types.h` | Settings validation and defaults | yes |
| `peers.c/.h` | Peer list: parse, validate, serialise | yes |
| `boot_guard_idf.c/.h` | NVS record, OTA slot checks, slot switch, app-valid marking | QEMU |
| `applog.c/.h` | Thread-safe event log: ESP-IDF log plus the `evlog` ring | no |
| `nvs_config.c/.h` | Settings in NVS namespace `novyrf`, Kconfig defaults | no |
| `wifi_mgr.c/.h` | STA DHCP/static, safe apply, strongest-AP scan, retry forever, setup AP, mDNS | no |
| `time_sync.c/.h`, `restart.c/.h` | SNTP, deferred restart with hooks | no |
| `rf_tx.c/.h` | RF job queue and worker, RMT TX channel | no |
| `status_led.c/.h` | led_strip driver and LED task | no |
| `mqtt_pub.c/.h` | MQTT client, LWT, subscriptions, discovery, presses, light | no |
| `web_server.c/.h`, `web/index.html`, `web/config.html` | Arduino endpoints, JSON API, pages, OTA, console | no |
| `peers_web.c/.h`, `web/peers.js` | Peer list in NVS key `peers`, `GET /peers.js`, `GET/POST /api/peers`, navbar drop-down | no |
| `main.c` | Boot guard first, init order, stability timers | no |

All RF, MQTT payload, state, LED and rollback decisions sit in host-tested modules. The ESP-IDF
modules move events between them and the peripherals.

## RF

### rc-switch semantics as the sketch uses them

`RCSwitch()` sets `nRepeatTransmit = 10` and protocol 1. `setProtocol(n)` copies the whole table
entry **including the pulse length**; `setPulseLength()` only changes the pulse length;
`setRepeatTransmit()` is never reset. `send(code, len)` sends `nRepeatTransmit` frames (bits MSB
first, then sync) and ends with `digitalWrite(LOW)`. Each pulse is `digitalWrite(first)`,
`delayMicroseconds(pulse * high)`, `digitalWrite(second)`, `delayMicroseconds(pulse * low)`; an
inverted protocol starts low.

| Sketch call sequence | Effective on air |
|---|---|
| boot: `enableTransmit(pin)`, `setPulseLength(350)`, `setProtocol(12)` | protocol 12, **320 µs** (the 350 is overwritten) |
| NOVY: `enableTransmit(pin)`, `setPulseLength(350)`, `setProtocol(12)` | protocol 12, 320 µs, repeat count unchanged |
| Fan: `enableTransmit(pin)`, `setProtocol(1)`, `setPulseLength(248)`, `setRepeatTransmit(3)` | protocol 1, 248 µs, 3 frames per `send()` |

**Repeat-count state (kept on purpose):** NOVY commands never reset `nRepeatTransmit`, so a NOVY
`send()` emits 10 frames until the first fan command after boot and 3 frames from then on.

### Timings

| Protocol | Pulse | "0" | "1" | Sync | Level order |
|---|---|---|---|---|---|
| 12 (NOVY) | 320 µs | L 320, H 640 | L 640, H 320 | L 11520, H 320 | inverted: low first |
| 1 (fan) | 248 µs | H 248, L 744 | H 744, L 248 | H 248, L 7688 | high first |

The line idles low before and after every transmission.

### Commands

NOVY code word = `NOVY_DEVICE_CODE[NOVY_CHANNEL - 1]` (4 bits, `<ch>`) + `NOVY_PREFIX` (`0101`) +
command, sent with `send(const char*)` `repeatCount` times in a row:

| # | Button | Code word | `send()` calls | Frames before / after the first fan command |
|---|---|---|---|---|
| 0 | Light | `<ch>01010111010001` | 2 | 20 / 6 |
| 1 | Power | `<ch>01010111010011` | 1 | 10 / 3 |
| 2 | Plus | `<ch>01010101` | 1 | 10 / 3 |
| 3 | Minus | `<ch>01010110` | 1 | 10 / 3 |
| 4 | Novy | `<ch>01010100` | 1 | 10 / 3 |
| 5 | Brightness | `<ch>01010111010001` | 4 | 40 / 12 |
| 6 | Brightness-low | `<ch>01010111010001` | 10 | 100 / 30 |

Fan code = `(KNOWN_DEVICE_ID << 12) | counter`, sent with `send(code, 32)` (3 frames). `counter`
comes from the command's 8-entry cycle array; every command has its own cycle index
(`fanCycleIndex[cmd - 7]`), which starts at 0 at boot and advances after each send.

| # | Button | Cycle array | # | Button | Cycle array |
|---|---|---|---|---|---|
| 7 | Fan-All-Off | `ALL_OFF_CODES` | 15 | Fan-Speed3 | `SPEED_3_CODES` |
| 8 | Fan-Off | `FAN_OFF_CODES` | 16 | Fan-Speed4 | `SPEED_4_CODES` |
| 9 | Fan-Light | `LIGHT_TOGGLE_CODES` | 17 | Fan-Speed5 | `SPEED_5_CODES` |
| 10 | Fan-Forward | `FAN_FORWARD_CODES` | 18 | Fan-Speed6 | `SPEED_6_CODES` |
| 11 | Fan-Reverse | `FAN_REVERSE_CODES` | 19 | Fan-Timer1H | `TIMER_1H_CODES` |
| 12 | Fan-Wave | `FAN_WAVE_CODES` | 20 | Fan-Timer2H | `TIMER_2H_CODES` |
| 13 | Fan-Speed1 | `SPEED_1_CODES` | 21 | Fan-Timer4H | `TIMER_4H_CODES` |
| 14 | Fan-Speed2 | `SPEED_2_CODES` | | | |

`RF_TX_GPIO`, `NOVY_CHANNEL`, `KNOWN_DEVICE_ID` and the cycle arrays come from `.local/rf_codes.h`
(or the example header); the generic Novy constants from `rf_novy.h`.

### Implementation

- `rcswitch.c` reimplements the transmit half of rc-switch with the same state and side effects;
  `rf_cmd.c` replays the sketch's `setup()`, `sendRFCommand()` and `sendFanRFCommand()` against it.
- `rf_wave.c` records every `digitalWrite(level)` + `delayMicroseconds(us)` step, merges equal
  levels, drops zero-length steps and packs the result into RMT symbols (1 µs ticks; a step longer
  than 32767 µs is split).
- `rf_tx.c` owns one RMT TX channel (1 MHz, 48-symbol block, idle and end level low) and a copy
  encoder. A whole command (up to 1900 symbols) is built in a static buffer and sent as one
  transaction, so WiFi interrupts cannot stretch pulses. `CONFIG_RMT_TX_ISR_CACHE_SAFE` keeps the
  refill interrupt running during flash writes (NVS, OTA).
- Commands from web and MQTT go into one FIFO queue (16 jobs) served by one worker, in order, like the
  sketch's blocking calls. The longest command (Brightness-low, 100 frames) takes 2.9 s.

### Verification

`tests/host/test_reference.cpp` compiles rc-switch 2.6.4 verbatim (`tests/host/ref/RCSwitch.*`) and
the sketch's own RF, button and press functions (`sketch_ref.inc`, extracted verbatim from the `.ino`
by `tools/extract_ref.py`) against fake `digitalWrite`/`delayMicroseconds`, and checks for every
command, every cycle step and both repeat-count states that the firmware emits the identical
(level, µs) sequence, log line and MQTT messages. The public fixtures carry the example names and
codes (`tools/make_public_fixtures.py`); `make -C tests/host local` runs the same tests on the real
captures and codes in `.local/` and checks that they still match the Arduino sources.

## Device state

As the sketch's `handleButtonPress()`: `Light` toggles `novy_light`; `Fan-Light` toggles `fan_light`;
`Fan-All-Off` sets speed 0 and fan light off; `Fan-Off` sets speed 0; `Fan-SpeedN` sets speed N;
`Fan-Forward`/`Fan-Reverse` set the direction. Defaults at boot: OFF, 0, OFF, `forward` (RAM only).

After a press: `<prefix>/<device>/state/{novy_light,fan_speed,fan_light,fan_direction}` (retained;
`ON`/`OFF`, `0`–`6`, `forward`/`reverse`), then `<prefix>/<device>/button/<id>/state` `PRESS` and an
empty payload (not retained).

## MQTT and Home Assistant

| Item | Value (identical to 2.3) |
|---|---|
| Broker | from the settings (default from `sdkconfig.secrets`) |
| Client id | `<device>_` + STA MAC in upper-case hex without colons, e.g. `NOVY-RF_AABBCCDDEEFF` |
| Session | MQTT 3.1.1, clean session, keepalive 15 s, QoS 0 for everything |
| Availability | `<prefix>/<device>/status`: will `offline` retained; `online` retained after subscribing |
| Commands | `<prefix>/<device>/button/<id>/set` for the 22 ids (`light`, `power`, `plus`, `minus`, `novy`, `brightness`, `brightness-low`, `fan-all-off`, `fan-off`, `fan-light`, `fan-forward`, `fan-reverse`, `fan-wave`, `fan-speed1`…`fan-speed6`, `fan-timer1h`, `fan-timer2h`, `fan-timer4h`), any payload |
| Discovery | `homeassistant/button/<device>/<id>/config` ×22 and `homeassistant/light/<device>_rgb_light/config`, retained, once per boot, 2 s after the first connect |
| Reconnect | first try as soon as WiFi has an IP, then 1, 2, 4 … 60 s between tries, back to 1 s after a connect |

`<prefix>` defaults to `ESP`, `<device>` to `NOVY-RF` (`CONFIG_NOVY_DEVICE_NAME`; use the sketch's
`HOSTNAME` to keep an existing installation). The discovery payloads are byte-equal to the sketch's
ArduinoJson output, including the device block (`hw_version` 1, `identifiers` `["<device>"]`, name
`<device> NovyFanController`, `model_id` `NOVYFANRF2.3`, `sw_version` `2.3`, `via_device` `""`), the
light's `object_id` `led_<device in lower case>` and its `origin`. With the same device name the
unique ids, and so the Home Assistant entities and anything built on them, stay.

### RGB LED light entity

The 2.3 discovery swaps the light's state and command topics for brightness and RGB, and 2.3 never
handles light commands (the entity stays `unknown` in Home Assistant). The discovery stays as it is;
the firmware serves it the way Home Assistant uses it:

| Home Assistant role | Topic | Payload |
|---|---|---|
| command | `<prefix>/<device>/rgb_light/set` | `ON` / `OFF` |
| brightness command | `<prefix>/<device>/rgb_light/brightness/state` | `0`–`255` |
| RGB command | `<prefix>/<device>/rgb_light/rgb/state` | `r,g,b` |
| state, brightness state, RGB state | `…/rgb_light/status`, `…/rgb_light/brightness/set`, `…/rgb_light/rgb/set` | `{"state":"ON","brightness":255,"rgb":[255,255,255]}`, retained |

The light state lives in RAM (OFF at boot) and is published on every connect and change. The LED
shows it only while no status colour applies.

## Status LED

WS2812 through `espressif/led_strip` (RMT backend; the C3 has two RMT TX channels: one for RF, one
for the LED). Colours are the 2.3 values after Adafruit_NeoPixel brightness scaling.

| Priority | Condition | Colour |
|---|---|---|
| 1 | OTA upload running | blue (0,0,80), alternating with (0,0,20) per received block |
| 2 | OTA finished / failed | green (0,80,0) 500 ms / red (80,0,0) 3 s |
| 3 | button press or `POST /flashRed` | red (255,0,0) for 100 ms |
| 4 | no IP for 80 s ("WiFi failed", at boot or later) | red (80,0,0) |
| 5 | boot, waiting for the first IP | blue (0,0,80) |
| 6 | first IP after boot | green (0,80,0) for 1 s |
| 7 | MQTT enabled and not connected | yellow (40,28,0) |
| 8 | Home Assistant light ON | its colour × brightness |
| 9 | otherwise | off |

## Network

| Field | Default | Notes |
|---|---|---|
| `wifi_ssid`, `wifi_pass` | from `sdkconfig.secrets` | WPA2 minimum |
| `dhcp` | on | a DHCP reservation gives a fixed address |
| `ip`, `netmask`, `gateway`, `dns1`, `dns2` | 192.168.1.50, 255.255.255.0, 192.168.1.1, empty, empty | static mode only; `CONFIG_NOVY_STATIC_*` |
| `hostname` | the device name | DHCP option 12 and mDNS `<hostname>.local` |
| `tx_power_dbm` | 20 | 2.3 never set it, so it ran at the 20 dBm default |
| `ntp1`, `ntp2`, `tz` | `pool.ntp.org`, empty, `CET-1CEST,M3.5.0,M10.5.0/3` | log and status times |

- All-channel scan, join the strongest AP, WiFi power save off, reconnect every 5 s (every 30 s
  while the setup AP runs), forever; no restart on WiFi loss.
- Safe apply: new network settings are tried after a reboot; without an IP within 120 s the previous
  settings return.
- Setup AP `<device>-setup` (WPA2, password from `sdkconfig.secrets`, both settable on the Network
  tab) after 120 s without an IP, or 10 min on request; web UI on 192.168.4.1.
- mDNS hostname and `_http._tcp` on port 80.

## Web interface

### Endpoints kept from 2.3 (method, status, content type and body identical)

| Method | Path | Response |
|---|---|---|
| GET | `/` | page (new dashboard) |
| POST | `/toggleLight`, `/togglePower`, `/togglePlus`, `/toggleMinus`, `/toggleNovy`, `/toggleBrightness`, `/toggleBrightnesslow` | 200 `text/plain` `Light toggled`, `Power toggled`, `Plus toggled`, `Minus toggled`, `Novy toggled`, `Brightness toggled`, `Brightness low toggled` |
| POST | `/fan_all_off`, `/fan_off`, `/fan_light`, `/fan_forward`, `/fan_reverse`, `/fan_wave`, `/fan_speed1`…`/fan_speed6`, `/fan_timer1h`, `/fan_timer2h`, `/fan_timer4h` | 200 `text/plain` `Fan All Off`, `Fan Off`, `Fan Light`, `Fan Forward`, `Fan Reverse`, `Fan Wave`, `Fan Speed 1`…`Fan Speed 6`, `Fan Timer 1H`, `Fan Timer 2H`, `Fan Timer 4H` |
| POST | `/flashRed` | 200 `text/plain` `Flashing red` |
| POST | `/reset` | 200 `text/plain` `Resetting device...`, restart after 0.5 s |
| GET | `/log` | 200 `text/plain`, last 50 event lines `<uptime s>s: <message>` |
| POST | `/log/clear` | 200 `text/plain` `OK` |
| GET | `/heap` | `{"current":…,"initial":…,"minimum":…,"percentage":…,"color":"#30d158"}` |
| GET | `/rssi` | `{"rssi":…,"percentage":…,"color":…,"bars":…}` (0 dBm when not connected, as 2.3) |
| GET | `/mqtt-status` | `{"connected":true}` |
| GET | `/favicon.ico` | 204, empty |
| any | unknown path or wrong method | 404 `text/plain` `Not found` |

The response is sent first; the command is queued (2.3 sent the response, then transmitted).

### Added endpoints

`GET /config` (settings page), `GET /api/status`, `GET /api/log` (console ring), `GET/POST /api/config`
(time, MQTT, setup AP), `POST /api/network` (pending, reboot, safe apply), `POST /api/setup-ap`,
`POST /api/reboot`, `POST /api/ota` (`?force=1` accepts an image of another project, e.g. the
Arduino firmware), `GET /peers.js` and `GET/POST /api/peers` (peer list, see Pages). No
authentication: trusted network only, as 2.3. Passwords are never returned.

### Pages

- `/`: NOVY hood buttons, ceiling fan buttons (Wave hidden, as 2.3), tracked state, device card
  (firmware, slot, rollback target, uptime, WiFi, IP, MQTT, heap, time), event log with auto-scroll
  and clear.
- `/config`: Network (WiFi and IP, setup AP, time), MQTT, RF (code source, channel, repeat state,
  fan cycle indexes, counters), Peers, Serial Console, Firmware OTA (with slot and boot-guard state).
- Peer menu: `<nav id="peers">` in the navbar holds one `<select id="peer-select">` with an option
  per entry of the peer list (`Label=URL;...`, max 12 entries / 512 chars, rules in `peers.h`), in
  list order, labels in capitals. The entry whose label equals `CONFIG_NOVY_DEVICE_NAME` (ignoring
  case) is the selected option, so one list fits every device; without one, a disabled "Peers"
  placeholder is selected. Choosing another entry opens its URL in the same tab. Hidden while the
  list holds no other device. Options are built with `createElement`/`textContent`/`value` only.
  Settings > Peers edits the list and applies it without a reboot. The list is a separate NVS string
  key, so the settings blob and `CFG_VERSION` are unchanged.

## Logging

- `/log`: the 2.3 event log (`addToLogBuffer`), same format and message texts (`RF sent: …`,
  `Fan RF sent: …`, `HA (MQTT) message received [...] ...`, `Connected to MQTT`, `Heap - Current: …`
  every 5 min, `WiFi lost`, `WiFi reconnected: …`), 50 lines, cleared by `POST /log/clear`. Every
  event line also goes to the ESP-IDF log.
- `/api/log`: 16 KB ring of the full ESP-IDF log (Serial Console tab).
- Telnet (port 23) is dropped: `/log` and `/api/log` replace it.

## Flash layout and migration

`partitions.csv` equals the Arduino-ESP32 default table (`CONFIG_PARTITION_TABLE_OFFSET=0x8000`,
4 MB):

| Name | Type | SubType | Offset | Size |
|---|---|---|---|---|
| nvs | data | nvs | 0x9000 | 0x5000 |
| otadata | data | ota | 0xe000 | 0x2000 |
| app0 | app | ota_0 | 0x10000 | 0x140000 |
| app1 | app | ota_1 | 0x150000 | 0x140000 |
| spiffs | data | spiffs | 0x290000 | 0x160000 |
| coredump | data | coredump | 0x3F0000 | 0x10000 |

- The app image must stay below 0x140000 with at least 10 % headroom (≤ 1,179,648 bytes): `-Os`,
  no WPA2-Enterprise, OWE or SoftAP-SAE, no MQTT over TLS/WebSocket. WiFi credentials are never
  stored in NVS (`CONFIG_ESP_WIFI_NVS_ENABLED=n`). `spiffs` and `coredump` stay unused.
- NVS is never erased; the Arduino leftovers stay. Settings live in namespace `novyrf`.
- First install over the air from 2.3: `python3 espota.py -i 192.168.1.50 -p 3232 -a <OTA password>
  -f <app.bin>` (`espota.py` of the Arduino-ESP32 core). ArduinoOTA writes the other slot and selects
  it; the Arduino image stays in the running slot as the rollback target.

## App validity, boot guard and rollback

The Arduino-ESP32 3.x bootloader is built with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, and
ArduinoOTA marks a new image `NEW`, so such a bootloader rolls back a first boot that resets before
the app marks itself valid. Older bootloaders may lack rollback, so the app has its own guard that
works with any bootloader:

1. First thing in `app_main`, before any other init: `nvs_flash_init()` (never erased; on error the
   guard is off and the app is still marked valid after 60 s with WiFi). The NVS record `bootguard`
   holds one entry per OTA slot: {slot address, first 8 bytes of the image's ELF SHA-256, fast-boot
   count, proven, failed}. A different image in a slot starts a fresh entry.
2. The fast-boot count of the running image is incremented and stored. At 3 the other OTA slot is
   probed (`esp_ota_get_next_update_partition()`, `esp_ota_get_partition_description()`, otadata
   state not `INVALID`/`ABORTED`, `esp_image_verify()`), and it must not be an image this guard left
   after a crash loop (same slot and SHA marked failed). When it is usable: count cleared, running
   image marked failed, boot partition set to the other slot, restart.
3. After 60 s of uptime the count is cleared (a "slow" boot). A planned restart (reboot button,
   `/reset`, network save, OTA) clears it too.
4. The image is **confirmed** at the first moment with 60 s of uptime and a WiFi IP, or earlier when
   the web UI restarts it, saves network settings or uploads firmware (the device is evidently
   reachable): `esp_ota_mark_app_valid_cancel_rollback()` runs (every boot, so a re-flashed identical
   image is confirmed for the bootloader again) and the entry is marked proven. The OTA upload has
   to confirm first anyway: `esp_ota_begin()` refuses while the running image is `PENDING_VERIFY`.
5. An image that was never proven and has no IP at 300 s switches to the other usable slot (the
   previous image is known to connect), checked once a minute from then on. It is not marked failed.
6. Manual recovery without a cable: power-cycle three times, each within 60 s after power-on; the
   third boot switches to the other slot.

The decisions are pure functions in `boot_guard.c` with host tests; `boot_guard_idf.c` runs under
QEMU (`tests/qemu`, see Testing).

## OTA afterwards

`POST /api/ota` (body = app image) with a project-name check; `tools/ota_upload.py <host> <image>`
uploads and waits until the device reports the other slot. ArduinoOTA (port 3232, `_arduino._tcp`)
is gone after the migration.

## Behavioural differences to 2.3

| Area | 2.3 | This firmware |
|---|---|---|
| State topics | only web presses update and publish them | web and MQTT presses both do |
| Light entity | commands ignored, state never published (`unknown`) | ON/OFF, brightness and RGB work; state published |
| Light subscriptions | `rgb_light/set`, `rgb_light/rgb/set`, `rgb_light/brightness/set` | `rgb_light/set`, `rgb_light/rgb/state`, `rgb_light/brightness/state` (the topics HA publishes to) |
| LED after a flash | off, even while MQTT is down | back to the status colour |
| LED at boot | blue until setup ends, green 1 s, then off until MQTT state changes | blue until the first IP, green 1 s, then status (yellow while MQTT is down) |
| WiFi loss | retry every 30 s, restart after 5 min | retry every 5 s forever, setup AP after 2 min, red LED after 80 s |
| MQTT retries | 3 blocking tries per round, delay doubles per try | one non-blocking try per delay (1, 2, 4 … 60 s) |
| Planned restart | no `offline` before restart | `offline` published (retained) before restart |
| RF timing | bit-banged, blocking, stretched by WiFi interrupts | RMT, exact; commands queued (16), web/MQTT stay responsive |
| Web OTA / ArduinoOTA | ArduinoOTA on 3232 | `/api/ota` and `tools/ota_upload.py` |
| Telnet | port 23 | dropped |
| Settings | compiled in | Network and MQTT tabs, NVS |
| Novy channel | fixed (default `channelIndex` of `sendRFCommand`) | `NOVY_CHANNEL` of the RF codes header |
| `/heap` minimum | lowest sample seen by `loop()` | heap low-water mark (`esp_get_minimum_free_heap_size()`) |
| Queue full | not possible (blocking) | 503 `RF queue full` (web) / dropped and logged (MQTT) |
| `/log` lines | PubSubClient codes (`MQTT failed, rc=-2 Retrying...`), `WiFi failed after 5 attempts`, Telnet line | `MQTT failed, retrying in N s`, `WiFi failed: no IP after 80 s, still trying`, no Telnet line, plus OTA and confirmation lines; RF, MQTT-message, discovery, heap and WiFi lines unchanged |
| mDNS service | `_arduino._tcp` port 3232 (ArduinoOTA) | `_http._tcp` port 80 |
| HTTP connection | `Connection: close` per response | keep-alive (esp_http_server) |

## Testing

Host (`make -C tests/host`, clang; `make -C tests/host local` for the real captures in `.local/`):

- `reference`: every command fresh, a 2000-command sequence (all cycle steps, both repeat states),
  log lines, web-press MQTT messages and MQTT-press waveforms against the verbatim sketch and
  rc-switch code.
- `rcswitch`, `rf_cmd`, `rf_wave`: protocol table, setter side effects, code words, cycle wrap,
  merging, symbol packing and splitting; hand-computed frames for one NOVY and one fan code.
- `discovery`: all 23 payloads byte-equal and JSON-equal to the capture; none of the stale entries.
- `devstate`, `light`, `led_logic`, `status_fmt`, `evlog`, `backoff`, `boot_guard`, `net_config`,
  `config_validate`.
- `peers`: label and URL rules, blanks/newlines/duplicates, 12-entry and 512-character limits,
  error texts that never echo input, serialisation.
- Local mode only: `check-sources` (the fixtures still match the Arduino sources) and `check-public`
  (the tracked public fixtures follow from the real ones).

QEMU (`tests/qemu/run_boot_guard.sh [--long]`, Espressif QEMU 9.2.2 for the C3): `boot_guard.c` and
`boot_guard_idf.c` in a WiFi-less test app on the real partition table, NVS and otadata with the IDF
5.5.3 bootloader: crash loop → switch to app1 on the third boot; empty app1 → no switch; stable boot
→ count cleared and confirmed at 60 s; (`--long`) never an IP → switch at 300 s. QEMU cannot start a
WiFi app on the C3 (ADC2 calibration before `app_main`) nor run an app from app1, so the full
firmware and the app after a switch are checked on the device.

Device, after an update: boot log and slot, LED sequence, WiFi on the strongest AP, `/heap`
`/rssi` `/mqtt-status` `/log`, MQTT client id and topics, discovery unchanged in Home Assistant,
every button from Home Assistant and the web UI, fan cycle stepping, light entity, 60 s confirmation,
OTA round trip.

## Out of scope

- Authentication.
- RF receive / learning.
- Persisting the tracked state or the light state across reboots (2.3 did not either).

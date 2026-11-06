<!-- ESP32 Novy RF Bridge — Copyright (c) 2026 renedis — GPL-3.0 -->
# Reference code for the host tests

- `RCSwitch.cpp`, `RCSwitch.h`: rc-switch 2.6.4 (sui77), copied unchanged from the Arduino library
  the sketch is built with. LGPL-2.1-or-later, see the file headers. Used by the host tests only,
  never linked into the firmware.
- `sketch_ref.inc`: verbatim sections of the Arduino sketch (firmware 2.3 with ceiling fan
  support), written by `tools/extract_ref.py sketch`. This public copy is written from it by
  `tools/make_public_fixtures.py`: only the default `channelIndex` comes from `config_rf.h` (the
  example Novy channel).
- `Arduino.h`: just enough Arduino API (String, pins, delays, interrupts) to compile both on the
  host. `digitalWrite`/`delayMicroseconds` record the waveform.
